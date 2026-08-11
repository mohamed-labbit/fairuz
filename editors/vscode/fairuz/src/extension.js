const vscode = require("vscode");
const { FairuzRtlEditorProvider, FAIRUZ_RTL_VIEW } = require("./rtlEditorProvider");

function activate(context) {
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

  context.subscriptions.push(FairuzRtlEditorProvider.register(context));
}

function deactivate() {}

module.exports = { activate, deactivate };