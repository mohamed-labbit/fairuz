// Run through VS Code's --extensionTestsPath, not node --test.
const assert = require("node:assert/strict");
const fs = require("node:fs/promises");
const os = require("node:os");
const path = require("node:path");
const vscode = require("vscode");
const { FairuzRtlEditorProvider } = require("../src/rtleditorprovider");
const { DocumentSync } = require("../src/documentSync");

async function run() {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), "fairuz-host-fixture-"));
  const uri = vscode.Uri.file(path.join(directory, "تجربة.fa"));
  const initial = '# عنوان\r\nاكتب("مرحبا")\r\n';
  await fs.writeFile(uri.fsPath, initial);
  const document = await vscode.workspace.openTextDocument(uri);
  const context = { extensionPath: path.resolve(__dirname, ".."), extensionUri: vscode.Uri.file(path.resolve(__dirname, "..")) };
  const provider = new FairuzRtlEditorProvider(context, { highlight: async () => null });
  const received = [], outbound = [];
  const sync = new DocumentSync(message => outbound.push(message));
  const emitters = [new vscode.EventEmitter(), new vscode.EventEmitter(), new vscode.EventEmitter()];
  const panel = {
    active: true, visible: true,
    onDidChangeViewState: emitters[0].event, onDidDispose: emitters[1].event,
    webview: {
      cspSource: "https://test.invalid", asWebviewUri: resource => resource,
      onDidReceiveMessage: emitters[2].event,
      postMessage: async message => { received.push(message); return true; }
    }
  };
  const deliver = () => {
    while (received.length) {
      const message = received.shift();
      if (message.type === "ack") sync.acknowledge(message);
      else if (message.type === "setText") sync.receive(message.text, message.version, message.rejected, message.authoritative, message.recoveryId);
    }
  };
  try {
    const extension = vscode.extensions.all.find(item => item.extensionPath === context.extensionPath);
    assert.ok(extension, "development extension is loaded");
    await extension.activate();
    const commands = await vscode.commands.getCommands(true);
    for (const command of ["fairuz.save", "fairuz.run", "fairuz.outline", "fairuz.undo"]) assert.ok(commands.includes(command));
    await provider.resolveCustomTextEditor(document, panel);
    const state = [...provider.editors.values()][0];
    await provider.handleMessage(state, { type: "ready" });
    deliver();
    assert.equal(document.eol, vscode.EndOfLine.CRLF);
    sync.edit('# عنوان\nاكتب("😀")\n');
    sync.edit('# عنوان\nاكتب("😀 فيروز")\n');
    let consumed = 0;
    while (consumed < outbound.length) {
      const edit = outbound[consumed++];
      emitters[2].fire(edit);
      emitters[2].fire({ type: "sync", recoveryId: edit.id });
      await state.queue;
      assert.ok(received.some(message => message.authoritative && message.recoveryId === edit.id
        && message.text === document.getText().replace(/\r\n/g, "\n")));
      deliver();
    }
    assert.equal(sync.pending, false);
    assert.equal(document.getText(), '# عنوان\r\nاكتب("😀 فيروز")\r\n');
    await provider.handleMessage(state, { type: "action", action: "save", version: document.version });
    assert.equal(await fs.readFile(uri.fsPath, "utf8"), document.getText());
    assert.equal(document.isDirty, false);

    // Simulate an external edit while a local change has not reached the host.
    sync.edit(sync.local + '# محلي\n');
    const external = new vscode.WorkspaceEdit();
    external.insert(uri, new vscode.Position(0, 0), '# خارجي\r\n');
    assert.equal(await vscode.workspace.applyEdit(external), true);
    // sendFullSync also exercises recovery in case host events arrive later.
    emitters[2].fire({ type: "sync", recoveryId: sync.flight?.id ?? null });
    await state.queue;
    deliver();
    assert.ok(sync.conflict);
    assert.ok(sync.local.includes('محلي'));
    assert.ok(document.getText().includes('خارجي'));
    sync.resolve(false);
    assert.equal(sync.pending, false);
    assert.equal(sync.local.replace(/\n/g, "\r\n"), document.getText());
    await document.save();
    console.log("FAIRUZ_HOST_TESTS_PASSED: activation, commands, real WorkspaceEdit, CRLF, rapid edits, save, external conflict, recovery");
  } finally {
    provider.dispose();
    for (const emitter of emitters) emitter.dispose();
    await fs.rm(directory, { recursive: true, force: true });
  }
}

exports.run = async () => {
  const resultPath = process.env.FAIRUZ_HOST_TEST_RESULT;
  try {
    await run();
    if (resultPath) await fs.writeFile(resultPath, JSON.stringify({ passed: true,
      message: "Host integration tests passed: activation, commands, WorkspaceEdit, CRLF, rapid edits, save, external conflict, recovery" }));
  } catch (error) {
    if (resultPath) await fs.writeFile(resultPath, JSON.stringify({ passed: false, error: error.stack || String(error) }));
    throw error;
  }
};
