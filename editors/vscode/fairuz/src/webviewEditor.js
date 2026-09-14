import { basicSetup } from "codemirror";
import { redo, undo, indentMore, indentLess, toggleLineComment } from "@codemirror/commands";
import { indentService, indentUnit, foldService, foldCode, unfoldCode } from "@codemirror/language";
import { autocompletion, snippetCompletion, acceptCompletion, nextSnippetField, prevSnippetField } from "@codemirror/autocomplete";
import { openSearchPanel } from "@codemirror/search";
import { setDiagnostics, lintGutter } from "@codemirror/lint";
import { DocumentSync, textChange } from "./documentSync";
import { keywords, builtins, scanLine, indentation, documentOutline } from "./languageTools";
import {
  Compartment,
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

const app = document.getElementById("app") || document.body.appendChild(document.createElement("div"));
app.id = "app";
app.innerHTML = `
  <header class="toolbar" aria-label="أدوات فيروز">
    <div class="identity"><span class="brand-mark" aria-hidden="true">ف</span><div><strong>فيروز</strong><span id="file-name" dir="auto">Fairuz</span></div><span id="dirty-dot" title="تغييرات غير محفوظة" hidden>●</span></div>
    <div class="actions" role="toolbar" aria-label="إجراءات الملف">
      <button data-action="run" class="primary" title="تشغيل · Ctrl/⌘ Enter"><span aria-hidden="true">▷</span> تشغيل</button>
      <button data-action="stop" id="stop-button" hidden title="إيقاف البرنامج">■ إيقاف</button>
      <span class="divider"></span>
      <button data-action="save" title="حفظ · Ctrl/⌘ S">حفظ</button>
      <button data-action="format" title="تنسيق الملف مع الحفاظ على النص والتعليقات · Shift Alt F">تنسيق</button>
      <button data-action="find" title="بحث واستبدال · Ctrl/⌘ F">بحث</button>
      <button data-action="outline" title="الانتقال إلى دالة أو نوع · Ctrl/⌘ Shift O">رموز</button>
      <button data-action="wrap" id="wrap-button" aria-pressed="true" title="التفاف الأسطر · Alt Z">التفاف</button>
      <details class="help"><summary title="اختصارات لوحة المفاتيح" aria-label="اختصارات لوحة المفاتيح">؟</summary><div class="help-card">
        <strong>مساحة للكتابة بالعربية</strong>
        <p>اتجاه طبيعي من اليمين إلى اليسار، مع دعم النص المختلط.</p>
        <dl><dt>تشغيل</dt><dd>⌘ / Ctrl + Enter</dd><dt>حفظ</dt><dd>⌘ / Ctrl + S</dd><dt>تراجع / إعادة</dt><dd>⌘ / Ctrl + Z / Shift Z</dd><dt>بحث</dt><dd>⌘ / Ctrl + F</dd><dt>إكمال</dt><dd>Ctrl + Space</dd><dt>تعليق</dt><dd>⌘ / Ctrl + /</dd><dt>تنسيق</dt><dd>Shift + Alt + F</dd><dt>مسافة بادئة</dt><dd>Tab / Shift + Tab</dd></dl>
        <p>للخروج من المحرر بلوحة المفاتيح: اضغط Escape ثم Tab.</p>
      </div></details>
    </div>
  </header>
  <section id="conflict" class="conflict" role="alert" hidden>
    <span>تغيّر الملف خارج المحرر. كتابتك محفوظة هنا؛ اختر النسخة التي تريد متابعتها.</span>
    <button id="keep-local">الاحتفاظ بكتابتي</button><button id="load-external">استخدام نسخة الملف</button>
  </section>
  <div id="container"></div>
  <section id="problems-panel" class="problems" aria-label="مشكلات الملف" hidden><div class="panel-heading"><strong>مشكلات الملف</strong><button id="close-problems" aria-label="إغلاق المشكلات">×</button></div><div id="problem-list"></div></section>
  <footer class="statusbar"><div><span id="cursor-position" dir="auto"></span><span class="status-separator">·</span><span id="indent-status">4 مسافات</span><span class="direction-badge">RTL</span></div><button id="sync-status" title="إعادة الاتصال بالملف">جارٍ فتح الملف…</button><button id="check-status" title="فحص الملف وعرض المشكلات">بانتظار الملف</button></footer>
  <dialog id="outline-dialog" aria-labelledby="outline-title">
    <div class="panel-heading"><strong id="outline-title">الانتقال إلى رمز</strong><button id="close-outline" aria-label="إغلاق الرموز">×</button></div>
    <input id="outline-query" type="search" placeholder="ابحث عن دالة أو نوع…" aria-label="البحث في الرموز" autocomplete="off" dir="auto">
    <p id="outline-count" aria-live="polite"></p><div id="outline-list" aria-label="رموز الملف"></div>
    <p class="outline-hint">↑ ↓ للتنقل · Enter للانتقال · Escape للإغلاق</p>
  </dialog>
  <div id="notice" class="notice" role="status" aria-live="polite" hidden></div>`;

let applyingRemoteEdit = false;
let semanticRequestId = 0, latestSemanticRequest = 0, checkRequestId = 0;
let formatRequestId = 0, pendingFormat = null;
let highlightTimer, lexicalFallbackTimer, checkTimer, noticeTimer;
let pendingLexicalTokens = [], lastAppliedTokenKey = null, diagnostics = [];
let queuedAction = null, dirty = false, trusted = false, running = false;
let settings = { wordWrap: true, tabSize: 4, liveDiagnostics: true, fontSize: 15, lineHeight: 1.8 };
const wrapConfig = new Compartment(), indentConfig = new Compartment(), editability = new Compartment();
const sync = new DocumentSync(message => vscode.postMessage(message));
const savedView = vscode.getState?.();
let restored = false, metadataSettingsKey = null;
let connectionTimer = null, watchedFlight = null, outlineCache = null, outlineMatches = [], outlineIndex = 0;
const $ = id => document.getElementById(id);
const HIGHLIGHT_IDLE_MS = 180, LEXICAL_FALLBACK_MS = 250;

function completions(context) {
  const line = context.state.doc.lineAt(context.pos);
  const prefix = line.text.slice(0, context.pos - line.from);
  const scanned = scanLine(prefix);
  if (scanned.quote || scanned.code.length < prefix.length) return null;
  const word = context.matchBefore(/[\p{L}\p{N}_]+/u);
  if (!word && !context.explicit) return null;
  const names = new Set();
  // Bound this work for long files; declarations near the caret are most useful.
  const start = Math.max(0, context.pos - 25000), end = Math.min(context.state.doc.length, context.pos + 25000);
  const source = context.state.sliceDoc(start, end);
  for (const line of source.split("\n")) {
    for (const match of scanLine(line).code.matchAll(/[\p{L}_][\p{L}\p{N}_]*/gu)) {
      if (match[0] !== word?.text) names.add(match[0]);
    }
  }
  const options = [
    ...keywords.map(label => ({ label, type: "keyword", detail: "كلمة محجوزة" })),
    ...builtins.map(label => ({ label, type: "function", detail: "دالة مدمجة" })),
    snippetCompletion("دالة ${اسم}(${وسائط}):\n\t${}", { label: "دالة", detail: "قالب دالة", type: "keyword", boost: 2 }),
    snippetCompletion("اذا ${شرط}:\n\t${}", { label: "اذا", detail: "قالب شرط", type: "keyword", boost: 2 }),
    snippetCompletion("لكل ${عنصر} في ${قائمة}:\n\t${}", { label: "لكل", detail: "قالب حلقة", type: "keyword", boost: 2 }),
    ...[...names].filter(name => !keywords.includes(name) && !builtins.includes(name)).map(label => ({ label, type: "variable", detail: "من الملف" }))
  ];
  return { from: word ? word.from : context.pos, options, validFor: /^[\p{L}\p{N}_]*$/u };
}

const view = new EditorView({
  state: EditorState.create({ doc: "", extensions: [
    basicSetup,
    editability.of(EditorState.readOnly.of(true)),
    EditorState.phrases.of({
      "Find": "بحث", "Replace": "استبدال", "next": "التالي", "previous": "السابق", "all": "الكل",
      "match case": "مطابقة حالة الأحرف", "regexp": "تعبير نمطي", "by word": "كلمة كاملة",
      "replace": "استبدال", "replace all": "استبدال الكل", "close": "إغلاق",
      "Fold line": "طي السطر", "Unfold line": "فتح السطر", "Go to line": "اذهب إلى السطر", "go": "انتقل",
      "No diagnostics": "لا مشكلات", "Diagnostics": "المشكلات", "Completions": "اقتراحات الإكمال"
    }),
    Prec.highest(keymap.of([
      { key: "Mod-z", run: undo, preventDefault: true },
      { key: "Mod-Shift-z", run: redo, preventDefault: true },
      { key: "Mod-y", run: redo, preventDefault: true },
      { key: "Mod-s", run: () => command("save"), preventDefault: true },
      { key: "Mod-Enter", run: () => command("run"), preventDefault: true },
      { key: "Mod-f", run: () => command("find"), preventDefault: true },
      { key: "Mod-h", run: () => command("replace"), preventDefault: true },
      { key: "Mod-Shift-o", run: () => command("outline"), preventDefault: true },
      { key: "Alt-Shift-f", run: () => command("format"), preventDefault: true },
      { key: "Alt-z", run: () => command("wrap"), preventDefault: true },
      { key: "Mod-/", run: toggleLineComment, preventDefault: true },
      { key: "Mod-Shift-[", run: foldCode }, { key: "Mod-Shift-]", run: unfoldCode },
      { key: "Tab", run: editor => nextSnippetField(editor) || acceptCompletion(editor) || indentMore(editor) },
      { key: "Shift-Tab", run: editor => prevSnippetField(editor) || indentLess(editor) }
    ])),
    EditorState.languageData.of(() => [{ commentTokens: { line: "#" }, closeBrackets: { brackets: ["(", "[", "{", "'", '"'] } }]),
    indentConfig.of([EditorState.tabSize.of(4), indentUnit.of("    ")]),
    indentService.of((context, pos) => {
      const line = context.state.doc.lineAt(pos);
      if (context.simulatedBreak === null) return indentation(line.text, settings.tabSize);
      const before = line.text.slice(0, pos - line.from);
      return indentation(before, settings.tabSize) + (scanLine(before).code.trimEnd().endsWith(":") ? settings.tabSize : 0);
    }),
    foldService.of((state, from, to) => {
      const first = state.doc.lineAt(from), base = indentation(first.text, settings.tabSize);
      if (!scanLine(first.text).code.trimEnd().endsWith(":")) return null;
      let end = to;
      for (let number = first.number + 1; number <= state.doc.lines; number++) {
        const line = state.doc.line(number);
        if (!line.text.trim()) continue;
        if (indentation(line.text, settings.tabSize) <= base) break;
        end = line.to;
      }
      return end > to ? { from: to, to: end } : null;
    }),
    autocompletion({ override: [completions] }),
    tokenDecorations, lintGutter(), wrapConfig.of(EditorView.lineWrapping),
    EditorView.perLineTextDirection.of(true),
    EditorView.contentAttributes.of({ dir: "rtl", lang: "ar", spellcheck: "false", "aria-label": "محرر فيروز" }),
    EditorView.updateListener.of(handleEditorUpdate)
  ] }), parent: $("container")
});

function persistView() {
  vscode.setState?.({ anchor: view.state.selection.main.anchor, scrollTop: view.scrollDOM.scrollTop,
    recovery: sync.pending || sync.conflict ? { local: sync.local, base: sync.base } : null });
}
function updateStatus() {
  const selection = view.state.selection.main, line = view.state.doc.lineAt(selection.head);
  $("cursor-position").textContent = `سطر ${line.number}، عمود ${[...line.text.slice(0, selection.head - line.from)].length + 1}${selection.empty ? "" : ` · ${selection.to - selection.from} محدد`}`;
  $("dirty-dot").hidden = !(dirty || sync.pending);
  $("conflict").hidden = !sync.conflict;
  document.querySelector('[data-action="run"]').disabled = sync.version === null || !trusted || running || !!sync.conflict;
  for (const action of ["save", "format"]) document.querySelector(`[data-action="${action}"]`).disabled = sync.version === null;
  $("stop-button").hidden = !running;
  $("sync-status").textContent = sync.version === null ? "جارٍ فتح الملف…" : sync.conflict ? "تعارض في النسخ" : sync.pending ? "جارٍ المزامنة…" : dirty ? "غير محفوظ" : "محفوظ";
  $("sync-status").disabled = sync.version !== null && !sync.pending && !sync.conflict;
  watchConnection();
}
function requestConnection() {
  vscode.postMessage({ type: sync.version === null ? "ready" : "sync", recoveryId: sync.flight?.id ?? null });
}
function watchConnection() {
  const key = sync.version === null ? "initial" : sync.flight?.id ?? null;
  if (key === watchedFlight) return;
  clearTimeout(connectionTimer);
  watchedFlight = key;
  if (key === null) return;
  connectionTimer = setTimeout(() => {
    watchedFlight = null;
    showNotice("المزامنة تستغرق وقتاً أطول. نحاول استعادة الاتصال؛ كتابتك باقية هنا.");
    requestConnection();
    watchConnection();
  }, 5000);
}
function handleEditorUpdate(update) {
  if (update.docChanged) {
    outlineCache = null;
    if ($("outline-dialog").open) {
      outlineCache = documentOutline(update.state.doc.toString(), settings.tabSize);
      renderOutline();
    }
  }
  if (update.docChanged && !applyingRemoteEdit && sync.version !== null) {
    sync.edit(update.state.doc.toString());
    lastAppliedTokenKey = null;
    scheduleHighlighting();
    scheduleCheck();
  }
  if (update.docChanged || update.selectionSet) { updateStatus(); persistView(); }
}
function applyTokens(tokens) {
  const key = JSON.stringify(tokens);
  if (key === lastAppliedTokenKey) return;
  lastAppliedTokenKey = key;
  view.dispatch({ effects: replaceTokens.of(tokens) });
}
function scheduleHighlighting(delay = HIGHLIGHT_IDLE_MS) {
  clearTimeout(highlightTimer); clearTimeout(lexicalFallbackTimer);
  const requestId = latestSemanticRequest = ++semanticRequestId;
  highlightTimer = setTimeout(() => {
    const text = view.state.doc.toString();
    pendingLexicalTokens = lexicalTokens(text);
    if (trusted) vscode.postMessage({ type: "semanticTokens", requestId, text });
    lexicalFallbackTimer = setTimeout(() => {
      if (requestId === latestSemanticRequest) applyTokens(pendingLexicalTokens);
    }, trusted ? LEXICAL_FALLBACK_MS : 0);
  }, delay);
}
function scheduleCheck(delay = 700, explicit = false) {
  clearTimeout(checkTimer);
  const requestId = ++checkRequestId;
  diagnostics = [];
  $("check-status").classList.remove("has-errors");
  $("check-status").title = "فحص الملف وعرض المشكلات";
  renderProblems();
  // Dispatch after the current update; nested editor dispatch is forbidden.
  queueMicrotask(() => {
    if (requestId === checkRequestId) view.dispatch(setDiagnostics(view.state, []));
  });
  if (!trusted || (!settings.liveDiagnostics && !explicit)) {
    $("check-status").textContent = trusted ? "فحص الملف" : "التلوين المحلي · مساحة غير موثوقة";
    return;
  }
  $("check-status").textContent = "جارٍ الفحص…";
  checkTimer = setTimeout(() => vscode.postMessage({ type: "check", requestId, text: view.state.doc.toString() }), delay);
}
function replaceDocument(text, history = false) {
  const before = view.state.doc.toString();
  if (text === before) return;
  applyingRemoteEdit = !history;
  try {
    const change = textChange(before, text);
    view.dispatch({ changes: change, annotations: Transaction.addToHistory.of(history) });
  } finally { applyingRemoteEdit = false; }
  lastAppliedTokenKey = null;
  scheduleHighlighting(0); scheduleCheck();
}
function showNotice(text, error = false) {
  $("notice").textContent = text;
  $("notice").classList.toggle("error", error);
  $("notice").hidden = false;
  clearTimeout(noticeTimer);
  noticeTimer = setTimeout(() => { $("notice").hidden = true; }, error ? 9000 : 2600);
}
function flushAction() {
  if (!queuedAction || sync.pending || sync.conflict || sync.version === null) return;
  vscode.postMessage({ type: "action", action: queuedAction, version: sync.version });
  queuedAction = null;
}
function command(action) {
  if (sync.version === null && ["save", "run", "format", "undo", "redo"].includes(action)) {
    showNotice("انتظر حتى يكتمل فتح الملف.");
    return true;
  }
  if (action === "undo") undo(view);
  else if (action === "redo") redo(view);
  else if (action === "find" || action === "replace") openSearchPanel(view);
  else if (action === "outline") openOutline();
  else if (action === "format") {
    if (!trusted || sync.conflict) {
      showNotice(!trusted ? "التنسيق يتطلب مساحة عمل موثوقة." : "حل تعارض الملف قبل التنسيق.", true);
    } else {
      pendingFormat = { requestId: ++formatRequestId, text: view.state.doc.toString() };
      vscode.postMessage({ type: "format", ...pendingFormat });
      showNotice("جارٍ التنسيق…");
    }
  } else if (action === "wrap") {
    settings.wordWrap = !settings.wordWrap;
    view.dispatch({ effects: wrapConfig.reconfigure(settings.wordWrap ? EditorView.lineWrapping : []) });
    $("wrap-button").setAttribute("aria-pressed", String(settings.wordWrap));
  } else if (action === "check") scheduleCheck(0, true);
  else if (action === "stop") vscode.postMessage({ type: "action", action });
  else if (action === "save" || action === "run") {
    if (sync.conflict) showNotice("اختر نسخة الملف أولاً، ثم احفظ أو شغّل البرنامج.", true);
    else {
      queuedAction = action;
      if (sync.pending) showNotice("جارٍ مزامنة الكتابة…");
      flushAction();
    }
  }
  if (!["find", "replace", "outline"].includes(action)) view.focus();
  return true;
}
function openOutline() {
  if (!outlineCache) outlineCache = documentOutline(view.state.doc.toString(), settings.tabSize);
  outlineIndex = 0;
  $("outline-query").value = "";
  renderOutline();
  if (!$("outline-dialog").open) $("outline-dialog").showModal();
  $("outline-query").focus();
}
function renderOutline() {
  const query = $("outline-query").value.trim().normalize("NFC").toLocaleLowerCase();
  outlineMatches = (outlineCache || []).filter(item => `${item.container} ${item.name}`.normalize("NFC").toLocaleLowerCase().includes(query));
  outlineIndex = Math.max(0, Math.min(outlineIndex, outlineMatches.length - 1));
  const list = $("outline-list");
  list.replaceChildren();
  $("outline-count").textContent = outlineMatches.length ? `${outlineMatches.length} رموز` : "لا توجد رموز مطابقة";
  for (const [index, symbol] of outlineMatches.entries()) {
    const button = document.createElement("button");
    button.className = "outline-symbol";
    button.classList.toggle("selected", index === outlineIndex);
    button.setAttribute("aria-current", String(index === outlineIndex));
    const name = document.createElement("span"), detail = document.createElement("small");
    name.dir = "auto";
    name.textContent = symbol.name;
    detail.dir = "auto";
    detail.textContent = `${symbol.kind === "class" ? "نوع" : symbol.kind === "method" ? "دالة عضو" : "دالة"} · ${symbol.container ? `${symbol.container} · ` : ""}سطر ${symbol.line + 1}`;
    button.append(name, detail);
    button.addEventListener("click", () => jumpToSymbol(symbol));
    list.appendChild(button);
  }
}
function jumpToSymbol(symbol) {
  if (!symbol) return;
  $("outline-dialog").close();
  view.dispatch({ selection: { anchor: symbol.from, head: symbol.to }, scrollIntoView: true });
  view.focus();
}
$("outline-query").addEventListener("input", () => { outlineIndex = 0; renderOutline(); });
$("close-outline").addEventListener("click", () => $("outline-dialog").close());
$("outline-dialog").addEventListener("close", () => view.focus());
$("outline-query").addEventListener("keydown", event => {
  if (event.key === "ArrowDown" || event.key === "ArrowUp") {
    event.preventDefault();
    outlineIndex += event.key === "ArrowDown" ? 1 : -1;
    renderOutline();
    $("outline-list").children[outlineIndex]?.scrollIntoView({ block: "nearest" });
  } else if (event.key === "Enter") {
    event.preventDefault();
    jumpToSymbol(outlineMatches[outlineIndex]);
  }
});
function renderProblems() {
  $("problem-list").replaceChildren();
  for (const diagnostic of diagnostics) {
    const button = document.createElement("button");
    button.className = "problem";
    button.dir = "auto";
    button.textContent = `${diagnostic.line + 1}:${diagnostic.start + 1}  ${diagnostic.message}`;
    button.addEventListener("click", () => {
      const line = view.state.doc.line(Math.min(view.state.doc.lines, diagnostic.line + 1));
      const anchor = Math.min(line.to, line.from + diagnostic.start);
      view.dispatch({ selection: { anchor }, scrollIntoView: true }); view.focus();
    });
    $("problem-list").appendChild(button);
  }
}
for (const button of document.querySelectorAll("[data-action]")) button.addEventListener("click", () => command(button.dataset.action));
$("sync-status").addEventListener("click", requestConnection);
$("check-status").addEventListener("click", () => {
  if (!diagnostics.length) command("check");
  else { renderProblems(); $("problems-panel").hidden = !$("problems-panel").hidden; }
});
$("close-problems").addEventListener("click", () => { $("problems-panel").hidden = true; view.focus(); });
for (const [id, keep] of [["keep-local", true], ["load-external", false]]) {
  $(id).addEventListener("click", () => {
    const text = sync.resolve(keep);
    if (typeof text === "string") replaceDocument(text, !keep);
    queuedAction = null; updateStatus(); persistView(); view.focus();
  });
}
view.scrollDOM.addEventListener("scroll", persistView, { passive: true });
window.addEventListener("keydown", event => {
  if (event.key === "Escape") {
    document.querySelector(".help").open = false;
    $("problems-panel").hidden = true;
  }
});
window.addEventListener("message", event => {
  const message = event.data;
  if (!message) return;
  if (message.type === "setText" && typeof message.text === "string" && Number.isSafeInteger(message.version)) {
    const result = sync.receive(message.text, message.version, message.rejected, message.authoritative, message.recoveryId);
    if (typeof result.text === "string") replaceDocument(result.text);
    if (!restored) {
      restored = true;
      view.dispatch({ effects: editability.reconfigure(EditorState.readOnly.of(false)) });
      if (savedView?.recovery && savedView.recovery.local !== sync.base) {
        sync.local = savedView.recovery.local;
        sync.conflict = { text: sync.base, version: sync.version };
        replaceDocument(sync.local);
      }
      const anchor = Math.min(savedView?.anchor || 0, view.state.doc.length);
      view.dispatch({ selection: { anchor } });
      view.scrollDOM.scrollTop = savedView?.scrollTop || 0;
    }
    if (sync.conflict) queuedAction = null;
    updateStatus(); persistView(); flushAction();
  } else if (message.type === "ack" && Number.isSafeInteger(message.version)) {
    sync.acknowledge(message); updateStatus(); persistView(); flushAction();
  } else if (message.type === "command") command(message.action);
  else if (message.type === "metadata") {
    $("file-name").textContent = message.name || "Fairuz";
    dirty = !!message.dirty;
    const settingsKey = JSON.stringify(message.settings);
    const changed = settingsKey !== metadataSettingsKey || trusted !== !!message.trusted;
    metadataSettingsKey = settingsKey;
    trusted = !!message.trusted;
    if (changed) {
      settings = { ...settings, ...message.settings };
      settings.fontSize = Math.max(10, Math.min(32, Number(settings.fontSize) || 15));
      settings.lineHeight = Math.max(1.3, Math.min(2.5, Number(settings.lineHeight) || 1.8));
      settings.tabSize = Math.max(1, Math.min(8, Math.floor(Number(settings.tabSize) || 4)));
      outlineCache = null;
      if ($("outline-dialog").open) {
        outlineCache = documentOutline(view.state.doc.toString(), settings.tabSize);
        renderOutline();
      }
      app.style.setProperty("--fairuz-font-size", `${settings.fontSize}px`);
      app.style.setProperty("--fairuz-line-height", String(settings.lineHeight));
      view.dispatch({ effects: [wrapConfig.reconfigure(settings.wordWrap ? EditorView.lineWrapping : []),
        indentConfig.reconfigure([EditorState.tabSize.of(settings.tabSize), indentUnit.of(" ".repeat(settings.tabSize))])] });
      $("indent-status").textContent = `${settings.tabSize} مسافات`;
      $("wrap-button").setAttribute("aria-pressed", String(settings.wordWrap));
      scheduleHighlighting(0); scheduleCheck();
    }
    updateStatus();
  } else if (message.type === "formatted" && message.requestId === pendingFormat?.requestId) {
    const original = pendingFormat.text;
    pendingFormat = null;
    if (message.error) showNotice(message.error, true);
    else if (view.state.doc.toString() !== original || sync.conflict)
      showNotice("تغير النص أثناء التنسيق؛ أعد المحاولة.");
    else if (typeof message.text === "string") {
      replaceDocument(message.text, true);
      showNotice("تم التنسيق مع الحفاظ على التعليقات");
    }
  } else if (message.type === "semanticTokens" && message.requestId === latestSemanticRequest) {
    clearTimeout(lexicalFallbackTimer);
    applyTokens(message.tokens?.length || !view.state.doc.length ? message.tokens : pendingLexicalTokens);
  } else if (message.type === "diagnostics" && message.requestId === checkRequestId) {
    diagnostics = message.diagnostics || [];
    const mapped = diagnostics.map(item => {
      const line = view.state.doc.line(Math.min(view.state.doc.lines, item.line + 1));
      const from = Math.min(line.to, line.from + item.start);
      return { from, to: Math.min(line.to, from + item.length), severity: item.severity, message: item.message };
    });
    view.dispatch(setDiagnostics(view.state, mapped));
    $("check-status").textContent = message.error ? "الفحص غير متاح" : diagnostics.length ? (diagnostics.length === 1 ? "مشكلة واحدة" : `${diagnostics.length} مشكلات`) : "✓ لا أخطاء في الفحص";
    $("check-status").classList.toggle("has-errors", diagnostics.length > 0);
    $("check-status").title = message.error || "فحص الملف وعرض المشكلات";
    renderProblems();
  } else if (message.type === "notice") showNotice(message.message, message.error);
  else if (message.type === "runState") { running = !!message.running; updateStatus(); }
});
updateStatus();
vscode.postMessage({ type: "ready" });
