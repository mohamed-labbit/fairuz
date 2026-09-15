const { test } = require('node:test');
const assert = require('node:assert/strict');
const Module = require('node:module');
const { DocumentSync, normalize } = require('../src/documentSync');
let formatImplementation;

const disposable = { dispose() {} };
class Position { constructor(line, character) { Object.assign(this, { line, character }); } }
class Range {
  constructor(a, b, c, d) {
    this.start = typeof a === 'number' ? new Position(a, b) : a;
    this.end = typeof a === 'number' ? new Position(c, d) : b;
  }
}
class WorkspaceEdit {
  constructor() { this.edits = []; }
  replace(uri, range, text) { this.edits.push({ uri, range, text }); }
}
const mock = {
  CancellationTokenSource: class {
    constructor() { this.token = { isCancellationRequested: false }; }
    cancel() { this.token.isCancellationRequested = true; }
    dispose() {}
  },
  Position, Range, WorkspaceEdit, EndOfLine: { CRLF: 2 },
  languages: { createDiagnosticCollection: () => ({ delete() {}, set() {}, dispose() {} }) },
  tasks: { onDidEndTask: () => disposable },
  workspace: { isTrusted: true, applyEdit: async () => true }
};
const load = Module._load;
Module._load = function(name, ...args) {
  if (name === 'vscode') return mock;
  if (name === './compilerService') return {
    ...load.call(this, name, ...args),
    formatSource: (...values) => formatImplementation(...values)
  };
  return load.call(this, name, ...args);
};
const { FairuzRtlEditorProvider } = require('../src/rtleditorprovider');
Module._load = load;

function harness(text = '', eol = 1) {
  const messages = [];
  const provider = new FairuzRtlEditorProvider({}, {});
  const document = { uri: { toString: () => 'file:///test.ف' }, version: 1, eol, getText: () => text };
  const state = { document, webviewPanel: { webview: { postMessage: message => messages.push(message) } }, ready: true };
  mock.workspace.applyEdit = async edit => {
    const { range, text: replacement } = edit.edits[0];
    const offset = position => {
      const parts = text.split('\n');
      return parts.slice(0, position.line).reduce((n, part) => n + part.length + 1, 0) + position.character;
    };
    const from = offset(range.start), to = offset(range.end);
    text = text.slice(0, from) + replacement + text.slice(to);
    document.version += 3; // versions need not increase by one
    return true;
  };
  return { provider, state, messages, text: () => text };
}

test('RTL formatting returns compiler output without directly replacing host text', async () => {
  const h = harness('ن:=1\n');
  h.provider.highlighter.executable = () => 'fairuz';
  formatImplementation = async (_, source) => {
    assert.equal(source, 'ن:=1\n');
    return { text: 'ن := 1\n' };
  };
  await h.provider.handleMessage(h.state, { type: 'format', requestId: 1, text: h.text() });
  assert.equal(h.messages[0].type, 'formatted');
  assert.equal(h.messages[0].text, 'ن := 1\n');
  assert.equal(h.text(), 'ن:=1\n'); // Webview applies one undoable, versioned edit.
});

test('RTL formatting drops superseded responses and respects workspace trust', async () => {
  const h = harness('ن:=1\n');
  h.provider.highlighter.executable = () => 'fairuz';
  const complete = [];
  formatImplementation = () => new Promise(resolve => complete.push(resolve));
  const first = h.provider.handleMessage(h.state, { type: 'format', requestId: 1, text: h.text() });
  const second = h.provider.handleMessage(h.state, { type: 'format', requestId: 2, text: h.text() });
  complete[1]({ text: 'new' });
  await second;
  complete[0]({ text: 'old' });
  await first;
  assert.deepEqual(h.messages.map(message => message.requestId), [2]);
  mock.workspace.isTrusted = false;
  formatImplementation = () => { throw new Error('must not execute'); };
  try {
    await h.provider.handleMessage(h.state, { type: 'format', requestId: 3, text: h.text() });
    assert.match(h.messages.at(-1).error, /Trust/);
  } finally { mock.workspace.isTrusted = true; }
});

test('provider and webview converge through delayed acknowledgements and burst edits', async () => {
  const h = harness('اكتب(1)\n');
  const outbound = [], sync = new DocumentSync(message => outbound.push(message));
  sync.receive(h.text(), 1);
  sync.edit('اكتب(2)\n'); sync.edit('اكتب(22)\n'); sync.edit('اكتب(222)\n');
  let index = 0;
  while (index < outbound.length) {
    await h.provider.handleMessage(h.state, outbound[index++]);
    sync.acknowledge(h.messages.shift());
  }
  assert.equal(h.text(), 'اكتب(222)\n');
  assert.equal(sync.pending, false);
});
test('CRLF documents preserve line endings while mapping LF browser offsets', async () => {
  const h = harness('a\r\nب\r\nc', 2);
  await h.provider.handleMessage(h.state, { type: 'edit', id: 1, baseVersion: 1, change: { from: 2, to: 3, insert: '😀\nج' } });
  assert.equal(h.text(), 'a\r\n😀\r\nج\r\nc');
  assert.equal(h.messages[0].type, 'ack');
});
test('stale or malformed edits cannot replace host content', async () => {
  const h = harness('external');
  await h.provider.handleMessage(h.state, { type: 'edit', id: 1, baseVersion: 0, change: { from: 0, to: 8, insert: 'stale' } });
  assert.equal(h.text(), 'external');
  assert.equal(h.messages[0].rejected, true);
  await h.provider.handleMessage(h.state, { type: 'edit', id: 2, baseVersion: 1, change: { from: -1, to: 8, insert: 'bad' } });
  assert.equal(h.text(), 'external');
});
test('failed application sends a recovery snapshot', async () => {
  const h = harness('original');
  mock.workspace.applyEdit = async () => false;
  await h.provider.handleMessage(h.state, { type: 'edit', id: 1, baseVersion: 1, change: { from: 0, to: 8, insert: 'local' } });
  assert.equal(h.messages[0].text, 'original');
  assert.equal(h.messages[0].rejected, true);
});
test('invisible editors still receive external changes', () => {
  const h = harness('updated');
  h.state.webviewPanel.visible = false;
  h.provider.sendFullSync(h.state);
  assert.equal(h.messages[0].text, 'updated');
});
test('commands target only the active editor', () => {
  const h = harness();
  h.state.webviewPanel.visible = true;
  h.provider.editors.set('test', h.state);
  assert.equal(h.provider.runCommand('undo'), false);
  h.state.webviewPanel.active = true;
  assert.equal(h.provider.runCommand('undo'), true);
  assert.equal(h.messages[0].action, 'undo');
  assert.equal(h.provider.runCommand('goToLine'), true);
  assert.deepEqual(h.messages[1], { type: 'command', action: 'goToLine' });
});
test('save refuses a stale snapshot and waits for document.save', async () => {
  const h = harness('saved');
  let saved = false;
  h.state.document.save = async () => { saved = true; return true; };
  h.provider.sendMetadata = () => {};
  await h.provider.handleMessage(h.state, { type: 'action', action: 'save', version: 0 });
  assert.equal(saved, false);
  await h.provider.handleMessage(h.state, { type: 'action', action: 'save', version: 1 });
  assert.equal(saved, true);
});
test('workspace trust gates program execution', async () => {
  const h = harness();
  mock.workspace.isTrusted = false;
  await assert.rejects(() => h.provider.runDocument(h.state), /Trust/);
  mock.workspace.isTrusted = true;
});

test('Run saves first and passes paths directly to a VS Code process task', async () => {
  const h = harness();
  const events = [];
  h.state.document.uri = { fsPath: '/tmp/a folder/تجربة;literal.ف', scheme: 'file', toString: () => 'file:///test' };
  h.state.document.save = async () => { events.push('save'); return true; };
  h.provider.highlighter = { executable: () => '/tmp/compiler folder/fairuz' };
  mock.workspace.getWorkspaceFolder = () => undefined;
  mock.TaskScope = { Global: 1 };
  mock.TaskRevealKind = { Always: 1 };
  mock.TaskPanelKind = { Dedicated: 2 };
  mock.ProcessExecution = class { constructor(process, args, options) { Object.assign(this, { process, args, options }); } };
  mock.Task = class { constructor(definition, scope, name, source, execution) { Object.assign(this, { definition, scope, name, source, execution }); } };
  let task;
  const execution = { terminate() { events.push('stop'); } };
  mock.tasks.executeTask = async value => { task = value; events.push('run'); return execution; };
  await h.provider.runDocument(h.state);
  assert.deepEqual(events, ['save', 'run']);
  assert.deepEqual(task.execution.args, ['/tmp/a folder/تجربة;literal.ف']);
  assert.equal(task.execution.process, '/tmp/compiler folder/fairuz');
  assert.equal(task.execution.options.cwd, '/tmp/a folder');
  await h.provider.handleMessage(h.state, { type: 'action', action: 'stop' });
  assert.deepEqual(events, ['save', 'run', 'stop']);
});
