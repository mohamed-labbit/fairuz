// One outstanding edit at a time. Host versions are opaque acknowledgements,
// never predicted from the number of keystrokes in the browser.
function normalize(text) { return text.replace(/\r\n?/g, "\n"); }

function textChange(before, after) {
  let from = 0;
  while (from < before.length && from < after.length && before[from] === after[from]) from++;
  let end = before.length, nextEnd = after.length;
  while (end > from && nextEnd > from && before[end - 1] === after[nextEnd - 1]) { end--; nextEnd--; }
  // Never split a surrogate pair, even when only its low surrogate changed.
  if (from && /[\uD800-\uDBFF]/.test(before[from - 1])) from--;
  return { from, to: end, insert: after.slice(from, nextEnd) };
}

class DocumentSync {
  constructor(send) {
    this.send = send;
    this.version = null;
    this.base = "";
    this.local = "";
    this.flight = null;
    this.conflict = null;
    this.sequence = 0;
  }
  get pending() { return !!this.flight || this.local !== this.base; }
  edit(text) { this.local = normalize(text); this.flush(); }
  flush() {
    if (this.version === null || this.flight || this.conflict || this.local === this.base) return;
    const id = ++this.sequence;
    const change = textChange(this.base, this.local);
    this.flight = { id, text: this.local };
    this.send({ type: "edit", id, baseVersion: this.version, change });
  }
  acknowledge(message) {
    if (!this.flight || message.id !== this.flight.id
        || !Number.isSafeInteger(message.version) || message.version < this.version) return;
    this.base = this.flight.text;
    this.version = message.version;
    this.flight = null;
    this.flush();
  }
  receive(text, version, rejected = false, authoritative = false, recoveryId = null) {
    text = normalize(text);
    if (this.version !== null && version < this.version) return { ignored: true };
    // An ack may start the next buffered edit before this recovery reply arrives.
    // A snapshot for the previous flight must never retry or settle that new edit.
    if (authoritative && recoveryId !== (this.flight?.id ?? null)) return { ignored: true };
    if (this.version === null) {
      this.base = this.local = text;
      this.version = version;
      return { text };
    }
    if (this.conflict) {
      this.conflict = { text, version };
      return { conflict: true };
    }
    if (!rejected && this.flight && text === this.flight.text) {
      this.acknowledge({ id: this.flight.id, version });
      return { ignored: true };
    }
    // A recovery snapshot is sent after the host has drained its edit queue.
    // If our edit was never applied it is safe to retry against this version.
    if (authoritative && !rejected && text === this.base) {
      this.flight = null;
      this.version = version;
      this.flush();
      return { ignored: true };
    }
    if (!rejected && text === this.base) {
      this.version = version;
      return { ignored: true };
    }
    if (this.pending && text !== this.local) {
      this.flight = null;
      this.conflict = { text, version };
      return { conflict: true };
    }
    this.flight = null;
    this.base = this.local = text;
    this.version = version;
    return { text };
  }
  resolve(keepLocal) {
    if (!this.conflict) return;
    this.base = this.conflict.text;
    this.version = this.conflict.version;
    this.conflict = null;
    if (!keepLocal) this.local = this.base;
    this.flush();
    return this.local;
  }
}

module.exports = { DocumentSync, textChange, normalize };
