# Fairuz RTL Editor

A comfortable Arabic editing surface for Fairuz, with native bidirectional text
layout, a right-side gutter, and parser-backed syntax colors. Version **0.3.2**.

## Daily editing

- Arabic toolbar with Save, Run/Stop, Format, Find/Replace, and word wrapping.
- Arabic keyword and builtin completion, document words, and function/condition/loop snippets.
- Four-space block indentation, Tab/Shift+Tab, line comments, and indentation-based folding.
- Undo/redo, multiple selections, bracket matching, search, and replacement.
- Light, dark, and high-contrast colors that follow VS Code's active theme.
- Cursor position, unsaved indicator, and clickable compiler errors.
- Saved cursor/scroll state when the webview is restored.
- Go to a line or `line:column` with Ctrl+G or by clicking the cursor status.
  Accepts Western and Arabic digits (for example, `١٢:٣`). Columns count Unicode
  code points, matching the status bar; columns past the line end stop at its end.
- Searchable function, type, and method navigator with keyboard selection.
  It reads the current buffer and remains useful while code is incomplete.

The editor uses CodeMirror's bidi layout rather than mirroring an LTR editor.
Mixed Arabic/Latin identifiers, numbers, selection, and caret movement remain
in logical source order. No external fonts or scripts are downloaded.

## Compiler integration

Build Fairuz first, or put an installed `fairuz` executable on `PATH`.
Alternatively, set `fairuz.executablePath` to its absolute path. The extension
also discovers `build/fairuz` in an open workspace.

Semantic colors use `fairuz --semantic-tokens -`. If the compiler is unavailable,
lexical colors remain available. Parser requests are debounced and stale
responses are ignored; existing decorations are mapped while typing.

Live checking runs `fairuz --check` on a private temporary copy of the unsaved
buffer after a 700 ms pause. It checks syntax and bytecode compilation; it does
not execute the program or validate runtime behavior. Diagnostics appear in
the RTL editor and VS Code's Problems collection. Clicking the status opens an
error list with jump-to-location buttons. Temporary copies are removed after
each request. Compiler requests have cancellation, timeouts and output limits.

Run saves the synchronized document and starts Fairuz in a VS Code task terminal,
with the file's directory as the working directory. The terminal supports input;
Stop terminates that task. Arguments are passed directly to the executable,
without shell interpolation. Compiler processes and Run require workspace trust.

## Saving and external edits

Only one edit is outstanding at a time. Further typing is buffered and sent after
the host acknowledges its actual document version. Save and Run wait for that
queue. CRLF files retain their line endings.

The buffer stays read-only until the initial file arrives. If an acknowledgement
is missing for five seconds, the editor requests the host's current document
after its edit queue drains, then resumes syncing. The status bar shows progress
and offers a manual retry; pending text stays in the buffer throughout recovery.

When external changes conflict with pending typing, the editor keeps the local
buffer visible and offers **الاحتفاظ بكتابتي** (keep my writing) or
**استخدام نسخة الملف** (use the file version). Keeping local writing replaces the
external version with the local buffer; choosing the file version is undoable.
Pending recovery text is also retained in the webview's VS Code state.

## Formatting

**Format Document** uses the same source-preserving formatter as `fairuz format`.
It normalizes block indentation to four spaces, operator and delimiter spacing,
Arabic commas, and the final newline. Comments, string and number spellings,
grouping, existing line breaks, and LF/CRLF line endings are preserved. Long
lines are left intact rather than split at unsafe positions.

Formatting requires a trusted workspace and an updated Fairuz executable.
Unsaved text is formatted in a temporary file; invalid source leaves the editor
unchanged. Both the normal VS Code editor and RTL editor discard stale results
if the document changes during formatting. RTL formatting is undoable.

## Keyboard shortcuts

| Action | macOS | Windows / Linux |
| --- | --- | --- |
| Save | Cmd+S | Ctrl+S |
| Run | Cmd+Enter | Ctrl+Enter |
| Undo / redo | Cmd+Z / Cmd+Shift+Z | Ctrl+Z / Ctrl+Y |
| Find | Cmd+F | Ctrl+F |
| Go to symbol | Cmd+Shift+O | Ctrl+Shift+O |
| Go to line / column | Ctrl+G | Ctrl+G |
| Replace | Cmd+Alt+F | Ctrl+H |
| Format document | Shift+Alt+F | Shift+Alt+F |
| Completion | Ctrl+Space | Ctrl+Space |
| Comment line | Cmd+/ | Ctrl+/ |
| Toggle wrapping | Alt+Z | Alt+Z |
| Indent / outdent | Tab / Shift+Tab | Tab / Shift+Tab |

Tab moves through an active snippet or accepts an open completion before
indenting. Press Escape, then Tab to move focus out of the editing surface.
All main actions are also available in the Command Palette under **Fairuz**.

## Settings

| Setting | Default | Purpose |
| --- | --- | --- |
| `fairuz.executablePath` | automatic | Compiler executable |
| `fairuz.fontSize` | 15 | Editor text size, 10–32 px |
| `fairuz.lineHeight` | 1.8 | Line spacing multiplier |
| `fairuz.tabSize` | 4 | Spaces per indentation level |
| `fairuz.wordWrap` | true | Wrap long lines |
| `fairuz.liveDiagnostics` | true | Check source after typing pauses |

## Build and install

```bash
npm ci
npm test
npm run package
code --install-extension fairuz-language-0.3.2.vsix --force
```

After updating, run **Developer: Reload Window** in VS Code. Open a `.ف` file,
or use **Reopen Editor With… → Fairuz RTL Editor**. Use
**Reopen Editor With… → Text Editor** for the standard VS Code editor.

## Development checks

```bash
npm test                   # Sync, host bridge, formatter and process regressions
npm run test:compiler      # Also checks valid/invalid buffers with build/fairuz
npm run test:host          # Real VS Code document APIs in an isolated test profile
npm run build:webview      # Rebuild the browser bundle
```

`test/rtl-editor-harness.html` is a standalone browser harness with delayed host
acknowledgements, dropped-ack recovery, external-edit injection, diagnostic fixtures, and theme
switching. Serve this extension directory over localhost to use it. Its Run
button simulates the host request; real execution is provided by VS Code tasks.
The harness and tests are excluded from the VSIX.

The host test uses the installed VS Code application (override its CLI with
`VSCODE_EXECUTABLE`). It checks activation, commands, real WorkspaceEdit updates,
CRLF, rapid typing, saving, and external conflicts. The panel transport is a
fixture; this does not replace manual testing of the full desktop webview.
