const fs = require("fs/promises");
const os = require("os");
const path = require("path");
const { spawn } = require("child_process");
const { parseDiagnostics } = require("./languageTools");

function runCompiler(executable, args, { input, cwd, token, timeout = 5000 } = {}) {
  return new Promise(resolve => {
    if (token?.isCancellationRequested) return resolve({ cancelled: true });
    let child, timer, subscription, settled = false, stdout = "", stderr = "";
    const finish = result => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      subscription?.dispose();
      resolve(result);
    };
    const stop = result => { child?.kill(); finish(result); };
    try {
      child = spawn(executable, args, {
        cwd, windowsHide: true, stdio: ["pipe", "pipe", "pipe"],
        env: { ...process.env, NO_COLOR: "1", ASAN_OPTIONS: `${process.env.ASAN_OPTIONS || ""}:detect_leaks=0` }
      });
    } catch (error) { finish({ error: error.message }); return; }
    timer = setTimeout(() => stop({ error: "Fairuz took too long to respond." }), timeout);
    subscription = token?.onCancellationRequested(() => stop({ cancelled: true }));
    for (const stream of [child.stdout, child.stderr]) stream.setEncoding("utf8");
    child.stdout.on("data", chunk => {
      stdout += chunk;
      if (stdout.length > 16 * 1024 * 1024) stop({ error: "Fairuz output exceeded the editor limit." });
    });
    child.stderr.on("data", chunk => {
      stderr += chunk;
      if (stderr.length > 1024 * 1024) stop({ error: "Fairuz diagnostics exceeded the editor limit." });
    });
    child.on("error", error => finish({ error: error.message }));
    child.on("close", code => finish({ code, stdout, stderr }));
    // A compiler may exit before consuming stdin. EPIPE must not crash the host.
    child.stdin.on("error", () => {});
    child.stdin.end(input || "", "utf8");
  });
}

async function checkSource(executable, source, options = {}) {
  if (Buffer.byteLength(source, "utf8") > 8 * 1024 * 1024)
    return { error: "This file is too large for live checking.", diagnostics: [] };
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), "fairuz-editor-"));
  try {
    const file = path.join(directory, "buffer.ف");
    await fs.writeFile(file, source, { mode: 0o600 });
    const result = await runCompiler(executable, ["--check", file], options);
    if (result.cancelled || result.error) return { ...result, diagnostics: [] };
    const diagnostics = parseDiagnostics(result.stderr, source);
    if (result.code !== 0 && !diagnostics.length)
      return { error: result.stderr.trim() || "Fairuz could not check this file.", diagnostics: [] };
    return { diagnostics };
  } finally {
    await fs.rm(directory, { recursive: true, force: true });
  }
}

async function formatSource(executable, source, options = {}) {
  if (Buffer.byteLength(source, "utf8") > 8 * 1024 * 1024)
    return { error: "This file is too large for editor formatting." };
  let directory;
  try {
    directory = await fs.mkdtemp(path.join(os.tmpdir(), "fairuz-format-"));
    const file = path.join(directory, "buffer.ف");
    await fs.writeFile(file, source, { mode: 0o600 });
    const result = await runCompiler(executable, ["format", file], options);
    if (result.cancelled || result.error) return result;
    if (result.code !== 0)
      return { error: result.stderr.trim() || "Fairuz could not format this file. The document was not changed." };
    return { text: await fs.readFile(file, "utf8") };
  } catch (error) {
    return { error: error.message };
  } finally {
    if (directory) await fs.rm(directory, { recursive: true, force: true });
  }
}

module.exports = { runCompiler, checkSource, formatSource };
