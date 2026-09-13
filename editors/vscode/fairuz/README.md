# Fairuz VS Code Support

This extension provides:

- parser-backed semantic highlighting for Fairuz source files
- resilient lexical highlighting while code is incomplete
- `.fa` file association
- a custom bidi-aware RTL editor with normal editing, selection, undo, find,
  folding, bracket pairing, and semantic highlighting

The highlighter uses Fairuz's own parser and AST. It distinguishes classes,
functions, methods, parameters, properties, variables, namespaces,
declarations, keywords, literals, operators, and comments. Source positions
are converted to UTF-16 so Arabic and supplementary Unicode characters align
correctly in VS Code and the bidi-aware RTL editor.

## Default File Extension

Fairuz source files use the `.fa` extension.

## Local Installation

From this directory:

```bash
npm install
npm run package
code --install-extension fairuz-language-0.2.2.vsix
```

Build Fairuz first (`bash build.sh build`) when developing from the repository.
For an installed extension, ensure `fairuz` is on `PATH`, or configure an
absolute executable path:

```json
{
  "fairuz.executablePath": "/absolute/path/to/fairuz"
}
```

The executable exposes the editor protocol as JSON through either a file or
standard input:

```bash
fairuz --semantic-tokens source.fa
fairuz --semantic-tokens - < source.fa
```

## Using The RTL Editor

VS Code does not allow extensions to replace the core text editor rendering engine directly. This extension works around that by providing a custom editor for Fairuz files.

After installing:

1. Open any `.fa` file
2. Run `Fairuz: Open RTL Editor` from the Command Palette

Or:

1. Right-click the tab
2. Choose `Reopen Editor With...`
3. Select `Fairuz RTL Editor`

If you want `.fa` files to always open in the RTL editor, set this in your VS Code `settings.json`:

```json
{
  "workbench.editorAssociations": {
    "*.fa": "fairuz.rtlEditor"
  }
}
```

Undo and redo use the normal platform shortcuts: `Cmd+Z` / `Cmd+Shift+Z` on
macOS and `Ctrl+Z` / `Ctrl+Y` (or `Ctrl+Shift+Z`) on Windows and Linux. They
are also available as `Fairuz: Undo` and `Fairuz: Redo` in the Command Palette
and in the editor's context menu.
