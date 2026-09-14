const { test } = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");
const { runCompiler, checkSource, formatSource } = require("../src/compilerService");

test("compiler process reports missing executables without throwing", async () => {
  const result = await runCompiler('/no/such/fairuz-editor-test', []);
  assert.match(result.error, /ENOENT/);
});
test("compiler process enforces a timeout", async () => {
  const result = await runCompiler(process.execPath, ['-e', 'setInterval(() => {}, 1000)'], { timeout: 80 });
  assert.match(result.error, /too long/);
});
test("early process exit while stdin is pending cannot produce unhandled EPIPE", async () => {
  const result = await runCompiler(process.execPath, ['-e', 'process.exit(0)'], { input: 'x'.repeat(1024 * 1024) });
  assert.equal(result.code, 0);
});
test("cancelled requests never launch a process", async () => {
  assert.deepEqual(await runCompiler('/no/such/executable', [], { token: { isCancellationRequested: true } }), { cancelled: true });
});
test("live checker uses the real Fairuz compiler on unsaved valid and invalid source", {
  skip: !process.env.FAIRUZ_TEST_COMPILER
}, async () => {
  const executable = process.env.FAIRUZ_EXECUTABLE || path.resolve(__dirname, '../../../../build/fairuz');
  const valid = await checkSource(executable, 'اكتب("مرحبا")\n');
  assert.equal(valid.error, undefined);
  assert.deepEqual(valid.diagnostics, []);
  const invalid = await checkSource(executable, 'دالة مثال(:\n');
  assert.equal(invalid.error, undefined);
  assert.ok(invalid.diagnostics.length > 0);
  assert.equal(invalid.diagnostics[0].line, 0);
});

test("editor formatting reports unavailable compilers and cancellation without edits", async () => {
  assert.match((await formatSource('/no/such/fairuz-editor-test', 'ن:=1\n')).error, /ENOENT/);
  assert.equal((await formatSource('/no/such/executable', 'ن:=1', {
    token: { isCancellationRequested: true }
  })).cancelled, true);
});

test("editor uses the real formatter for unsaved text and rejects invalid source", {
  skip: !process.env.FAIRUZ_TEST_COMPILER
}, async () => {
  const executable = process.env.FAIRUZ_EXECUTABLE || path.resolve(__dirname, '../../../../build/fairuz');
  const source = '# عنوان\r\nلكل عنصر في [1,2]:\r\n\tتاكد عنصر>0\r\n';
  const result = await formatSource(executable, source);
  assert.equal(result.error, undefined);
  assert.equal(result.text, '# عنوان\r\nلكل عنصر في [1، 2]:\r\n    تاكد عنصر > 0\r\n');
  assert.equal((await formatSource(executable, result.text)).text, result.text);
  const invalid = await formatSource(executable, 'دالة مثال(:\n');
  assert.equal(invalid.text, undefined);
  assert.ok(invalid.error);
  const unfinished = await formatSource(executable, 'اكتب("نص  \n\tتكملة  ');
  assert.equal(unfinished.text, undefined);
  assert.ok(unfinished.error);
});

test("RTL native_contract list keeps its items and normalizes continuation indentation", {
  skip: !process.env.FAIRUZ_TEST_COMPILER
}, async () => {
  const executable = process.env.FAIRUZ_EXECUTABLE || path.resolve(__dirname, '../../../../build/fairuz');
  const source = 'بدائيات_الذاكرة := [\n'
    + '    "__رياضيات__"، "__رياضيات2_"، "__استدعاء__"، "__اقرا_سلسلة_JSON__"، "__اهرب_JSON__"    ،"__base64_رمز"، "__base64_فك"،\n'
    + '    "__عشري_جديد"، "__عشري_عملية"، "__عشري_نص"،\n'
    + '        "__hex_رمز"، "__hex_فك"\n]\n';
  const expected = 'بدائيات_الذاكرة := [\n'
    + '    "__رياضيات__"، "__رياضيات2_"، "__استدعاء__"، "__اقرا_سلسلة_JSON__"، "__اهرب_JSON__"، "__base64_رمز"، "__base64_فك"،\n'
    + '    "__عشري_جديد"، "__عشري_عملية"، "__عشري_نص"،\n'
    + '    "__hex_رمز"، "__hex_فك"\n]\n';
  const result = await formatSource(executable, source);
  assert.equal(result.error, undefined);
  assert.equal(result.text, expected);
  assert.deepEqual(result.text.match(/"[^"]*"/g), source.match(/"[^"]*"/g));
  assert.equal((await formatSource(executable, result.text)).text, result.text);
});
