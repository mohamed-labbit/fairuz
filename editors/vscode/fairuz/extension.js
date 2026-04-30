const vscode = require("vscode");

const FAIRUZ_RTL_VIEW = "fairuz.rtlEditor";
const EDIT_DEBOUNCE_MS = 120;
const MAX_UNDO_STACK = 100;

function activate(context) {
  try {
    context.subscriptions.push(
      vscode.commands.registerCommand("fairuz.openRtlEditor", async () => {
        try {
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
        } catch (error) {
          vscode.window.showErrorMessage(
            `Failed to open RTL editor: ${error.message}`
          );
          console.error("Error opening RTL editor:", error);
        }
      })
    );

    const provider = new FairuzRtlEditorProvider(context);
    context.subscriptions.push(
      vscode.window.registerCustomEditorProvider(
        FAIRUZ_RTL_VIEW,
        provider,
        {
          webviewOptions: {
            retainContextWhenHidden: true,
            enableFindWidget: true
          },
          supportsMultipleEditorsPerDocument: false
        }
      )
    );
  } catch (error) {
    vscode.window.showErrorMessage(
      `Failed to activate Fairuz RTL Editor: ${error.message}`
    );
    console.error("Activation error:", error);
  }
}

class FairuzRtlEditorProvider {
  constructor(context) {
    this.context = context;
    this.editors = new Map(); // Track active editors
  }

  async resolveCustomTextEditor(document, webviewPanel) {
    try {
      const editorId = `${document.uri.toString()}_${Date.now()}`;
      const editorState = {
        document,
        webviewPanel,
        changeSubscription: null,
        isApplyingRemoteUpdate: false,
        pendingEdits: [],
        lastSyncedVersion: document.version,
        syncInProgress: false
      };

      this.editors.set(editorId, editorState);

      webviewPanel.webview.options = {
        enableScripts: true,
        localResourceRoots: [this.context.extensionUri]
      };

      // Initialize webview
      webviewPanel.webview.html = this.getHtmlForWebview(
        webviewPanel.webview,
        document.getText()
      );

      // Handle document changes from VS Code
      editorState.changeSubscription = vscode.workspace.onDidChangeTextDocument(
        (event) => {
          if (event.document.uri.toString() === document.uri.toString()) {
            this.handleDocumentChange(editorId, event);
          }
        }
      );

      // Handle webview messages (edits from RTL editor)
      webviewPanel.webview.onDidReceiveMessage(
        async (message) => {
          try {
            await this.handleWebviewMessage(editorId, message);
          } catch (error) {
            console.error("Error handling webview message:", error);
            vscode.window.showErrorMessage(
              `Sync error: ${error.message}`
            );
          }
        }
      );

      // Cleanup on close
      webviewPanel.onDidDispose(() => {
        this.cleanupEditor(editorId);
      });

      // Send initial content
      this.syncToWebview(editorId);
    } catch (error) {
      vscode.window.showErrorMessage(
        `Failed to initialize editor: ${error.message}`
      );
      console.error("Editor initialization error:", error);
    }
  }

  async handleDocumentChange(editorId, event) {
    const editorState = this.editors.get(editorId);
    if (!editorState) return;

    // Prevent feedback loop when applying remote updates
    if (editorState.isApplyingRemoteUpdate) {
      return;
    }

    editorState.lastSyncedVersion = event.document.version;
    this.syncToWebview(editorId);
  }

  syncToWebview(editorId) {
    const editorState = this.editors.get(editorId);
    if (!editorState || !editorState.webviewPanel.visible) return;

    try {
      const text = editorState.document.getText();
      editorState.webviewPanel.webview.postMessage({
        type: "setText",
        text,
        version: editorState.document.version
      });
    } catch (error) {
      console.error("Sync to webview failed:", error);
    }
  }

  async handleWebviewMessage(editorId, message) {
    const editorState = this.editors.get(editorId);
    if (!editorState) return;

    if (message.type !== "edit") {
      return;
    }

    // Validate message
    if (typeof message.text !== "string") {
      console.warn("Invalid edit message: text is not a string");
      return;
    }

    const { document } = editorState;
    const currentText = document.getText();

    // Avoid redundant edits
    if (message.text === currentText) {
      return;
    }

    editorState.isApplyingRemoteUpdate = true;
    try {
      const edit = new vscode.WorkspaceEdit();
      const fullRange = new vscode.Range(
        document.positionAt(0),
        document.positionAt(currentText.length)
      );
      edit.replace(document.uri, fullRange, message.text);
      
      const success = await vscode.workspace.applyEdit(edit);
      if (!success) {
        console.warn("Failed to apply workspace edit");
      }
    } catch (error) {
      console.error("Error applying edit:", error);
      // Resync webview to recover from error
      this.syncToWebview(editorId);
    } finally {
      editorState.isApplyingRemoteUpdate = false;
    }
  }

  cleanupEditor(editorId) {
    const editorState = this.editors.get(editorId);
    if (editorState) {
      if (editorState.changeSubscription) {
        editorState.changeSubscription.dispose();
      }
      this.editors.delete(editorId);
    }
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
      --success: #31c754;
      --error: #ff453a;
    }

    * {
      box-sizing: border-box;
    }

    html,
    body {
      margin: 0;
      padding: 0;
      height: 100%;
      background: var(--bg);
      color: var(--fg);
      font-family: "IBM Plex Sans Arabic", "Noto Sans Arabic", "SF Mono", Monaco, Consolas, monospace;
    }

    .root {
      display: grid;
      grid-template-rows: auto 1fr auto;
      height: 100%;
      overflow: hidden;
    }

    .toolbar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      padding: 10px 14px;
      background: var(--panel);
      border-bottom: 1px solid var(--accent);
      font-size: 12px;
      flex-wrap: wrap;
    }

    .toolbar-left {
      display: flex;
      gap: 12px;
      align-items: center;
    }

    .toolbar-right {
      display: flex;
      gap: 8px;
      align-items: center;
    }

    .hint {
      color: var(--muted);
      font-size: 11px;
    }

    .status-indicator {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--success);
      animation: pulse 2s infinite;
    }

    .status-indicator.syncing {
      background: var(--accent);
      animation: none;
    }

    .status-indicator.error {
      background: var(--error);
      animation: none;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }

    .editor-shell {
      min-height: 0;
      overflow: hidden;
      flex: 1;
    }

    #input {
      resize: none;
      border: 0;
      outline: none;
      width: 100%;
      height: 100%;
      padding: 18px 20px;
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

    .status-bar {
      display: flex;
      justify-content: space-between;
      padding: 6px 14px;
      background: var(--panel);
      border-top: 1px solid var(--accent);
      font-size: 11px;
      color: var(--muted);
    }

    .status-item {
      display: flex;
      gap: 6px;
    }
  </style>
</head>
<body>
  <div class="root">
    <div class="toolbar">
      <div class="toolbar-left">
        <span>Fairuz RTL Editor</span>
        <span class="status-indicator" id="syncStatus"></span>
      </div>
      <div class="toolbar-right">
        <span class="hint" id="statusText">Ready</span>
      </div>
    </div>
    
    <div class="editor-shell">
      <textarea
        id="input"
        spellcheck="false"
        autocomplete="off"
        autocorrect="off"
        autocapitalize="off"
      ></textarea>
    </div>

    <div class="status-bar">
      <div class="status-item">
        <span>Line <span id="lineNum">1</span>, Column <span id="colNum">1</span></span>
      </div>
      <div class="status-item">
        <span id="charCount">0</span> characters
      </div>
    </div>
  </div>

  <script nonce="${nonce}">
    (function() {
      'use strict';

      const vscode = acquireVsCodeApi();
      const input = document.getElementById("input");
      const syncStatus = document.getElementById("syncStatus");
      const statusText = document.getElementById("statusText");
      const lineNum = document.getElementById("lineNum");
      const colNum = document.getElementById("colNum");
      const charCount = document.getElementById("charCount");

      const initialText = ${initialTextJson};
      const DEBOUNCE_MS = 120;

      let applyingRemoteUpdate = false;
      let pendingTimer = null;
      let lastMessageTime = 0;

      function setSyncStatus(state) {
        syncStatus.className = "status-indicator " + (state === "syncing" ? "syncing" : state === "error" ? "error" : "");
        statusText.textContent = state === "syncing" ? "Syncing..." : state === "error" ? "Sync error" : "Ready";
      }

      function updateStatusBar() {
        const lines = input.value.substring(0, input.selectionStart).split('\\n');
        const line = lines.length;
        const col = lines[lines.length - 1].length + 1;
        
        lineNum.textContent = line;
        colNum.textContent = col;
        charCount.textContent = input.value.length;
      }

      function queueEdit() {
        if (applyingRemoteUpdate) {
          return;
        }

        updateStatusBar();
        
        clearTimeout(pendingTimer);
        setSyncStatus("syncing");
        
        pendingTimer = setTimeout(() => {
          try {
            vscode.postMessage({
              type: "edit",
              text: input.value,
              timestamp: Date.now()
            });
            setSyncStatus("ready");
          } catch (error) {
            console.error("Failed to send edit:", error);
            setSyncStatus("error");
          }
        }, DEBOUNCE_MS);
      }

      function handleWindowMessage(event) {
        try {
          const message = event.data;
          
          if (message.type !== "setText") {
            return;
          }

          if (typeof message.text !== "string") {
            console.warn("Invalid setText message");
            return;
          }

          if (message.text === input.value) {
            return;
          }

          applyingRemoteUpdate = true;
          const selectionStart = input.selectionStart;
          const selectionEnd = input.selectionEnd;
          
          input.value = message.text;
          
          // Restore selection safely
          const maxPos = input.value.length;
          input.selectionStart = Math.min(selectionStart, maxPos);
          input.selectionEnd = Math.min(selectionEnd, maxPos);
          
          updateStatusBar();
          applyingRemoteUpdate = false;
        } catch (error) {
          console.error("Error handling window message:", error);
          applyingRemoteUpdate = false;
        }
      }

      // Initialize
      try {
        input.value = initialText;
        updateStatusBar();
        setSyncStatus("ready");
        
        input.addEventListener("input", queueEdit);
        input.addEventListener("selectionchange", updateStatusBar);
        input.addEventListener("click", updateStatusBar);
        input.addEventListener("keydown", (e) => {
          if (e.key === "Tab") {
            e.preventDefault();
            const start = input.selectionStart;
            const end = input.selectionEnd;
            input.value = input.value.substring(0, start) + "\\t" + input.value.substring(end);
            input.selectionStart = input.selectionEnd = start + 1;
            queueEdit();
          }
        });
        
        window.addEventListener("message", handleWindowMessage);
      } catch (error) {
        console.error("Initialization error:", error);
        setSyncStatus("error");
      }
    })();
  </script>
</body>
</html>`;
  }
}

function getNonce() {
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  let result = "";
  for (let i = 0; i < 32; i += 1) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

function deactivate() {
  // Cleanup is handled per-editor via onDidDispose
}

module.exports = {
  activate,
  deactivate
};