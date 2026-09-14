const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const { runCompiler } = require("./compilerService");

const TOKEN_TYPES = [
  "keyword", "comment", "string", "number", "operator", "boolean", "null",
  "variable", "parameter", "property", "function", "method", "class", "namespace"
];
const TOKEN_MODIFIERS = ["declaration"];

class FairuzSemanticHighlighter {
  constructor(context) {
    this.context = context;
    this.reportedUnavailable = false;
  }

  executable() {
    const configured = vscode.workspace.getConfiguration("fairuz").get("executablePath", "").trim();
    if (configured) return configured;
    if (!vscode.workspace.isTrusted) return "fairuz";

    // An installed VSIX lives under ~/.vscode/extensions, so paths relative
    // to extensionPath cannot locate the compiler in the user's checkout.
    // Prefer a build in any currently open workspace, then support running
    // the extension directly from this repository during development.
    for (const folder of vscode.workspace.workspaceFolders || []) {
      const workspaceBinary = path.join(folder.uri.fsPath, "build", "fairuz");
      if (fs.existsSync(workspaceBinary)) return workspaceBinary;
    }
    const developmentBinary = path.resolve(this.context.extensionPath, "../../..", "build", "fairuz");
    if (fs.existsSync(developmentBinary)) return developmentBinary;
    return "fairuz";
  }

  reportUnavailable(error) {
    if (this.reportedUnavailable) return;
    this.reportedUnavailable = true;
    const detail = error?.code === "ENOENT"
      ? "The Fairuz executable was not found. Build the workspace or set fairuz.executablePath."
      : `The Fairuz semantic highlighter could not start: ${error?.message || "unknown error"}`;
    vscode.window.showWarningMessage(detail);
  }

  async highlight(text, cancellationToken) {
    if (!vscode.workspace.isTrusted || typeof text !== "string"
        || Buffer.byteLength(text, "utf8") > 8 * 1024 * 1024) return null;
    const result = await runCompiler(this.executable(), ["--semantic-tokens", "-"], {
      input: text, token: cancellationToken
    });
    if (result.error) {
      this.reportUnavailable(new Error(result.error));
      return null;
    }
    if (result.cancelled || result.code !== 0) return null;
    try {
      const parsed = JSON.parse(result.stdout);
      return Array.isArray(parsed.tokens) ? parsed : null;
    } catch (_) { return null; }
  }

}

function registerDocumentSemanticTokens(context, highlighter) {
  const legend = new vscode.SemanticTokensLegend(TOKEN_TYPES, TOKEN_MODIFIERS);
  const provider = {
    async provideDocumentSemanticTokens(document, cancellationToken) {
      const result = await highlighter.highlight(document.getText(), cancellationToken);
      const builder = new vscode.SemanticTokensBuilder(legend);
      if (result) {
        for (const token of result.tokens) {
          const type = TOKEN_TYPES.indexOf(token.type);
          if (type < 0 || token.length <= 0) continue;
          builder.push(token.line, token.start, token.length, type, token.declaration ? 1 : 0);
        }
      }
      return builder.build();
    }
  };
  return vscode.languages.registerDocumentSemanticTokensProvider({ language: "fairuz" }, provider, legend);
}

module.exports = {
  FairuzSemanticHighlighter,
  registerDocumentSemanticTokens,
  TOKEN_TYPES,
  TOKEN_MODIFIERS
};
