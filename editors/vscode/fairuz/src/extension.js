const vscode = require("vscode");
const { FairuzRtlEditorProvider, FAIRUZ_RTL_VIEW } = require("./rtleditorprovider");
const { FairuzSemanticHighlighter, registerDocumentSemanticTokens } = require("./semanticHighlighter");

function activate(context) {
  const highlighter = new FairuzSemanticHighlighter(context);
  context.subscriptions.push(registerDocumentSemanticTokens(context, highlighter));
  context.subscriptions.push(
    vscode.commands.registerCommand("fairuz.openRtlEditor", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        vscode.window.showInformationMessage("No active editor to reopen.");
        return;
      }
      try {
        await vscode.commands.executeCommand(
          "vscode.openWith",
          editor.document.uri,
          FAIRUZ_RTL_VIEW
        );
      } catch (error) {
        vscode.window.showErrorMessage(`Failed to open RTL editor: ${error.message}`);
      }
    })
  );

  const rtlEditor = FairuzRtlEditorProvider.register(context, highlighter);
  context.subscriptions.push(rtlEditor);
  context.subscriptions.push(
    vscode.commands.registerCommand("fairuz.undo", () => rtlEditor.runHistoryCommand("undo")),
    vscode.commands.registerCommand("fairuz.redo", () => rtlEditor.runHistoryCommand("redo"))
  );
}

function deactivate() {}

module.exports = { activate, deactivate };
