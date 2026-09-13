import { basicSetup } from "codemirror";
import { redo, undo } from "@codemirror/commands";
import {
  EditorState,
  Prec,
  RangeSetBuilder,
  StateEffect,
  StateField,
  Transaction
} from "@codemirror/state";
import { Decoration, EditorView, keymap } from "@codemirror/view";

const vscode = acquireVsCodeApi();

const KEYWORDS = new Set([
  "اذا", "غيره", "طالما", "لكل", "في", "ارجع", "اكمل", "اخرج",
  "دالة", "نوع", "هذا", "استورد", "من", "باسم", "تاكد", "assert",
  "و", "او", "ليس"
]);
const BOOLEANS = new Set(["صحيح", "خطا"]);
const NULLS = new Set(["عدم"]);
const IDENTIFIER_START = /[\p{L}_]/u;
const IDENTIFIER_CONTINUE = /[\p{L}\p{N}_]/u;
const DIGIT = /[0-9٠-٩]/u;
const OPERATOR = /[+\-*/%٪&|^~<>=:]/u;

const replaceTokens = StateEffect.define();

const tokenDecorations = StateField.define({
  create() {
    return Decoration.none;
  },
  update(decorations, transaction) {
    decorations = decorations.map(transaction.changes);
    for (const effect of transaction.effects) {
      if (effect.is(replaceTokens))
        decorations = buildDecorations(transaction.state.doc, effect.value);
    }
    return decorations;
  },
  provide: (field) => EditorView.decorations.from(field)
});

function buildDecorations(doc, tokens) {
  const ranges = [];
  for (const token of tokens || []) {
    if (!Number.isInteger(token.line) || !Number.isInteger(token.start)
        || !Number.isInteger(token.length) || token.length <= 0
        || token.line < 0 || token.line >= doc.lines) continue;
    const line = doc.line(token.line + 1);
    const from = line.from + token.start;
    const to = from + token.length;
    if (from < line.from || to > line.to) continue;
    const type = /^[a-z]+$/.test(token.type) ? token.type : "variable";
    const classes = `fa-token fa-${type}${token.declaration ? " fa-declaration" : ""}`;
    ranges.push({ from, to, decoration: Decoration.mark({ class: classes }) });
  }
  ranges.sort((a, b) => a.from - b.from || a.to - b.to);
  const builder = new RangeSetBuilder();
  let previousTo = -1;
  for (const range of ranges) {
    if (range.from < previousTo) continue;
    builder.add(range.from, range.to, range.decoration);
    previousTo = range.to;
  }
  return builder.finish();
}

function lexicalTokens(text) {
  const tokens = [];
  let offset = 0;
  let line = 0;
  let column = 0;
  const push = (startOffset, startLine, startColumn, type) => {
    tokens.push({
      line: startLine,
      start: startColumn,
      length: offset - startOffset,
      type,
      declaration: false
    });
  };
  while (offset < text.length) {
    const startOffset = offset;
    const startLine = line;
    const startColumn = column;
    const char = text[offset];
    if (char === "\n") {
      offset++;
      line++;
      column = 0;
      continue;
    }
    if (char === "#") {
      while (offset < text.length && text[offset] !== "\n") {
        offset++;
        column++;
      }
      push(startOffset, startLine, startColumn, "comment");
      continue;
    }
    if (char === "\"" || char === "'") {
      const quote = char;
      offset++;
      column++;
      let escaped = false;
      while (offset < text.length && text[offset] !== "\n") {
        const current = text[offset++];
        column++;
        if (!escaped && current === quote) break;
        escaped = !escaped && current === "\\";
      }
      push(startOffset, startLine, startColumn, "string");
      continue;
    }
    if (IDENTIFIER_START.test(char)) {
      offset++;
      column++;
      while (offset < text.length && IDENTIFIER_CONTINUE.test(text[offset])) {
        offset++;
        column++;
      }
      const word = text.slice(startOffset, offset);
      push(startOffset, startLine, startColumn,
        BOOLEANS.has(word) ? "boolean" : NULLS.has(word) ? "null"
          : KEYWORDS.has(word) ? "keyword" : "variable");
      continue;
    }
    if (DIGIT.test(char)) {
      offset++;
      column++;
      while (offset < text.length && /[0-9٠-٩A-Fa-f_xXoObB.]/u.test(text[offset])) {
        offset++;
        column++;
      }
      push(startOffset, startLine, startColumn, "number");
      continue;
    }
    if (OPERATOR.test(char)) {
      offset++;
      column++;
      while (offset < text.length && OPERATOR.test(text[offset])) {
        offset++;
        column++;
      }
      push(startOffset, startLine, startColumn, "operator");
      continue;
    }
    // CodeMirror and the compiler protocol both count UTF-16 code units.
    const width = text.codePointAt(offset) > 0xffff ? 2 : 1;
    offset += width;
    column += width;
  }
  return tokens;
}

let documentVersion = null;
let applyingRemoteEdit = false;
let semanticRequestId = 0;
let latestSemanticRequest = 0;
let highlightTimer = null;
let lexicalFallbackTimer = null;
let pendingLexicalTokens = [];
let lastAppliedTokenKey = null;

const HIGHLIGHT_IDLE_MS = 160;
const LEXICAL_FALLBACK_MS = 220;

const view = new EditorView({
  state: EditorState.create({
    doc: "",
    extensions: [
      basicSetup,
      // VS Code normally owns these shortcuts before a webview sees them.
      // Keep a highest-priority local binding as a fallback for standalone
      // use and platforms where the key event is delivered to the webview.
      Prec.highest(keymap.of([
        { key: "Mod-z", run: undo, preventDefault: true },
        { key: "Mod-Shift-z", run: redo, preventDefault: true },
        { key: "Mod-y", run: redo, preventDefault: true }
      ])),
      tokenDecorations,
      EditorView.lineWrapping,
      EditorView.perLineTextDirection.of(true),
      EditorView.contentAttributes.of({ dir: "rtl", lang: "ar", spellcheck: "false" }),
      EditorView.updateListener.of(handleEditorUpdate)
    ]
  }),
  parent: document.getElementById("container")
});

function offsetPosition(doc, offset) {
  const line = doc.lineAt(offset);
  return { line: line.number, column: offset - line.from + 1 };
}

function handleEditorUpdate(update) {
  if (!update.docChanged || applyingRemoteEdit || documentVersion === null) return;
  const edits = [];
  update.changes.iterChanges((fromA, toA, _fromB, _toB, inserted) => {
    const start = offsetPosition(update.startState.doc, fromA);
    const end = offsetPosition(update.startState.doc, toA);
    edits.push({
      startLine: start.line,
      startColumn: start.column,
      endLine: end.line,
      endColumn: end.column,
      text: inserted.toString()
    });
  });
  const baseVersion = documentVersion;
  // VS Code increments a TextDocument once for each accepted WorkspaceEdit.
  // Advance optimistically so rapid keystrokes queue against successive
  // versions instead of every second edit being rejected as stale.
  documentVersion++;
  vscode.postMessage({ type: "edit", baseVersion, edits });
  scheduleHighlighting();
}

function applyTokens(tokens) {
  const tokenKey = (tokens || []).map((token) =>
    `${token.line}:${token.start}:${token.length}:${token.type}:${token.declaration ? 1 : 0}`
  ).join(";");
  if (tokenKey === lastAppliedTokenKey) return;
  lastAppliedTokenKey = tokenKey;
  view.dispatch({ effects: replaceTokens.of(tokens) });
}

function requestHighlighting(requestId) {
  highlightTimer = null;
  const text = view.state.doc.toString();
  pendingLexicalTokens = lexicalTokens(text);
  vscode.postMessage({ type: "semanticTokens", requestId, text });

  // Usually the parser answers before this expires, producing one repaint.
  // If it is unavailable or busy, the lexical highlighter keeps the editor
  // useful without making every keystroke synchronously recolor the document.
  clearTimeout(lexicalFallbackTimer);
  lexicalFallbackTimer = setTimeout(() => {
    if (requestId === latestSemanticRequest) applyTokens(pendingLexicalTokens);
  }, LEXICAL_FALLBACK_MS);
}

function scheduleHighlighting(delay = HIGHLIGHT_IDLE_MS) {
  clearTimeout(highlightTimer);
  clearTimeout(lexicalFallbackTimer);
  // Allocate the request id now, not when the timer fires. This immediately
  // makes any parser response for the pre-edit document stale.
  const requestId = ++semanticRequestId;
  latestSemanticRequest = requestId;
  highlightTimer = setTimeout(() => requestHighlighting(requestId), delay);
}

function replaceDocument(text) {
  const anchor = Math.min(view.state.selection.main.anchor, text.length);
  applyingRemoteEdit = true;
  try {
    // A whole-document replacement discards mapped decorations. Force the
    // next token set to be installed even if its signature matches the old
    // document by coincidence.
    lastAppliedTokenKey = null;
    view.dispatch({
      changes: { from: 0, to: view.state.doc.length, insert: text },
      selection: { anchor },
      // Initial loads and edits coming from VS Code are synchronization, not
      // user actions. They must not become entries in CodeMirror's history.
      annotations: Transaction.addToHistory.of(false)
    });
  } finally {
    applyingRemoteEdit = false;
  }
  scheduleHighlighting(0);
}

window.addEventListener("message", (event) => {
  const message = event.data;
  if (!message) return;
  if (message.type === "setText"
      && typeof message.text === "string" && typeof message.version === "number") {
    documentVersion = message.version;
    if (view.state.doc.toString() !== message.text) replaceDocument(message.text);
    else scheduleHighlighting(0);
    return;
  }
  if (message.type === "ack" && typeof message.version === "number") {
    documentVersion = message.version;
    return;
  }
  if (message.type === "history") {
    if (message.action === "undo") undo(view);
    else if (message.action === "redo") redo(view);
    return;
  }
  if (message.type === "semanticTokens" && message.requestId === latestSemanticRequest) {
    clearTimeout(lexicalFallbackTimer);
    // Keep the deferred lexical fallback when the native highlighter is
    // unavailable. An empty successful stream is only meaningful for an
    // empty document.
    if (Array.isArray(message.tokens)
        && (message.tokens.length > 0 || view.state.doc.length === 0))
      applyTokens(message.tokens);
    else
      applyTokens(pendingLexicalTokens);
  }
});

vscode.postMessage({ type: "ready" });
