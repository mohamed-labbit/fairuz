const fs = require("fs/promises");
const os = require("os");
const path = require("path");
const { spawn } = require("child_process");

(async () => {
  const temporary = await fs.mkdtemp(path.join(os.tmpdir(), "fairuz-vscode-test-"));
  const executable = process.env.VSCODE_EXECUTABLE || (process.platform === "darwin"
    ? "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code" : "code");
  const extension = path.resolve(__dirname, "..");
  const resultPath = path.join(temporary, "result.json");
  const child = spawn(executable, ["--new-window", "--skip-welcome", "--skip-release-notes", "--disable-extensions",
    `--user-data-dir=${path.join(temporary, "profile")}`, `--extensions-dir=${path.join(temporary, "extensions")}`,
    `--extensionDevelopmentPath=${extension}`, `--extensionTestsPath=${path.join(extension, "test", "extension-host.cjs")}`
  ], { stdio: "inherit", env: { ...process.env, FAIRUZ_HOST_TEST_RESULT: resultPath } });
  let launchError;
  child.on("error", error => { launchError = error; });
  child.on("close", code => {
    if (code) launchError = new Error(`VS Code exited with code ${code}`);
  });
  // The macOS CLI can exit before the extension host. Require its explicit result.
  const deadline = Date.now() + 90000;
  console.log(`VS Code test profile: ${temporary}`);
  while (Date.now() < deadline) {
    if (launchError) throw launchError;
    try {
      const result = JSON.parse(await fs.readFile(resultPath, "utf8"));
      if (!result.passed) throw new Error(result.error || "Host integration tests failed");
      console.log(result.message);
      return;
    } catch (error) {
      if (error.code !== "ENOENT" && !(error instanceof SyntaxError)) throw error;
    }
    await new Promise(resolve => setTimeout(resolve, 250));
  }
  child.kill();
  throw new Error("Timed out waiting for the extension host test result");
})().catch(error => { console.error(error); process.exitCode = 1; });
