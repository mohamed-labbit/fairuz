const { test } = require("node:test");
const assert = require("node:assert/strict");
const { DocumentSync, textChange } = require("../src/documentSync");

function setup(text = "", version = 1) {
  const messages = [], sync = new DocumentSync(message => messages.push(message));
  sync.receive(text, version);
  return { sync, messages };
}
test("rapid typing coalesces behind one acknowledged edit without predicting versions", () => {
  const { sync, messages } = setup();
  sync.edit("ف"); sync.edit("في"); sync.edit("فيروز");
  assert.equal(messages.length, 1);
  sync.acknowledge({ id: 1, version: 7 });
  assert.equal(messages.length, 2);
  assert.equal(messages[1].baseVersion, 7);
  assert.equal(messages[1].change.insert, "يروز");
  sync.acknowledge({ id: 1, version: 2 }); // duplicate, late acknowledgement
  assert.equal(sync.version, 7);
  sync.acknowledge({ id: 2, version: 12 });
  assert.equal(sync.pending, false);
  assert.equal(sync.base, "فيروز");
});
test("external changes during typing preserve both versions until resolution", () => {
  const { sync, messages } = setup("original");
  sync.edit("local");
  assert.deepEqual(sync.receive("external", 3), { conflict: true });
  assert.equal(sync.local, "local");
  sync.edit("local continued");
  assert.equal(messages.length, 1);
  sync.receive("new external", 4);
  sync.resolve(true);
  assert.equal(messages[1].baseVersion, 4);
  assert.equal(sync.local, "local continued");
});
test("discarding local conflict loads the chosen external version", () => {
  const { sync, messages } = setup("a");
  sync.edit("b"); sync.receive("c", 2, true);
  assert.equal(sync.resolve(false), "c");
  assert.equal(sync.pending, false);
  assert.equal(messages.length, 1);
});
test("failed host apply preserves unsent text, including at the same version", () => {
  const { sync } = setup("a", 1);
  sync.edit("ab");
  sync.receive("a", 1, true);
  assert.ok(sync.conflict);
  assert.equal(sync.local, "ab");
});
test("a matching document event can acknowledge before the promise settles", () => {
  const { sync, messages } = setup("a");
  sync.edit("ab"); sync.edit("abc");
  sync.receive("ab", 3);
  assert.equal(messages.length, 2);
  sync.acknowledge({ id: 1, version: 3 });
  sync.acknowledge({ id: 2, version: 5 });
  assert.equal(sync.local, sync.base);
});
test("older external events cannot move the document backwards", () => {
  const { sync } = setup("new", 8);
  sync.receive("old", 6);
  assert.equal(sync.local, "new");
});
test("a lost acknowledgement is recovered from the authoritative host snapshot", () => {
  const { sync, messages } = setup("a");
  sync.edit("ab"); sync.edit("abc");
  sync.receive("ab", 5, false, true, 1);
  assert.equal(messages.length, 2);
  assert.equal(messages[1].baseVersion, 5);
  sync.acknowledge({ id: 2, version: 6 });
  assert.equal(sync.pending, false);
  assert.equal(sync.base, "abc");
});
test("a lost edit is retried only after the host reports its drained queue", () => {
  const { sync, messages } = setup("a");
  sync.edit("ab");
  sync.receive("a", 1);
  assert.equal(messages.length, 1);
  sync.receive("a", 1, false, true, 1);
  assert.equal(messages.length, 2);
  assert.equal(messages[1].id, 2);
  sync.acknowledge({ id: 1, version: 2 });
  assert.equal(sync.flight.id, 2);
});
test("matching edit IDs cannot acknowledge a version older than the current snapshot", () => {
  const { sync } = setup("a", 5);
  sync.edit("b");
  sync.acknowledge({ id: 1, version: 4 });
  assert.equal(sync.version, 5);
  assert.equal(sync.pending, true);
});
test("a late recovery reply cannot retry a newer buffered edit", () => {
  const { sync, messages } = setup("a");
  sync.edit("ab"); sync.edit("abc");
  sync.acknowledge({ id: 1, version: 2 });
  sync.receive("ab", 2, false, true, 1);
  assert.equal(messages.length, 2);
  assert.equal(sync.flight.id, 2);
  sync.acknowledge({ id: 2, version: 3 });
  assert.equal(sync.pending, false);
  assert.equal(sync.base, "abc");
});
test("duplicate recovery replies cannot settle an edit that returns to old text", () => {
  const { sync } = setup("a");
  sync.edit("ab"); sync.edit("a");
  sync.receive("ab", 2, false, true, 1);
  sync.receive("a", 2, false, true, 1);
  assert.equal(sync.flight.id, 2);
  assert.equal(sync.pending, true);
  sync.acknowledge({ id: 2, version: 3 });
  assert.equal(sync.base, "a");
});
test("minimal changes round-trip Arabic, emoji, CRLF and replacements", () => {
  for (const [before, after] of [["😀", "😁"], ["اكتب(\"أ\")", "اكتب(\"ب\")"], ["a\nb", "a\n\n"], ["", "x"], ["x", ""], ["abc", "ab"]]) {
    const change = textChange(before, after);
    assert.equal(before.slice(0, change.from) + change.insert + before.slice(change.to), after);
  }
  const { sync } = setup("a\r\nb");
  assert.equal(sync.base, "a\nb");
});
test("random edit bursts and duplicate acknowledgements converge", () => {
  const { sync, messages } = setup();
  let host = "", version = 1, consumed = 0;
  for (let i = 0; i < 500; i++) {
    sync.edit(`${i} فيروز 😀\n${"x".repeat(i % 13)}`);
    if (i % 7 === 0) {
      const message = messages[consumed++];
      assert.equal(message.baseVersion, version);
      const c = message.change;
      host = host.slice(0, c.from) + c.insert + host.slice(c.to);
      version += 2;
      sync.acknowledge({ id: message.id, version });
      sync.acknowledge({ id: message.id, version: version - 1 });
    }
  }
  while (consumed < messages.length) {
    const message = messages[consumed++], c = message.change;
    assert.equal(message.baseVersion, version);
    host = host.slice(0, c.from) + c.insert + host.slice(c.to);
    sync.acknowledge({ id: message.id, version: ++version });
  }
  assert.equal(sync.local, host);
  assert.equal(sync.pending, false);
});
