const vscode = require("vscode");
const { FairuzRtlEditorProvider, FAIRUZ_RTL_VIEW } = require("./rtleditorprovider");
const { FairuzSemanticHighlighter, registerDocumentSemanticTokens } = require("./semanticHighlighter");
const { formatSource } = require("./compilerService");

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
  for (const action of ["undo", "redo", "save", "run", "stop", "format", "find", "replace", "check", "wrap", "outline", "goToLine"]) {
    context.subscriptions.push(vscode.commands.registerCommand(`fairuz.${action}`, () => {
      const handled = rtlEditor.runCommand(action);
      if (!handled && action === "format") return vscode.commands.executeCommand("editor.action.formatDocument");
      return handled;
    }));
  }
  context.subscriptions.push(vscode.languages.registerDocumentFormattingEditProvider({ language: "fairuz" }, {
    async provideDocumentFormattingEdits(document, options, token) {
      if (!vscode.workspace.isTrusted) {
        vscode.window.showWarningMessage("Trust this workspace to enable Fairuz formatting.");
        return [];
      }
      const text = document.getText();
      const version = document.version;
      const result = await formatSource(highlighter.executable(), text, { token });
      if (result.cancelled || token?.isCancellationRequested || document.version !== version) return [];
      if (result.error) {
        vscode.window.showWarningMessage(result.error);
        return [];
      }
      const formatted = result.text;
      return text === formatted ? [] : [vscode.TextEdit.replace(
        new vscode.Range(document.positionAt(0), document.positionAt(text.length)), formatted)];
    }
  }));
}

function deactivate() {}

module.exports = { activate, deactivate };
