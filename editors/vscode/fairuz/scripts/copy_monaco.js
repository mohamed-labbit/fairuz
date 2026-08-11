// Copies node_modules/monaco-editor/min/vs into media/vs so the VSIX ships
// only the minified runtime Monaco needs at load time, not the full
// monaco-editor package (which also contains dev/esm builds, docs, etc).
// Run automatically via the "vscode:prepublish" npm script before packaging.
const fs = require("fs");
const path = require("path");

const SRC = path.join(__dirname, "..", "node_modules", "monaco-editor", "min", "vs");
const DEST = path.join(__dirname, "..", "media", "vs");

function copyRecursive(src, dest) {
  const stat = fs.statSync(src);
  if (stat.isDirectory()) {
    fs.mkdirSync(dest, { recursive: true });
    for (const entry of fs.readdirSync(src)) {
      copyRecursive(path.join(src, entry), path.join(dest, entry));
    }
  } else {
    fs.copyFileSync(src, dest);
  }
}

if (!fs.existsSync(SRC)) {
  console.error(`[copy-monaco] source not found: ${SRC}`);
  console.error(`[copy-monaco] did you run "npm install" first?`);
  process.exit(1);
}

fs.rmSync(DEST, { recursive: true, force: true });
copyRecursive(SRC, DEST);
console.log(`[copy-monaco] copied ${SRC} -> ${DEST}`);