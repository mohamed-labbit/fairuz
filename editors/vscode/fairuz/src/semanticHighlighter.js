const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const { spawn } = require("child_process");

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

  highlight(text, cancellationToken) {
    return new Promise((resolve) => {
      if (typeof text !== "string" || Buffer.byteLength(text, "utf8") > 8 * 1024 * 1024) {
        resolve(null);
        return;
      }
      const child = spawn(this.executable(), ["--semantic-tokens", "-"], {
        stdio: ["pipe", "pipe", "pipe"],
        windowsHide: true
      });
      let stdout = "";
      let settled = false;
      let cancellationSubscription;
      const finish = (value) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        cancellationSubscription?.dispose();
        resolve(value);
      };
      const timer = setTimeout(() => {
        child.kill();
        finish(null);
      }, 5000);
      cancellationSubscription = cancellationToken?.onCancellationRequested(() => {
        child.kill();
        finish(null);
      });
      // Diagnostics are irrelevant to the semantic-token protocol, but this
      // pipe still has to be drained so it cannot back-pressure the child.
      child.stderr.resume();
      child.stdout.setEncoding("utf8");
      child.stdout.on("data", (chunk) => {
        stdout += chunk;
        if (stdout.length > 16 * 1024 * 1024) {
          child.kill();
          finish(null);
        }
      });
      child.on("error", (error) => {
        this.reportUnavailable(error);
        finish(null);
      });
      child.on("close", (code) => {
        if (code !== 0) return finish(null);
        try {
          const parsed = JSON.parse(stdout);
          finish(Array.isArray(parsed.tokens) ? parsed : null);
        } catch (_) {
          finish(null);
        }
      });
      child.stdin.end(text, "utf8");
    });
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
