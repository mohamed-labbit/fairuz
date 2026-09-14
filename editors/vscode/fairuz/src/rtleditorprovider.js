const vscode = require("vscode");
const path = require("path");
const crypto = require("crypto");
const { normalize } = require("./documentSync");
const { checkSource, formatSource } = require("./compilerService");
const FAIRUZ_RTL_VIEW = "fairuz.rtlEditor";
const ACTIONS = new Set(["save", "run", "stop", "format", "find", "replace", "undo", "redo", "check", "wrap", "outline"]);

class FairuzRtlEditorProvider {
  constructor(context, highlighter) {
    this.context = context;
    this.highlighter = highlighter;
    this.editors = new Map();
    this.diagnostics = vscode.languages.createDiagnosticCollection("fairuz");
    this.taskSubscription = vscode.tasks.onDidEndTask(event => {
      for (const state of this.editors.values()) {
        if (state.execution === event.execution) {
          state.execution = null;
          this.post(state, { type: "runState", running: false });
        }
      }
    });
  }
  static register(context, highlighter) {
    const provider = new FairuzRtlEditorProvider(context, highlighter);
    provider.registration = vscode.window.registerCustomEditorProvider(FAIRUZ_RTL_VIEW, provider, {
      webviewOptions: { retainContextWhenHidden: true }, supportsMultipleEditorsPerDocument: false
    });
    return provider;
  }
  dispose() {
    this.registration?.dispose();
    this.taskSubscription.dispose();
    for (const id of [...this.editors.keys()]) this.cleanupEditor(id);
    this.diagnostics.dispose();
  }
  runCommand(action) {
    const state = [...this.editors.values()].find(entry => entry.webviewPanel.active);
    if (!state || !ACTIONS.has(action)) return false;
    this.post(state, { type: "command", action });
    return true;
  }
  post(state, message) {
    if (!state.disposed) return state.webviewPanel.webview.postMessage(message);
  }
  async resolveCustomTextEditor(document, webviewPanel) {
    const id = crypto.randomUUID();
    const state = { document, webviewPanel, subscriptions: [], queue: Promise.resolve(),
      expectedText: null, lastSentVersion: null, ready: false, disposed: false };
    this.editors.set(id, state);
    webviewPanel.webview.options = {
      enableScripts: true, localResourceRoots: [vscode.Uri.joinPath(this.context.extensionUri, "media")]
    };
    state.subscriptions.push(
      vscode.workspace.onDidChangeTextDocument(event => {
        if (event.document.uri.toString() !== document.uri.toString()) return;
        this.diagnostics.delete(document.uri);
        if (normalize(document.getText()) !== state.checkText) state.checkCancellation?.cancel();
        if (normalize(document.getText()) !== state.expectedText && document.version !== state.lastSentVersion)
          this.sendFullSync(state);
        this.sendMetadata(state);
      }),
      vscode.workspace.onDidSaveTextDocument(saved => {
        if (saved.uri.toString() === document.uri.toString()) this.sendMetadata(state);
      }),
      vscode.workspace.onDidChangeConfiguration(event => {
        if (event.affectsConfiguration("fairuz") || event.affectsConfiguration("editor")) this.sendMetadata(state);
      }),
      vscode.workspace.onDidGrantWorkspaceTrust(() => this.sendMetadata(state)),
      webviewPanel.onDidChangeViewState(() => {
        if (state.ready && webviewPanel.visible) { this.sendFullSync(state); this.sendMetadata(state); }
      }),
      webviewPanel.webview.onDidReceiveMessage(message => {
        if (!message || typeof message.type !== "string") return;
        if (["edit", "action", "sync"].includes(message.type)) {
          state.queue = state.queue.then(() => this.handleMessage(state, message)).catch(error => {
            this.post(state, { type: "notice", message: error.message, error: true });
            this.sendFullSync(state, true);
          });
        } else this.handleMessage(state, message).catch(error =>
          this.post(state, { type: "notice", message: error.message, error: true }));
      }),
      webviewPanel.onDidDispose(() => this.cleanupEditor(id))
    );
    webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview);
  }
  sendMetadata(state) {
    if (!state.ready) return;
    const config = vscode.workspace.getConfiguration("fairuz", state.document.uri);
    const editor = vscode.workspace.getConfiguration("editor", state.document.uri);
    this.post(state, { type: "metadata", name: path.basename(state.document.uri.fsPath),
      dirty: state.document.isDirty, trusted: vscode.workspace.isTrusted,
      settings: {
        fontSize: config.get("fontSize", editor.get("fontSize", 15)), lineHeight: config.get("lineHeight", 1.8),
        wordWrap: config.get("wordWrap", true), tabSize: config.get("tabSize", 4),
        liveDiagnostics: config.get("liveDiagnostics", true)
      }
    });
  }
  sendFullSync(state, rejected = false, authoritative = false, recoveryId = null) {
    if (!state.ready) return;
    state.lastSentVersion = state.document.version;
    this.post(state, { type: "setText", text: normalize(state.document.getText()), version: state.document.version, rejected, authoritative, recoveryId });
  }
  async handleMessage(state, message) {
    if (state.disposed) return;
    if (message.type === "ready") {
      state.ready = true; this.sendFullSync(state); this.sendMetadata(state); return;
    }
    if (message.type === "sync") {
      this.sendFullSync(state, false, true, message.recoveryId ?? null);
      this.sendMetadata(state);
      return;
    }
    if (message.type === "semanticTokens" || message.type === "check" || message.type === "format") {
      if (typeof message.text !== "string" || !Number.isSafeInteger(message.requestId)) return;
      const field = message.type === "format" ? "formatCancellation"
        : message.type === "check" ? "checkCancellation" : "semanticCancellation";
      state[field]?.cancel(); state[field]?.dispose();
      const cancellation = new vscode.CancellationTokenSource();
      state[field] = cancellation;
      try {
        if (message.type === "format") {
          const result = vscode.workspace.isTrusted
            ? await formatSource(this.highlighter.executable(), message.text, { token: cancellation.token })
            : { error: "Trust this workspace to enable Fairuz formatting." };
          if (state[field] !== cancellation || cancellation.token.isCancellationRequested || state.disposed) return;
          this.post(state, { type: "formatted", requestId: message.requestId, ...result });
        } else if (message.type === "check") {
          state.checkText = message.text;
          const result = vscode.workspace.isTrusted
            ? await checkSource(this.highlighter.executable(), message.text, {
              token: cancellation.token,
              cwd: state.document.uri.scheme === "file" ? path.dirname(state.document.uri.fsPath) : undefined
            }) : { diagnostics: [], error: "Trust this workspace to enable compiler checking." };
          if (state[field] !== cancellation || cancellation.token.isCancellationRequested || state.disposed) return;
          if (normalize(state.document.getText()) === message.text) {
            this.diagnostics.set(state.document.uri, (result.diagnostics || []).map(item => {
              const diagnostic = new vscode.Diagnostic(
                new vscode.Range(item.line, item.start, item.line, item.start + item.length), item.message,
                item.severity === "warning" ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error);
              diagnostic.source = "Fairuz";
              return diagnostic;
            }));
          }
          this.post(state, { type: "diagnostics", requestId: message.requestId, ...result });
        } else {
          const result = await this.highlighter.highlight(message.text, cancellation.token);
          if (state[field] !== cancellation || state.disposed) return;
          this.post(state, { type: "semanticTokens", requestId: message.requestId, tokens: result?.tokens || [] });
        }
      } finally {
        if (state[field] === cancellation) state[field] = null;
        cancellation.dispose();
      }
      return;
    }
    if (message.type === "action") {
      if (!ACTIONS.has(message.action)) return;
      if (message.action === "stop") { state.execution?.terminate(); return; }
      if (message.version !== state.document.version) { this.sendFullSync(state, true); return; }
      if (message.action === "save") {
        const saved = await state.document.save();
        this.post(state, { type: "notice", message: saved ? "تم الحفظ" : "لم يُحفظ الملف", error: !saved });
        this.sendMetadata(state);
      }
      if (message.action === "run") await this.runDocument(state);
      return;
    }
    if (message.type !== "edit") return;
    const { document } = state, change = message.change;
    const before = normalize(document.getText());
    if (message.baseVersion !== document.version || !Number.isSafeInteger(message.id)
        || !change || !Number.isSafeInteger(change.from) || !Number.isSafeInteger(change.to)
        || change.from < 0 || change.to < change.from || change.to > before.length || typeof change.insert !== "string") {
      this.sendFullSync(state, true); return;
    }
    // CodeMirror offsets use LF even when the TextDocument uses CRLF.
    const position = offset => {
      const prefix = before.slice(0, offset), line = prefix.split("\n").length - 1;
      return new vscode.Position(line, offset - prefix.lastIndexOf("\n") - 1);
    };
    const expected = before.slice(0, change.from) + change.insert + before.slice(change.to);
    const edit = new vscode.WorkspaceEdit();
    edit.replace(document.uri, new vscode.Range(position(change.from), position(change.to)),
      document.eol === vscode.EndOfLine.CRLF ? change.insert.replace(/\n/g, "\r\n") : change.insert);
    state.expectedText = expected;
    try {
      const applied = await vscode.workspace.applyEdit(edit);
      if (state.disposed) return;
      if (!applied || normalize(document.getText()) !== expected) this.sendFullSync(state, true);
      else {
        state.lastSentVersion = document.version;
        this.post(state, { type: "ack", id: message.id, version: document.version });
      }
    } finally { state.expectedText = null; }
  }
  async runDocument(state) {
    if (!vscode.workspace.isTrusted) throw new Error("Trust this workspace before running Fairuz programs.");
    if (state.execution || state.startingRun) return;
    state.startingRun = true;
    try {
      if (!await state.document.save()) return;
      if (state.document.uri.scheme !== "file") throw new Error("Save this program to a local .fa file before running it.");
      const task = new vscode.Task(
        { type: "fairuz", file: state.document.uri.toString() },
        vscode.workspace.getWorkspaceFolder(state.document.uri) || vscode.TaskScope.Global,
        `Fairuz: ${path.basename(state.document.uri.fsPath)}`, "Fairuz",
        new vscode.ProcessExecution(this.highlighter.executable(), [state.document.uri.fsPath], {
          cwd: path.dirname(state.document.uri.fsPath)
        })
      );
      task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, panel: vscode.TaskPanelKind.Dedicated, clear: true, focus: true };
      state.execution = await vscode.tasks.executeTask(task);
      if (state.disposed) state.execution.terminate();
      else this.post(state, { type: "runState", running: true });
    } finally { state.startingRun = false; }
  }
  cleanupEditor(id) {
    const state = this.editors.get(id);
    if (!state) return;
    state.disposed = true;
    for (const key of ["semanticCancellation", "checkCancellation", "formatCancellation"]) { state[key]?.cancel(); state[key]?.dispose(); }
    for (const subscription of state.subscriptions) subscription.dispose();
    this.diagnostics.delete(state.document.uri);
    this.editors.delete(id);
  }
  getHtmlForWebview(webview) {
    const nonce = crypto.randomBytes(16).toString("hex");
    const resource = file => webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, "media", file));
    return `<!DOCTYPE html>
<html lang="ar" dir="rtl"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource} 'unsafe-inline'; font-src ${webview.cspSource}; script-src 'nonce-${nonce}';">
<title>Fairuz RTL Editor</title><link rel="stylesheet" href="${resource("editor.css")}">
</head><body><div id="app"></div><script nonce="${nonce}" src="${resource("editor.js")}"></script></body></html>`;
  }
}
module.exports = { FairuzRtlEditorProvider, FAIRUZ_RTL_VIEW };
