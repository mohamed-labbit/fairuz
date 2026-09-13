const path = require("path");
const esbuild = require("esbuild");

esbuild.buildSync({
  entryPoints: [path.join(__dirname, "..", "src", "webviewEditor.js")],
  outfile: path.join(__dirname, "..", "media", "editor.js"),
  bundle: true,
  format: "iife",
  platform: "browser",
  target: ["es2020"],
  minify: true,
  legalComments: "none"
});

console.log("[build-editor] bundled CodeMirror RTL editor");
