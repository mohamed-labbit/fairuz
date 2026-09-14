const { test } = require("node:test");
const assert = require("node:assert/strict");
const { formatWhitespace, parseDiagnostics, scanLine, documentOutline } = require("../src/languageTools");

test("formatting preserves comments, quoted whitespace, Arabic and CRLF", () => {
  const source = '# مقدمة  \r\nدالة مثال():   \r\n\tاكتب("نص  ") # شرح  \r\n';
  assert.equal(formatWhitespace(source), '# مقدمة\r\nدالة مثال():\r\n    اكتب("نص  ") # شرح\r\n');
  assert.equal(formatWhitespace(formatWhitespace(source)), formatWhitespace(source));
});
test("unfinished multiline strings are never trimmed or reindented", () => {
  const source = 'اكتب("نص  \n\tتكملة  ';
  assert.equal(formatWhitespace(source), source);
});
test("tabs expand to tab stops without changing nesting", () => {
  assert.equal(formatWhitespace(' \tاكتب(1)\n', 4), '    اكتب(1)\n');
});
test("colons and hashes inside strings do not start blocks or comments", () => {
  assert.equal(scanLine('اكتب("#:") # comment').code.trimEnd(), 'اكتب(    )');
  assert.ok(scanLine('اذا صحيح: # comment').code.trimEnd().endsWith(":"));
});
test("diagnostics map codepoint columns to UTF-16 after Arabic and emoji", () => {
  const result = parseDiagnostics('buffer.fa: error: Unexpected token\n  --> line 1:5\n', 'ا😀ب +');
  assert.equal(result[0].start, 5);
  assert.equal(result[0].length, 1);
  assert.equal(result[0].message, 'Unexpected token');
});
test("notes and source snippets cannot steal an error location", () => {
  const result = parseDiagnostics('x: error: First\n  --> line 2:1\n  2 | x\nnote: context\n --> line 8\nx: warning: Second\n --> line 1:2\n', 'abc\ndef');
  assert.equal(result.length, 2);
  assert.equal(result[0].line, 1);
  assert.equal(result[1].severity, 'warning');
});
test("outline identifies classes, methods and nested functions with exact UTF-16 ranges", () => {
  const source = '# 😀\nنوع حساب:\n    دالة احسب(س):\n        دالة داخلي():\n            ارجع 1\n        ارجع داخلي()\nدالة رئيسية():\n    ارجع عدم\n';
  const symbols = documentOutline(source);
  assert.deepEqual(symbols.map(item => [item.name, item.kind, item.depth]), [
    ['حساب', 'class', 0], ['احسب', 'method', 1], ['داخلي', 'function', 2], ['رئيسية', 'function', 0]
  ]);
  for (const symbol of symbols) assert.equal(source.slice(symbol.from, symbol.to), symbol.name);
  assert.equal(symbols[2].container, 'حساب › احسب');
});
test("outline ignores declarations in comments and strings, and accepts incomplete headers", () => {
  const source = '# دالة تعليق():\nاكتب("نوع نص:")\nس := "بداية\nدالة مخفي():\nنهاية"\nدالة فعلي(';
  assert.deepEqual(documentOutline(source).map(item => item.name), ['فعلي']);
});
test("masking quoted supplementary characters preserves UTF-16 offsets", () => {
  const source = 'اكتب("😀") + قيمة';
  const masked = scanLine(source).code;
  assert.equal(masked.length, source.length);
  assert.equal(masked.indexOf('قيمة'), source.indexOf('قيمة'));
});
