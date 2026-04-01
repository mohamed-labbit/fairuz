# Fairuz VS Code Support

This extension provides:

- syntax highlighting for Fairuz source files
- `.fa` file association
- a custom RTL editor for Fairuz source that renders code right-to-left

## Default File Extension

Fairuz source files use the `.fa` extension.

## Local Installation

From this directory:

```bash
vsce package
code --install-extension fairuz-language-0.1.0.vsix
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
