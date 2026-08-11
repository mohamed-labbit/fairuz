// Runs inside the webview. Hosts a real Monaco editor instance configured
// for Fairuz's Arabic-first, mixed-direction source (RTL keywords/identifiers
// interleaved with LTR operators, numeric literals, and a few Latin builtins
// like `input`/`str`/`join` -- see fairuz.tmLanguage.json). Monaco's own
// bidi-aware text model and view layer handle caret affinity and mixed-run
// selection correctly; a <textarea> with `direction: rtl` cannot.
(function () {
  "use strict";

  const vscode = acquireVsCodeApi();

  // `version` tracks the TextDocument version this webview's model was last
  // confirmed to match. Every outgoing edit is stamped with it; the
  // extension host rejects any edit whose baseVersion doesn't match its
  // current document.version, instead of applying a stale edit over newer
  // content. This is what replaces the old "just diff full text" approach.
  let version = null;
  let model = null;
  let editor = null;
  let applyingRemoteEdit = false;

  require.config({ paths: { vs: window.__fairuzMonacoBaseUri } });

  require(["vs/editor/editor.main"], function () {
    registerFairuzLanguage();

    model = monaco.editor.createModel("", "fairuz");

    editor = monaco.editor.create(document.getElementById("container"), {
      model,
      theme: "vs-dark",
      automaticLayout: true,
      fontFamily:
        '"IBM Plex Sans Arabic", "Noto Sans Arabic", "SF Mono", Monaco, Consolas, monospace',
      fontSize: 15,
      lineHeight: 1.75,
      minimap: { enabled: true },
      // Monaco auto-detects per-line/per-run direction from Unicode
      // bidi classes when this is on -- it does NOT force the whole
      // buffer RTL the way `direction: rtl; text-align: right` on a
      // <textarea> does, which is what you want for source that mixes
      // Arabic identifiers with `<=`, brackets, and English builtins.
      // See also: our own hand-rolled toy editor didn't have this
      // option at all, since a <textarea>'s bidi behavior isn't
      // programmable.
      "bidi.enabled": true
    });

    editor.onDidChangeModelContent((event) => {
      if (applyingRemoteEdit) return;
      sendEdits(event.changes);
    });

    vscode.postMessage({ type: "ready" });
    window.addEventListener("message", handleExtensionMessage);
  });

  function registerFairuzLanguage() {
    monaco.languages.register({ id: "fairuz" });
    // Full tokenization stays in fairuz.tmLanguage.json for the plain-text
    // VS Code editor; Monaco's Monarch tokenizer here is intentionally a
    // light mirror of it purely for the webview's syntax highlighting; it
    // is not a second source of truth for the grammar, since the .tmLanguage
    // file remains authoritative for VS Code's own editor and for any
    // future tree-sitter grammar work.
    monaco.languages.setMonarchTokensProvider("fairuz", {
      keywords: ["اذا", "غيره", "طالما", "بكل", "ارجع", "اكمل", "اخرج", "دالة"],
      logicalOps: ["و", "او", "ليس"],
      constants: ["صحيح", "خطا", "عدم"],
      tokenizer: {
        root: [
          [/#.*$/, "comment"],
          [/"([^"\\]|\\.)*"/, "string"],
          [/'([^'\\]|\\.)*'/, "string"],
          [/\b\d+(\.\d+)?\b|[٠-٩]+/, "number"],
          [
            /[\p{L}_][\p{L}\p{N}_]*/u,
            {
              cases: {
                "@keywords": "keyword",
                "@logicalOps": "keyword.operator",
                "@constants": "constant",
                "@default": "identifier"
              }
            }
          ],
          [/[+\-*/%٪&|^~<>=]+/, "operator"],
          [/[()\[\]{}]/, "@brackets"]
        ]
      }
    });
  }

  function sendEdits(changes) {
    if (version === null) return; // haven't received an initial version yet

    const edits = changes.map((change) => ({
      startLine: change.range.startLineNumber,
      startColumn: change.range.startColumn,
      endLine: change.range.endLineNumber,
      endColumn: change.range.endColumn,
      text: change.text
    }));

    vscode.postMessage({
      type: "edit",
      baseVersion: version,
      edits
    });
  }

  function handleExtensionMessage(event) {
    const message = event.data;
    if (!message || message.type !== "setText") return;
    if (typeof message.text !== "string" || typeof message.version !== "number") return;

    version = message.version;

    if (!model) return; // Monaco hasn't finished loading yet; version is
                          // still recorded above so the first real edit
                          // carries the right baseVersion once it has.

    if (model.getValue() === message.text) return;

    // Preserve cursor/selection across a full resync using Monaco's own
    // position types rather than clamped string offsets -- this keeps the
    // caret correct even across mixed-direction lines, which raw
    // selectionStart/selectionEnd math (as in the old textarea version)
    // cannot do reliably.
    const selection = editor.getSelection();

    applyingRemoteEdit = true;
    try {
      model.setValue(message.text);
      if (selection) {
        const lineCount = model.getLineCount();
        const clampedLine = Math.min(selection.positionLineNumber, lineCount);
        editor.setSelection(
          new monaco.Selection(clampedLine, selection.selectionStartColumn, clampedLine, selection.positionColumn)
        );
      }
    } finally {
      applyingRemoteEdit = false;
    }
  }
})();