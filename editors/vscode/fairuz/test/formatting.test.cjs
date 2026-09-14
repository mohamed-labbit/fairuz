const { test } = require('node:test');
const assert = require('node:assert/strict');
const Module = require('node:module');

let formatter, implementation;
const commands = new Map(), warnings = [], executed = [];
const disposable = { dispose() {} };
const vscode = {
  workspace: { isTrusted: true },
  window: { showWarningMessage: message => warnings.push(message) },
  commands: {
    registerCommand: (name, callback) => { commands.set(name, callback); return disposable; },
    executeCommand: name => executed.push(name)
  },
  languages: { registerDocumentFormattingEditProvider: (_, provider) => { formatter = provider; return disposable; } },
  TextEdit: { replace: (range, text) => ({ range, text }) },
  Range: class { constructor(start, end) { Object.assign(this, { start, end }); } }
};
const originalLoad = Module._load;
Module._load = function(name, ...args) {
  if (name === 'vscode') return vscode;
  if (name === './compilerService') return { formatSource: (...values) => implementation(...values) };
  if (name === './rtleditorprovider') return { FairuzRtlEditorProvider: { register: () => ({ runCommand: () => false }) } };
  if (name === './semanticHighlighter') return {
    FairuzSemanticHighlighter: class { executable() { return 'fairuz'; } },
    registerDocumentSemanticTokens: () => disposable
  };
  return originalLoad.call(this, name, ...args);
};
const { activate } = require('../src/extension');
Module._load = originalLoad;
activate({ subscriptions: [] });

function document(text = 'ن:=1\n') {
  return { version: 1, getText: () => text, positionAt: offset => offset };
}

test('native formatting replaces unsaved text with compiler output', async () => {
  implementation = async (executable, source) => {
    assert.equal(executable, 'fairuz');
    assert.equal(source, 'ن:=1\n');
    return { text: 'ن := 1\n' };
  };
  const edits = await formatter.provideDocumentFormattingEdits(document(), {}, {});
  assert.equal(edits[0].text, 'ن := 1\n');
});

test('native formatting never overwrites text edited while the compiler runs', async () => {
  let resolve;
  implementation = () => new Promise(done => { resolve = done; });
  const doc = document();
  const pending = formatter.provideDocumentFormattingEdits(doc, {}, {});
  doc.version++;
  resolve({ text: 'stale' });
  assert.deepEqual(await pending, []);
});

test('native formatting handles cancellation and errors without edits', async () => {
  const token = { isCancellationRequested: false };
  implementation = async () => { token.isCancellationRequested = true; return { text: 'stale' }; };
  assert.deepEqual(await formatter.provideDocumentFormattingEdits(document(), {}, token), []);
  implementation = async () => ({ error: 'Invalid source' });
  assert.deepEqual(await formatter.provideDocumentFormattingEdits(document(), {}, {}), []);
  assert.equal(warnings.at(-1), 'Invalid source');
});

test('untrusted workspaces cannot execute the formatter', async () => {
  vscode.workspace.isTrusted = false;
  implementation = () => { throw new Error('must not execute'); };
  try {
    assert.deepEqual(await formatter.provideDocumentFormattingEdits(document(), {}, {}), []);
  } finally { vscode.workspace.isTrusted = true; }
});

test('Fairuz format command also works outside the RTL editor', async () => {
  await commands.get('fairuz.format')();
  assert.equal(executed.at(-1), 'editor.action.formatDocument');
});
