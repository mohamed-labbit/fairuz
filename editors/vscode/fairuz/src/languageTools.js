// Shared by the custom editor, native VS Code formatting, and regression tests.
const keywords = ["اذا", "غيره", "طالما", "لكل", "في", "ارجع", "اكمل", "اخرج", "دالة", "نوع", "هذا", "استورد", "من", "باسم", "تاكد", "و", "او", "ليس", "صحيح", "خطا", "عدم"];
const builtins = ["اكتب", "ادخل", "طول", "اضف", "احذف", "مقطع", "قائمة", "قاموس", "صنف", "طبيعي", "حقيقي", "سلسلة", "منطقي", "اقسم", "اجمع", "جزء", "يحتوي", "قص", "مطلق", "اصغر", "اكبر", "قوة", "جذر", "ساعة", "عطل"];

// Scan strings/comments rather than treating a colon or hash inside a string
// as structural punctuation. Quotes may span lines in unfinished source.
function scanLine(text, initialQuote = null) {
  let quote = initialQuote, escaped = false, code = "";
  for (const char of text) {
    if (quote) {
      if (!escaped && char === quote) quote = null;
      escaped = !escaped && char === "\\";
      code += " ".repeat(char.length);
    } else if (char === "#") break;
    else if (char === "'" || char === '"') { quote = char; code += " "; }
    else code += char;
  }
  return { code, quote };
}

// Navigation is deliberately tolerant of unfinished declarations. This is an
// outline, not a replacement for the compiler's parser or scope analysis.
function documentOutline(source, tabSize = 4) {
  const symbols = [], stack = [];
  let quote = null, offset = 0;
  const lines = source.split("\n");
  for (let line = 0; line < lines.length; line++) {
    const text = lines[line], startedInString = quote !== null;
    const scanned = scanLine(text, quote);
    quote = scanned.quote;
    const code = scanned.code;
    if (!startedInString && code.trim()) {
      const indent = indentation(text, tabSize);
      while (stack.length && indent <= stack[stack.length - 1].indent) stack.pop();
      const match = code.match(/^[ \t]*(دالة|نوع)\s+([\p{L}_][\p{L}\p{N}\p{M}_]*)/u);
      if (match) {
        const parent = stack[stack.length - 1];
        const start = match[0].length - match[2].length;
        const kind = match[1] === "نوع" ? "class" : parent?.kind === "class" ? "method" : "function";
        const symbol = { name: match[2], kind, line, from: offset + start, to: offset + start + match[2].length,
          indent, depth: stack.length, container: stack.map(item => item.name).join(" › ") };
        symbols.push(symbol);
        stack.push(symbol);
      }
    }
    offset += text.length + 1;
  }
  return symbols;
}

function formatWhitespace(text, tabSize = 4) {
  let quote = null;
  const eol = text.includes("\r\n") ? "\r\n" : "\n";
  const lines = text.split(/\r?\n/).map(line => {
    const startedInString = quote !== null;
    const result = scanLine(line, quote);
    quote = result.quote;
    if (!startedInString) {
      line = line.replace(/^[ \t]+/, whitespace => {
        let width = 0;
        for (const char of whitespace) width += char === "\t" ? tabSize - width % tabSize : 1;
        return " ".repeat(width);
      });
    }
    return quote ? line : line.replace(/[ \t]+$/, "");
  });
  let formatted = lines.join(eol);
  if (formatted && !quote && !formatted.endsWith("\n")) formatted += eol;
  return formatted;
}

function indentation(line, unit = 4) {
  const whitespace = line.match(/^[ \t]*/)[0];
  let width = 0;
  for (const char of whitespace) width += char === "\t" ? unit - width % unit : 1;
  return width;
}

// User-facing columns count code points, just like the cursor status. Keep
// conversion at the boundary so navigation never splits a surrogate pair.
function lineLocation(doc, query) {
  const normalized = query.trim().replace(/[٠-٩۰-۹]/g, digit =>
    String(digit.charCodeAt(0) - (digit <= "٩" ? 0x660 : 0x6f0)));
  const match = normalized.match(/^(\d+)(?:\s*:\s*(\d+))?$/);
  if (!match) return null;
  const number = Number(match[1]), column = Number(match[2] || 1);
  if (!Number.isSafeInteger(number) || !Number.isSafeInteger(column)
      || number < 1 || number > doc.lines || column < 1) return null;
  const line = doc.line(number);
  let offset = 0, remaining = column - 1;
  for (const char of line.text) {
    if (remaining-- === 0) break;
    offset += char.length;
  }
  return line.from + offset;
}

function parseDiagnostics(stderr, source) {
  const lines = source.split(/\r?\n/), diagnostics = [];
  let current = null;
  for (const line of stderr.replace(/\x1b\[[0-9;]*m/g, "").split("\n")) {
    const header = line.match(/(?:^|: )(error|fatal|warning):\s*(.*)/);
    if (header) {
      current = { severity: header[1] === "warning" ? "warning" : "error", message: header[2].trim(), line: 0, start: 0, length: 1 };
      diagnostics.push(current);
      continue;
    }
    const location = line.match(/^\s*--> line (\d+):(\d+)/);
    if (current && location) {
      current.line = Math.min(lines.length - 1, Math.max(0, Number(location[1]) - 1));
      const content = lines[current.line];
      // The lexer counts Unicode code points; VS Code and CodeMirror use UTF-16.
      current.start = [...content].slice(0, Math.max(0, Number(location[2]) - 1)).join("").length;
      const point = content.codePointAt(current.start);
      current.length = point > 0xffff ? 2 : current.start < content.length ? 1 : 0;
      current = null;
    }
  }
  return diagnostics.slice(0, 30);
}

module.exports = { keywords, builtins, scanLine, formatWhitespace, indentation, parseDiagnostics, documentOutline, lineLocation };
