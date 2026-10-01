#!/usr/bin/env python3
"""Run selected complete documentation examples with the local interpreter."""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lang", choices=("en", "ar"), default="en")
LANG = parser.parse_args().lang
DOC_ROOT = ROOT / ("docs_ar" if LANG == "ar" else "docs")
EXE = ROOT / "build" / "fairuz"
CASES = {
    "getting-started/first-program.md": ["مرحبا فيروز\n"],
    "tutorial/index.md": ["9\nصحيح\n", "تم\n", "4\n", "120\n"],
    "language/expressions.md": ["14\n0.25\n"],
    "language/statements.md": ["تم\n8\n", "1\n3\n", "2\n"],
    "language/functions.md": ["", "7\n1\n"],
    "language/errors.md": ["ok\n"],
    "runtime/memory-management.md": ["2\n"],
    "standard-library/builtins.md": ["في\n"],
}

if not EXE.is_file():
    raise SystemExit(f"Build the interpreter first: missing {EXE}")

count = 0
for relative, expected_outputs in CASES.items():
    source = (DOC_ROOT / relative).read_text(encoding="utf-8")
    examples = re.findall(r"```fa\n(.*?)\n```", source, re.S)
    if len(examples) != len(expected_outputs):
        raise SystemExit(f"{relative}: expected {len(expected_outputs)} examples, found {len(examples)}")
    for index, (program, expected) in enumerate(zip(examples, expected_outputs), 1):
        with tempfile.TemporaryDirectory(prefix="fairuz-doc-") as directory:
            path = Path(directory) / "example.fa"
            path.write_text(program + "\n", encoding="utf-8")
            result = subprocess.run([str(EXE), str(path)], cwd=ROOT, capture_output=True, text=True)
        if result.returncode or result.stdout != expected:
            raise SystemExit(
                f"{relative} example {index}: exit={result.returncode}, "
                f"stdout={result.stdout!r}, stderr={result.stderr!r}; expected {expected!r}"
            )
        count += 1

module_count = 0
for page in sorted((DOC_ROOT / "standard-library" / "modules").glob("*.md")):
    examples = re.findall(r"```fa\n(.*?)\n```", page.read_text(encoding="utf-8"), re.S)
    if len(examples) != 1:
        raise SystemExit(f"{page}: expected one module lookup example, found {len(examples)}")
    with tempfile.TemporaryDirectory(prefix="fairuz-doc-module-") as directory:
        path = Path(directory) / "example.fa"
        path.write_text(examples[0] + "\n", encoding="utf-8")
        result = subprocess.run([str(EXE), str(path)], cwd=ROOT, capture_output=True, text=True, timeout=15)
    if result.returncode or not result.stdout.strip():
        raise SystemExit(f"{page}: exit={result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}")
    module_count += 1
print(f"Validated {count} guided examples and {module_count} module lookup examples.")
