const vscode = require("vscode");
const path = require("path");
const crypto = require("crypto");

const FAIRUZ_RTL_VIEW = "fairuz.rtlEditor";

class FairuzRtlEditorProvider {
  constructor(context, highlighter) {
    this.context = context;
    this.highlighter = highlighter;
    this.editors = new Map();
  }

  static register(context, highlighter) {
    const provider = new FairuzRtlEditorProvider(context, highlighter);
    provider.registration = vscode.window.registerCustomEditorProvider(FAIRUZ_RTL_VIEW, provider, {
      webviewOptions: {
        retainContextWhenHidden: true,
        enableFindWidget: true
      },
      supportsMultipleEditorsPerDocument: false
    });
    return provider;
  }

  dispose() {
    this.registration?.dispose();
    for (const editorId of [...this.editors.keys()]) this.cleanupEditor(editorId);
  }

  runHistoryCommand(action) {
    // `active` identifies the focused custom editor. The visible fallback is
    // useful while VS Code is transferring focus from its menu/command UI.
    const state = [...this.editors.values()].find((entry) => entry.webviewPanel.active)
      || [...this.editors.values()].find((entry) => entry.webviewPanel.visible);
    if (!state) return false;
    state.webviewPanel.webview.postMessage({ type: "history", action });
    return true;
  }

  async resolveCustomTextEditor(document, webviewPanel) {
    const editorId = crypto.randomUUID();
    const editorState = {
      document,
      webviewPanel,
      changeSubscription: null,
      semanticCancellation: null,
      editQueue: Promise.resolve(),
      synchronizedDocumentVersion: document.version,
      // True only while we are actively pushing a remote (document -> webview
      // or webview -> document) update through the pipe. This must wrap the
      // FULL round trip for a given direction, not just the inner await, or
      // an interleaved change from a third party (formatter, git, LSP) can
      // slip through the gap and get silently dropped or silently overwrite
      // in-flight user keystrokes.
      applyingDocumentEdit: false
    };
    this.editors.set(editorId, editorState);

    webviewPanel.webview.options = {
      enableScripts: true,
      localResourceRoots: [
        vscode.Uri.joinPath(this.context.extensionUri, "media")
      ]
    };

    webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview);

    editorState.changeSubscription = vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.uri.toString() !== document.uri.toString()) return;
      this.handleDocumentChange(editorId, event);
    });

    webviewPanel.webview.onDidReceiveMessage((message) => {
      const state = this.editors.get(editorId);
      if (!state) return;
      if (message.type === "edit") {
        state.editQueue = state.editQueue
          .then(() => this.handleWebviewMessage(editorId, message))
          .catch((error) => console.error("[fairuz-rtl] error handling edit:", error));
        return;
      }
      this.handleWebviewMessage(editorId, message)
        .catch((error) => console.error("[fairuz-rtl] error handling webview message:", error));
    });

    webviewPanel.onDidDispose(() => this.cleanupEditor(editorId));

    // Wait for the webview editor to mount before pushing initial content.
    const readySub = webviewPanel.webview.onDidReceiveMessage((message) => {
      if (message.type === "ready") {
        this.sendFullSync(editorId);
        readySub.dispose();
      }
    });
  }

  // --- Document -> Webview -----------------------------------------------

  handleDocumentChange(editorId, event) {
    const editorState = this.editors.get(editorId);
    if (!editorState) return;

    // If this change was caused by us applying a webview-originated edit,
    // don't bounce it back -- the webview already has this content locally.
    if (editorState.applyingDocumentEdit) {
      editorState.synchronizedDocumentVersion = event.document.version;
      return;
    }

    // Some VS Code builds deliver the document-change notification after the
    // applyEdit promise settles. In that ordering `applyingDocumentEdit` is
    // already false, but the accepted version is still exactly the version
    // the webview has. Do not turn that acknowledgement into a full reload.
    if (event.document.version === editorState.synchronizedDocumentVersion) return;

    // Any other source of change (external edit, git, formatter, another
    // view of the same doc) invalidates whatever the webview has: resync
    // fully rather than trying to translate VS Code's TextDocumentContentChangeEvent
    // deltas into editor changes, since ordering/version skew between the two
    // models is exactly the bug we're removing.
    this.sendFullSync(editorId);
  }

  sendFullSync(editorId) {
    const editorState = this.editors.get(editorId);
    if (!editorState || !editorState.webviewPanel.visible) return;

    editorState.synchronizedDocumentVersion = editorState.document.version;
    editorState.webviewPanel.webview.postMessage({
      type: "setText",
      text: editorState.document.getText(),
      version: editorState.document.version
    });
  }

  // --- Webview -> Document -------------------------------------------------

  async handleWebviewMessage(editorId, message) {
    const editorState = this.editors.get(editorId);
    if (!editorState) return;

    if (message.type === "semanticTokens") {
      editorState.semanticCancellation?.cancel();
      editorState.semanticCancellation?.dispose();
      const cancellation = new vscode.CancellationTokenSource();
      editorState.semanticCancellation = cancellation;
      const result = await this.highlighter.highlight(message.text, cancellation.token);
      if (editorState.semanticCancellation !== cancellation) {
        cancellation.dispose();
        return;
      }
      editorState.semanticCancellation = null;
      cancellation.dispose();
      editorState.webviewPanel.webview.postMessage({
        type: "semanticTokens",
        requestId: message.requestId,
        tokens: result ? result.tokens : []
      });
      return;
    }

    if (message.type !== "edit") return;

    const { document } = editorState;

    // Reject edits computed against a stale version of the document instead
    // of applying them anyway. This is the core fix for the data-loss race:
    // the webview must tell us which version it was looking at, and if the
    // document has moved on since (external edit landed in between), we
    // discard the edit and force a resync rather than clobbering newer
    // content with an edit based on old content.
    if (message.baseVersion !== document.version) {
      console.warn(
        `[fairuz-rtl] dropping stale edit (base v${message.baseVersion}, doc is v${document.version})`
      );
      this.sendFullSync(editorId);
      return;
    }

    if (!Array.isArray(message.edits) || message.edits.length === 0) return;

    editorState.applyingDocumentEdit = true;
    try {
      const workspaceEdit = new vscode.WorkspaceEdit();
      for (const e of message.edits) {
        if (
          typeof e.startLine !== "number" ||
          typeof e.startColumn !== "number" ||
          typeof e.endLine !== "number" ||
          typeof e.endColumn !== "number" ||
          typeof e.text !== "string"
        ) {
          continue; // ignore malformed edit entries rather than throwing
        }
        const range = new vscode.Range(
          e.startLine - 1,
          e.startColumn - 1,
          e.endLine - 1,
          e.endColumn - 1
        );
        workspaceEdit.replace(document.uri, range, e.text);
      }

      const applied = await vscode.workspace.applyEdit(workspaceEdit);
      if (!applied) {
        console.warn("[fairuz-rtl] applyEdit returned false, resyncing");
        this.sendFullSync(editorId);
      } else {
        editorState.synchronizedDocumentVersion = document.version;
        editorState.webviewPanel.webview.postMessage({
          type: "ack",
          version: document.version
        });
      }
    } finally {
      editorState.applyingDocumentEdit = false;
    }
  }

  cleanupEditor(editorId) {
    const editorState = this.editors.get(editorId);
    if (!editorState) return;
    editorState.semanticCancellation?.cancel();
    editorState.semanticCancellation?.dispose();
    if (editorState.changeSubscription) editorState.changeSubscription.dispose();
    this.editors.delete(editorId);
  }

  getHtmlForWebview(webview) {
    const nonce = getNonce();
    const scriptUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this.context.extensionUri, "media", "editor.js")
    );
    const styleUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this.context.extensionUri, "media", "editor.css")
    );
    return `<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy" content="
    default-src 'none';
    style-src ${webview.cspSource} 'unsafe-inline';
    font-src ${webview.cspSource};
    worker-src ${webview.cspSource} blob:;
    script-src 'nonce-${nonce}' ${webview.cspSource};
  " />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Fairuz RTL Editor</title>
  <link rel="stylesheet" href="${styleUri}" />
</head>
<body>
  <div id="container"></div>
  <script nonce="${nonce}" src="${scriptUri}"></script>
</body>
</html>`;
  }
}

function getNonce() {
  return crypto.randomBytes(16).toString("hex");
}

module.exports = { FairuzRtlEditorProvider, FAIRUZ_RTL_VIEW };
