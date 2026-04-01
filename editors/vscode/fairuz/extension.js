const vscode = require("vscode");

const FAIRUZ_RTL_VIEW = "fairuz.rtlEditor";

function activate(context) {
  context.subscriptions.push(
    vscode.commands.registerCommand("fairuz.openRtlEditor", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        vscode.window.showInformationMessage("No active editor to reopen.");
        return;
      }

      await vscode.commands.executeCommand(
        "vscode.openWith",
        editor.document.uri,
        FAIRUZ_RTL_VIEW
      );
    })
  );

  context.subscriptions.push(
    vscode.window.registerCustomEditorProvider(
      FAIRUZ_RTL_VIEW,
      new FairuzRtlEditorProvider(context),
      {
        webviewOptions: {
          retainContextWhenHidden: true
        },
        supportsMultipleEditorsPerDocument: false
      }
    )
  );
}

class FairuzRtlEditorProvider {
  constructor(context) {
    this.context = context;
  }

  async resolveCustomTextEditor(document, webviewPanel) {
    webviewPanel.webview.options = {
      enableScripts: true
    };

    const updateWebview = () => {
      webviewPanel.webview.postMessage({
        type: "setText",
        text: document.getText()
      });
    };

    webviewPanel.webview.html = this.getHtmlForWebview(webviewPanel.webview, document.getText());

    const changeDocumentSubscription = vscode.workspace.onDidChangeTextDocument(
      (event) => {
        if (event.document.uri.toString() === document.uri.toString()) {
          updateWebview();
        }
      }
    );

    webviewPanel.onDidDispose(() => {
      changeDocumentSubscription.dispose();
    });

    webviewPanel.webview.onDidReceiveMessage(async (message) => {
      if (message.type !== "edit") {
        return;
      }

      const edit = new vscode.WorkspaceEdit();
      const fullRange = new vscode.Range(
        document.positionAt(0),
        document.positionAt(document.getText().length)
      );
      edit.replace(document.uri, fullRange, message.text);
      await vscode.workspace.applyEdit(edit);
    });

    updateWebview();
  }

  getHtmlForWebview(_webview, initialText) {
    const nonce = getNonce();
    const initialTextJson = JSON.stringify(initialText);
    return `<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
  <meta charset="UTF-8" />
  <meta
    http-equiv="Content-Security-Policy"
    content="default-src 'none'; style-src 'unsafe-inline'; script-src 'nonce-${nonce}';"
  />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Fairuz RTL Editor</title>
  <style>
    :root {
      color-scheme: dark light;
      --bg: #11161c;
      --panel: #171d24;
      --fg: #e7edf3;
      --muted: #8a97a6;
      --accent: #2a3440;
    }

    html,
    body {
      margin: 0;
      height: 100%;
      background: var(--bg);
      color: var(--fg);
      font-family: "IBM Plex Sans Arabic", "Noto Sans Arabic", "SF Mono", Monaco, Consolas, monospace;
    }

    .root {
      display: grid;
      grid-template-rows: auto 1fr;
      height: 100%;
    }

    .toolbar {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      padding: 10px 14px;
      background: var(--panel);
      border-bottom: 1px solid var(--accent);
      font-size: 12px;
    }

    .hint {
      color: var(--muted);
    }

    .editor-shell {
      min-height: 0;
    }

    #input {
      resize: none;
      border: 0;
      outline: none;
      width: 100%;
      height: 100%;
      padding: 18px 20px;
      box-sizing: border-box;
      background:
        linear-gradient(180deg, rgba(255,255,255,0.02), rgba(255,255,255,0)) no-repeat,
        var(--bg);
      color: var(--fg);
      caret-color: var(--fg);
      overflow: auto;
      font-family: "IBM Plex Sans Arabic", "Noto Sans Arabic", "SF Mono", Monaco, Consolas, monospace;
      font-size: 15px;
      line-height: 1.75;
      white-space: pre;
      tab-size: 4;
      direction: rtl;
      unicode-bidi: plaintext;
      text-align: right;
    }

    #input::selection {
      background: rgba(124, 199, 255, 0.35);
    }
  </style>
</head>
<body>
  <div class="root">
    <div class="toolbar">
      <div>Fairuz RTL Editor</div>
      <div class="hint">RTL layout, right-aligned flow, live file sync</div>
    </div>
    <div class="editor-shell">
      <textarea id="input" spellcheck="false"></textarea>
    </div>
  </div>

  <script nonce="${nonce}">
    const vscode = acquireVsCodeApi();
    const input = document.getElementById("input");
    const initialText = ${initialTextJson};

    let applyingRemoteUpdate = false;
    let pendingTimer = null;

    function queueEdit() {
      if (applyingRemoteUpdate) {
        return;
      }

      clearTimeout(pendingTimer);
      pendingTimer = setTimeout(() => {
        vscode.postMessage({
          type: "edit",
          text: input.value
        });
      }, 120);
    }

    input.addEventListener("input", queueEdit);

    input.value = initialText;

    window.addEventListener("message", (event) => {
      const message = event.data;
      if (message.type !== "setText") {
        return;
      }

      if (message.text === input.value) {
        return;
      }

      applyingRemoteUpdate = true;
      const selectionStart = input.selectionStart;
      const selectionEnd = input.selectionEnd;
      input.value = message.text;
      input.selectionStart = Math.min(selectionStart, input.value.length);
      input.selectionEnd = Math.min(selectionEnd, input.value.length);
      applyingRemoteUpdate = false;
    });
  </script>
</body>
</html>`;
  }
}

function getNonce() {
  const chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < 32; i += 1) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
