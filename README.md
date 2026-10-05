# Fairuz (فيروز)

Fairuz is an Arabic-syntax programming language implemented in C++23. It has a
hand-written lexer and parser, a register-based bytecode compiler, and a virtual
machine with automatic mark-and-sweep garbage collection. The runtime uses
NaN-boxed values on supported platforms, with arbitrary-size integers stored on
the heap when they outgrow the inline representation.

The standard library combines native runtime primitives with modules written in
Fairuz under [`stdlib/`](stdlib/). The executable currently reports version
`0.1.0`; the standard library's own metadata reports `0.2.0`. Both are under active
development, and some library APIs still need runtime support (see below).

## Language tour

Blocks use indentation. `:=` assigns a value; `=` tests equality. Both Arabic
`،` and ASCII `,` commas are accepted.

A body containing one statement can also follow the colon on the same line.
This works for `اذا`, `غيره اذا`, `غيره`, `طالما`, `لكل`, functions, and methods.
A class can contain a single inline method. A newline immediately after the
colon still requires an indented body; `غيره` aligns with its `اذا` header.

```fa
قيمة := -3
اذا قيمة < 0: قيمة := -قيمة
لكل عنصر في [1، 2، 3]: اكتب(عنصر)
طالما قيمة > 0: قيمة -= 1
دالة ضعف(س): ارجع س * 2
نوع علبة: دالة بداية(قيمة): هذا.قيمة := قيمة
```

```fa
دالة فيب(ن):
    اذا ن <= 1:
        ارجع ن
    ارجع فيب(ن - 1) + فيب(ن - 2)

اكتب(فيب(10)) # 55
```

```fa
الدرجات := {"ا": 1، "ب": 2}
الدرجات["ب"] := 9
اكتب(الدرجات["ب"]) # 9

المجموع := 0
لكل قيمة في [1، 2، 3]:
    المجموع += قيمة
تاكد(المجموع = 6)
```

The language supports UTF-8 source, Arabic and Latin identifiers, functions,
classes with constructors and single inheritance, methods using `هذا`,
conditionals, loops, lists, dictionaries, indexing, and augmented assignment.
Native `لكل` loops currently iterate over lists. Lazy library iterators use an
explicit `التالي()` protocol; convert them with `الى_قائمة` before using a
native `لكل` loop.

| Fairuz | Meaning | Fairuz | Meaning |
|---|---|---|---|
| `دالة` | function | `اذا` / `غيره` | if / else |
| `طالما` | while | `لكل` ... `في` | for ... in |
| `ارجع` | return | `اخرج` / `اكمل` | break / continue |
| `نوع` | class | `هذا` | this |
| `صحيح` / `خطا` | true / false | `عدم` | nil |
| `و` / `او` / `ليس` | and / or / not | `تاكد` | assert |
| `استورد` | import | `من` / `باسم` | from / as |

### Numbers and text

- Integers grow beyond machine-word limits for arithmetic, bitwise operations,
  shifts, and nonnegative integer powers. `**` and `قوة` support integer powers.
- Real numbers use double precision. Integer division returns an integer when
  exact (`6 / 3`), otherwise a real (`7 / 2`). Negative integer powers also return
  reals. Integer `%` follows the dividend's sign; division by zero is an error.
- `طبيعي` converts to an integer, `حقيقي` to a real, and `سلسلة` to text.
  Native APIs that require a machine-sized count or index can reject a large
  integer even though arithmetic supports it.
- String length and substring positions count Unicode code points, not UTF-8
  bytes or grapheme clusters. Binary file, encoding, and compression APIs use
  strings as byte containers.

```fa
كبير := 2 ** 100
تاكد(كبير = 1267650600228229401496703205376)
تاكد(6 / 3 = 2)
تاكد(7 / 2 = 3.5)
تاكد(طول("مرحبا") = 5)
```

## Standard library

Core native functions are available without imports:

| Area | Functions |
|---|---|
| Collections | `طول`, `اضف`, `احذف`, `مقطع`, `قائمة`, `قاموس` |
| I/O | `اكتب`, `ادخل`, `افتح`, `اضف_ملف`, `اغلق` |
| Conversion | `صنف`, `طبيعي`, `حقيقي`, `سلسلة`, `منطقي` |
| Strings | `اقسم`, `اجمع`, `جزء`, `يحتوي`, `قص` |
| Math | `ادنى`, `اعلى`, `تقريب`, `مطلق`, `اصغر`, `اكبر`, `قوة`, `جذر` |
| Assertions and time | `تاكد`, `عطل`, `ساعة`, `وقت` |

Higher-level functions and classes must be imported. Module names use their
Arabic filenames without the `.ف` extension:

```fa
استورد مجموعات باسم مج
من رياضيات استورد عاملي
من جيسون استورد JSON_رمز، JSON_حلل

القيم := مج.مدى(1، 6، 1)
تاكد(مج.مجموع(القيم) = 15)
تاكد(عاملي(5) = 120)

النص := JSON_رمز({"القيم": القيم})
تاكد(JSON_حلل(النص)["القيم"][4] = 5)
اكتب(النص)
```

### Module loading

`استورد مجموعات` binds a module accessed through `مجموعات.مجموع(...)`.
`من مجموعات استورد مجموع باسم جمع` binds one exported value under an alias.
Modules have separate global environments and are loaded once per resolved path
within a VM. Imported functions retain their defining module's globals.

For a dotted name such as `حساب.ادوات`, the loader searches for
`حساب/ادوات.ف`. Search order is:

1. The directory named by `FAIRUZ_STDLIB`, when set.
2. The source-tree stdlib directory recorded at build time.
3. The installation stdlib directory recorded at configuration time.
4. The importing file's directory.
5. The current working directory.

Bundled stdlib names therefore take precedence over local files with the same
name. Entrypoint files can use `.fa` or `.ف`; imported modules use `.ف`.

### Library areas

These modules contain implemented library functionality. Their source files and
[`stdlib/tests/`](stdlib/tests/) show the available functions, argument order,
and examples; the limitations following the table apply to partially supported
APIs.

| Area | Modules | Contents |
|---|---|---|
| Values and functions | `وقت_التشغيل`, `نتيجة`, `دوالية` | Type predicates, result/option classes, function composition |
| Collections | `مجموعات`, `قواميس`, `تراكيب`, `نسخ` | Mapping, filtering, sorting, dictionary helpers, stacks, queues, sets, LRU cache, shallow/deep copying |
| Iteration | `مكررات`, `أدوات_التكرار` | Lazy ranges and transformations, chaining, combinations, permutations |
| Text | `سلاسل`, `لف_النص` | Searching, replacement, padding, wrapping, indentation |
| Numbers | `رياضيات`, `إحصاء`, `أعداد` | Math helpers, statistics, fraction and complex classes; decimal support is incomplete |
| Structured data | `جيسون`, `قيم_مفصولة`, `إعدادات` | JSON, CSV, INI |
| Time | `تاريخ_ووقت` | Date/time construction, parsing, formatting, UTC and local time |
| Files | `ملفات`, `نظام_الملفات`, `مسارات` | File handles, text I/O, globbing and temporary resources; several path/file operations are incomplete |
| Patterns and URLs | `تعابير_نمطية`, `عناوين_شبكية` | Regular expressions, URL components, query encoding/decoding |
| Bytes | `ترميزات`, `ضغط` | Base64, hex, SHA-256, HMAC-SHA-256, gzip with a decompression size limit |
| Application helpers | `سجل`, `طرفية`, `محلل_وسائط` | Structured logging, terminal messages, parsing an explicitly supplied argument list |
| Task API | `لا_متزامن` | Executors and futures with synchronous execution in the current runtime |
| Library metadata | `حزمة`, `عقد_البدائيات` | Library identity and native-interface declarations |

### Current limits

A module's presence on disk or in `حزمة` is not a guarantee that every function
has a working native backend. Names beginning with `__` are runtime hooks used
by the library; prefer the public wrappers.

- `لا_متزامن` executes submitted work immediately on the calling VM. Worker
  counts and timeout arguments do not provide parallel execution or timed waits.
- `ترميزات` currently supports only `sha256` for hashing/HMAC, and `ضغط`
  supports only `gzip` as its compression algorithm.
- `ملفات` supports opening, reading, writing, flushing, and closing. Seeking,
  truncation, file metadata, and atomic writing still reference missing hooks.
  Many `مسارات` methods and `نظام_الملفات.انسخ_مسار` also lack native support.
- `أعداد.عشري`, random-number APIs in `عشوائي`, sleeping in `زمن`, OS APIs in
  `نظام_التشغيل`, subprocesses in `عمليات_فرعية`, SQLite in `قاعدة_سكليت`,
  secrets in `أسرار`, and UUIDs in `معرفات_فريدة` reference unimplemented hooks.
- Basic assertions in `اختبار` are written in Fairuz, but exception capture and
  the test runner need missing hooks. Some introspection helpers in `انواع`
  are also incomplete.
- `حزمة` lists `بروتوكول_نقل_النص`, but that module is not shipped yet. URL
  utilities do not provide an HTTP client.

## Build and test

Requirements:

- CMake 3.14+ and a C++23 compiler (Clang or GCC).
- zlib development headers and library.
- `simdutf`, found through your package manager or fetched by CMake.
- Network access for uncached dependencies. Test builds fetch GoogleTest and
  compile it with the selected compiler.

The build script prefers Clang, selects Ninja when available, and defaults to a
Release build with LTO:

```bash
./build.sh
./build.sh --gcc
./build.sh --gcc --debug
./build.sh test
./build.sh test-stdlib
```

`test` enables Debug and ASan, runs GoogleTest, then the Fairuz stdlib tests.
Debug/test builds disable LTO. On macOS, GCC builds suppress automatic dSYM
generation at link time to avoid a `dsymutil` crash; Debug objects retain debug
information. The script resets its managed flags when changing build modes.

For direct CMake use, configure a separate build directory for each compiler:

```bash
cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=ON
cmake --build build-clang -j4
ctest --test-dir build-clang --output-on-failure
```

CTest includes both the C++ suite and individual stdlib programs. To select only
stdlib cases, add `-R '^Stdlib\.'`. Use `./build.sh --gcc` for the macOS GCC
workaround; a direct CMake configuration does not apply the script's flags.

The GoogleTest suite also includes public stdlib API tests that launch the
interpreter in isolated temporary directories. They exercise complete workflows
and check exit status, output, and diagnostics for rejected inputs. Run just
these cases after building `fairuz_tests`:

```bash
ASAN_OPTIONS=detect_leaks=0 ./build/fairuz_tests '--gtest_filter=*StdlibE2E*'
```

### Windows CI

The `CI` workflow runs native x64 Windows builds on `windows-2022` with Visual
Studio 2022 (MSVC), in both Debug and Release. It installs zlib through vcpkg,
builds the interpreter and all C++ tests, then runs the full CTest suite,
including the standard-library programs. Compilation failures, test failures,
and an empty test suite fail the job. Windows compatibility is still in
progress: existing POSIX dependencies must be ported before these jobs can pass.

After committing and pushing the workflow and source changes, open a pull
request targeting `main` to run CI, or use **Actions → CI → Run workflow** on a
branch containing the workflow. Pushes to `main` also run CI. Inspect the
`test (MSVC, Windows, Debug)` and `test (MSVC, Windows, Release)` jobs; available
build/configuration logs and test reports are saved as
`windows-Debug-diagnostics` and `windows-Release-diagnostics` artifacts.

## Run and install

```bash
./build/fairuz examples/hello.fa
./build/fairuz examples/fibonacci.fa
./build/fairuz --check examples/sum_list.fa
./build/fairuz stdlib/tests/اختبار_جيسون.ف
```

Configure the installation prefix before building so the runtime records the
correct stdlib path:

```bash
cmake -S . -B build-install -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tmp/fairuz
cmake --build build-install --target fairuz -j4
cmake --install build-install
/tmp/fairuz/bin/fairuz examples/hello.fa
```

The installation includes `bin/fairuz`, documentation under `share/doc/Fairuz/`,
and the `.ف` library files under `share/fairuz/stdlib/`. If you relocate the
installation or override the prefix only during `cmake --install`, set
`FAIRUZ_STDLIB` to the installed library directory:

```bash
FAIRUZ_STDLIB=/tmp/fairuz/share/fairuz/stdlib /tmp/fairuz/bin/fairuz program.ف
```

## Command line

```bash
fairuz <file> [options]
fairuz format [--check] <file-or-directory>
```

| Flag | Effect |
|---|---|
| `--check` | Parse and compile the input without executing it |
| `--dump-ast` | Print the parsed AST |
| `--dump-bytecode` | Print the compiled bytecode |
| `--time` | Print execution time to stderr |
| `--diagnostics=json` | Write structured diagnostics to stderr |
| `--diagnostics=text` | Use text diagnostics (the default) |
| `--semantic-tokens` | Emit parser-backed semantic tokens as JSON |
| `-h`, `--help` | Show usage |
| `-V`, `--version` | Show the language version |

Options may appear before or after the input path. The CLI accepts one input
file; it does not currently forward arbitrary arguments to Fairuz programs.
Normal execution parses the whole input but compiles function and method bodies
only on their first call, then reuses their bytecode. Unused bodies remain
uncompiled, so body compilation errors are reported when called. `--check`
compiles every body in the input without running it. `--dump-bytecode` shows
the initial bytecode; combine it with `--check` to include all function bodies.
Imports execute at runtime, so `--check` does not verify imported modules or
native functions used only during execution. Semantic-token mode accepts `-`
as its input path to read source from standard input.

`fairuz format <file-or-directory>` rewrites Fairuz files with canonical formatting.
It preserves comments, literal spellings, grouping, and existing line breaks;
normalizes indentation to four spaces and comma/operator spacing; wraps long
single-line calls and collections at commas; and validates the result before
replacing a file. Lines without a safe comma break remain intact. Use
`# fmt: off` and `# fmt: on` to preserve a region, or `# fmt: skip` on one line.
`fairuz format --check <path>` leaves files unchanged and exits with 0 when
everything is formatted, 1 when changes are needed, or an error code for
invalid input. Invalid source is left unchanged.
Diagnostics disable ANSI styling when stderr is redirected or `NO_COLOR` is
set, and escape terminal control bytes from source text and paths.

Fairuz programs execute with the permissions of the `fairuz` process and can
access files. Fairuz is not a sandbox: run untrusted source only inside an
appropriately restricted container or operating-system sandbox.

## Container

Build the non-root runtime image and run a source file from the current directory:

```bash
docker build -t fairuz .
docker run --rm -v "$PWD:/work:ro" \
  -e FAIRUZ_STDLIB=/opt/fairuz/share/fairuz/stdlib fairuz examples/hello.fa
```

## Project layout

```
fairuz/          Lexer, parser, compiler, VM, GC, integer runtime, native builtins
stdlib/          Standard-library modules written in Fairuz (.ف)
stdlib/tests/    Executable Fairuz library tests
tests/           GoogleTest unit/regression tests and data-driven test_cases/
examples/        Sample entrypoint programs (.fa)
editors/vscode/  VS Code syntax/language extension
packaging/       Homebrew formula template
main.cpp         CLI entry point
```

## Editor support

A VS Code extension is included in [`editors/vscode/fairuz`](editors/vscode/fairuz).

To package and install it locally:

```bash
cd editors/vscode/fairuz
vsce package
code --install-extension fairuz-language-0.1.0.vsix
```

## Homebrew

A Homebrew formula template for release packaging is included at
[`packaging/homebrew/fairuz.rb`](packaging/homebrew/fairuz.rb). Before publishing
it, replace the `sha256` placeholder with the checksum of the `v0.1.0` release
tarball after creating that tag.

## License

MIT
