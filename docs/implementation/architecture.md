# Architecture

```text
UTF-8 file → FileManager/SourceManager → Lexer → Parser/AST → Compiler/Chunk → VM
                                                                     ↓
                                                   Value/Object ← Builtins/stdlib
                                                                     ↓
                                                       GarbageCollector
```

Public headers live under `include/`; implementations live under `lib/`.
The lexer (`lib/Parser/lexer.cc`) produces tokens and indentation events using
the character table in `include/Util/ctype.hpp`. The parser
(`lib/Parser/parser.cc`) builds nodes defined in `include/Parser/ast.hpp`.
The compiler (`lib/Compiler/`) resolves names and emits the instructions defined
in `include/Compiler/opcode.hpp`. Bytecode and constants belong to `Chunk`.
The VM (`lib/Interpreter/`) executes chunks, returns runtime errors, and loads
modules.

`include/Interpreter/value.hpp`, `include/Interpreter/object.hpp`, and
`lib/Support/integer.cc` define runtime values, objects, and integer operations.
`lib/Interpreter/gc.cc` collects unreachable objects. Native functions are
registered in `lib/Interpreter/builtins/registry.cc` and implemented in that
directory. Modules written in Fairuz live under `stdlib/`, outside the C++ core.
The VM uses NaN-boxed values on supported platforms and a fallback representation.
Value layouts and the bytecode format are internal implementation details.

## Source size

Snapshot: **2026-10-19**, source commit `ad323255`.

The core engine consists of **79 tracked files and 21,026 physical source
lines** under `include/` and `lib/`. The VM/runtime alone, including native
builtins, accounts for **29 files and 7,187 lines** within that total.

| Component | Files | Physical lines |
|---|---:|---:|
| VM/runtime and native builtins (`include/Interpreter/`, `lib/Interpreter/`) | 29 | 7,187 |
| Lexer, parser, and AST (`include/Parser/`, `lib/Parser/`) | 10 | 4,207 |
| Compiler and bytecode (`include/Compiler/`, `lib/Compiler/`) | 11 | 2,884 |
| Diagnostics (`include/Diag/`, `lib/Diag/`) | 6 | 1,178 |
| Remaining core files: support, utilities, file/source management, platform, and runner | 23 | 5,570 |
| **Core total** | **79** | **21,026** |

These counts include headers, implementations, templates, definition tables,
comments, and blank lines. They are not a count of executable statements or
comment-free code. Tests, tools, Fairuz standard-library modules, documentation,
editor assets, dependencies, and generated build files are excluded. The CLI's
`main.cpp` is also excluded: it contributes another **407 lines in one file**.

To recount the core and runtime scopes from the repository root without building:

```sh
python3 - <<'PY'
import pathlib
import subprocess

files = subprocess.check_output(
    ["git", "ls-files", "-z", "--", "include", "lib"]
).decode("utf-8").split("\0")
files = [path for path in files if path]

def physical_lines(path):
    data = pathlib.Path(path).read_bytes()
    return data.count(b"\n") + int(bool(data) and not data.endswith(b"\n"))

runtime = [path for path in files if path.startswith(
    ("include/Interpreter/", "lib/Interpreter/")
)]
for label, paths in [("Core engine", files), ("VM/runtime", runtime)]:
    print(f"{label}: {len(paths)} files, {sum(map(physical_lines, paths)):,} lines")
PY
```

The command reads the current contents of tracked source files. Recount and
update the snapshot when source files change; do not add these subset totals
to the core total a second time.

## Protected program entry

`main.cpp` owns CLI argument parsing and process exit codes. Program execution,
`--check`, AST/bytecode dumping, and execution timing go through
`run_protected()` in `include/VMRunner.hpp` and `lib/VMRunner.cc`. Formatting
and semantic-token generation remain separate tool operations.

The runner owns a fresh allocator arena for each invocation and a fresh VM
when executing a program. It saves and
restores the caller's allocator context and complete diagnostic state, reports
failures, and contains C++ exceptions. The convenience overload returns
`ExitStatus`; the options overload also classifies failures so the CLI can map
them to its exit codes. No AST, chunk, or VM `Value` escapes the private arena.
JSON diagnostics are emitted at the boundary before the caller's engine is
restored; compile-phase diagnostics are retained across VM diagnostic resets.

Whole-program tests use the same boundary through `tests/test_source_program.hpp`
and assert their results inside the source program. Tests that inspect bytecode,
returned values/errors, or repeated execution in one VM retain direct compiler
and VM access. Runner coverage lives in `tests/test_vm_runner.cpp`.

This boundary is not a sandbox or a trap for assertions, signals, or native
process faults. The allocator context and diagnostic engine are process-global;
callers must serialize operations that use them.

## Register and operand identities

`RegType` and `PCType` identify registers and instruction positions. Constant,
big-integer, function, and class-descriptor pools have separate index types;
inline-cache slots and method slots are also distinct. These wrappers support
explicit construction and comparison, not general-purpose integer arithmetic.

Each `CompilerState` owns its register allocator. Its cursor, checkpoints, and
peak slot count are wider integer counts, not register identities: a full
250-slot frame has a one-past-last cursor of 250 but a highest register ID of
249. Raw instruction words and decoded fields remain integers because an
operand can encode a register, count, immediate, index, or payload. The emitter
checks wide operands before narrowing them into encoded fields.

## AST construction and lazy function bodies

`Parser` creates expression and statement nodes through `AST::ASTBuilder`
(`include/Parser/ast_builder.hpp`). Nodes, token pointers, and their spellings use the
existing arena lifetime. The builder does not own a second program or allocator;
`parse_program()` returns the top-level statements.

The default `BodyParsing::Lazy` mode parses ordinary function names and parameters
immediately. Each definition is still a `FuncDefStmt`, with a non-null body that
initially holds a `FunctionStub`. Stubs have their own `FUNCTION_STUB` kind, so
visitors, casts, and cloning cannot confuse a deferred body with a definition.

The parser scans the body's tokens to find its boundary without constructing its
statement/expression tree. It handles inline suites, indented suites, nested
indentation, comments, multiline expressions, and end of file. Each stub retains
a bounded token stream ending in `ENDMARKER`, body text, and the immutable source
snapshot. Token locations retain the original file's UTF-8 byte offsets, lines,
and columns. This is lazy **parsing**, not lazy lexing: lexical errors and
indentation errors are still reported on the initial pass.

On first invocation, the existing deferred compiler calls
`ASTBuilder::materialize()`. It replays the saved tokens through the parser and
caches the body only after a successful, complete parse. The compiler then emits
bytecode transactionally. A parsing failure leaves the AST stub unmaterialized;
a compilation failure leaves the chunk deferred with no partial bytecode. A
successfully parsed body can be reused when compilation is retried. Neither the
original parser, file manager, nor compiler needs to remain alive.

Callers that need full syntax trees use `Parser(&file, BodyParsing::Eager)`.
Formatting, semantic highlighting, and `--dump-ast` use this mode. `--check` uses the normal pipeline and
`Compiler::compile_all()` to materialize and compile unused functions without
executing them or loading imports. Class methods currently remain eagerly parsed
because their assignments determine the class field layout; their bytecode is
still compiled on first invocation. Nested function definitions remain unsupported
by the compiler.

Regression coverage lives in `tests/test_parser/test_lazy_parsing.cpp`,
`tests/test_compiler/test_lazy_compilation.cpp`, and the parser/CLI/formatter suites. Run:

```sh
cmake --build build --target fairuz_tests -j4
./build/fairuz_tests --gtest_filter='LazyParsing.*:LazyCompilation.*:ParserTest.*:CliE2E.*:Formatter.*'
ctest --test-dir build --output-on-failure
```
