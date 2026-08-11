const vscode = require("vscode");
const path = require("path");
const crypto = require("crypto");

const FAIRUZ_RTL_VIEW = "fairuz.rtlEditor";

class FairuzRtlEditorProvider {
  constructor(context) {
    this.context = context;
    this.editors = new Map();
  }

  static register(context) {
    const provider = new FairuzRtlEditorProvider(context);
    return vscode.window.registerCustomEditorProvider(FAIRUZ_RTL_VIEW, provider, {
      webviewOptions: {
        retainContextWhenHidden: true,
        enableFindWidget: true
      },
      supportsMultipleEditorsPerDocument: false
    });
  }

  async resolveCustomTextEditor(document, webviewPanel) {
    const editorId = crypto.randomUUID();
    const editorState = {
      document,
      webviewPanel,
      changeSubscription: null,
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
        vscode.Uri.joinPath(this.context.extensionUri, "media"),
        vscode.Uri.joinPath(this.context.extensionUri, "node_modules", "monaco-editor")
      ]
    };

    webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview);

    editorState.changeSubscription = vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.uri.toString() !== document.uri.toString()) return;
      this.handleDocumentChange(editorId, event);
    });

    webviewPanel.webview.onDidReceiveMessage(async (message) => {
      try {
        await this.handleWebviewMessage(editorId, message);
      } catch (error) {
        console.error("[fairuz-rtl] error handling webview message:", error);
      }
    });

    webviewPanel.onDidDispose(() => this.cleanupEditor(editorId));

    // Wait for the webview to signal it's mounted before pushing initial
    // content -- Monaco needs to finish its own async init first.
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
    // don't bounce it back -- Monaco already has this content locally.
    if (editorState.applyingDocumentEdit) return;

    // Any other source of change (external edit, git, formatter, another
    // view of the same doc) invalidates whatever the webview has: resync
    // fully rather than trying to translate VS Code's TextDocumentContentChangeEvent
    // deltas into Monaco edits, since ordering/version skew between the two
    // models is exactly the bug we're removing.
    this.sendFullSync(editorId);
  }

  sendFullSync(editorId) {
    const editorState = this.editors.get(editorId);
    if (!editorState || !editorState.webviewPanel.visible) return;

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
      }
    } finally {
      editorState.applyingDocumentEdit = false;
    }
  }

  cleanupEditor(editorId) {
    const editorState = this.editors.get(editorId);
    if (!editorState) return;
    if (editorState.changeSubscription) editorState.changeSubscription.dispose();
    this.editors.delete(editorId);
  }

  getHtmlForWebview(webview) {
    const nonce = getNonce();
    const scriptUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this.context.extensionUri, "media", "editor.js")
    );
    const monacoBaseUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this.context.extensionUri, "node_modules", "monaco-editor", "min", "vs")
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
  <style>
    html, body, #container { margin: 0; padding: 0; height: 100%; overflow: hidden; }
  </style>
</head>
<body>
  <div id="container"></div>
  <script nonce="${nonce}">
    window.__fairuzMonacoBaseUri = "${monacoBaseUri}";
  </script>
  <script nonce="${nonce}" src="${monacoBaseUri}/loader.js"></script>
  <script nonce="${nonce}" src="${scriptUri}"></script>
</body>
</html>`;
  }
}

function getNonce() {
  return crypto.randomBytes(16).toString("hex");
}

module.exports = { FairuzRtlEditorProvider, FAIRUZ_RTL_VIEW };