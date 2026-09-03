# Fairuz (فيروز)

Fairuz is an Arabic-syntax programming language with a register-based bytecode
compiler and a virtual machine, written from scratch in C++23. Keywords,
identifiers, and standard library functions are all Arabic — the goal is a
language that reads naturally right-to-left rather than one that merely
transliterates English syntax.

This `0.1.0` release is the first public source drop. The pipeline is a
hand-written lexer and recursive-descent parser, a register-based bytecode
compiler, and a computed-goto VM with a NaN-boxed value representation and a
tricolor mark-and-sweep garbage collector.

## Language tour

```fa
دالة فيب(ن):
    اذا ن <= 1:
        ارجع ن

    ارجع فيب(ن - 1) + فيب(ن - 2)

اكتب(فيب(10))
```

```fa
قاموس := {
    "ا": 1،
    "ب": 2
}
اكتب(قاموس["ا"])
قاموس["ب"] := 9
اكتب(طول(قاموس))
```

Currently supported: UTF-8 source, Arabic identifiers, functions, classes
(`نوع`) with `هذا` (this), conditionals, `while`/`for` loops, `break`/
`continue`, lists, dictionaries, indexing, augmented assignment, and a small
standard library.

Core keywords:

| Fairuz | Meaning | Fairuz | Meaning |
|---|---|---|---|
| `دالة` | function | `اذا` / `غيره` | if / else |
| `طالما` | while | `بكل` ... `في` | for ... in |
| `ارجع` | return | `اخرج` / `اكمل` | break / continue |
| `نوع` | class | `هذا` | this |
| `صحيح` / `خطا` | true / false | `عدم` | nil |
| `و` / `او` / `ليس` | and / or / not | | |

## Standard library

Grouped by area, all accessed as global functions:

- **Collections** — `طول` (len), `اضف` (append), `احذف` (pop), `مقطع` (slice), `قائمة` (list), `قاموس` (dict)
- **I/O** — `اكتب` (print), `ادخل` (input), `افتح` (open), `اضف_ملف` (append to file), `اغلق` (close)
- **Conversion** — `صنف` (type), `طبيعي` (int), `حقيقي` (float), `سلسلة` (str), `منطقي` (bool)
- **Strings** — `اقسم` (split), `اجمع` (join), `جزء` (substr), `يحتوي` (contains), `قص` (trim)
- **Math** — `ادنى` (floor), `اعلى` (ceil), `تقريب` (round), `مطلق` (abs), `اصغر` (min), `اكبر` (max), `قوة` (pow), `جذر` (sqrt)
- **Misc** — `تاكد` (assert), `ساعة` (clock), `عطل` (error), `وقت` (time)

## Build

Requirements:

- CMake 3.14+
- A C++23 compiler (Clang recommended; GCC via `-DUSE_GCC=ON`)
- `simdutf`, available via your package manager or CMake's network fetch path
- OpenMP support is optional but recommended

Configure and build:

```bash
cmake -S . -B build -DBUILD_TESTS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target fairuz -j4
```

Run the test suite (400+ cases across the lexer, parser, compiler, VM, and
stdlib):

```bash
cmake --build build --target fairuz_tests -j4
./build/fairuz_tests
```

## Run

```bash
./build/fairuz examples/hello.fa
./build/fairuz examples/fibonacci.fa
./build/fairuz --check examples/sum_list.fa
```

Install to a custom prefix:

```bash
cmake --install build --prefix /tmp/fairuz
```

Installed files:

- `bin/fairuz`
- `share/doc/Fairuz/README.md`
- `share/doc/Fairuz/LICENSE`
- `share/fairuz/examples/*.fa`

## Command line

```bash
fairuz <file.fa> [options]
fairuz format <file.fa>
```

Options:

| Flag | Effect |
|---|---|
| `--check` | Parse and compile without executing |
| `--dump-ast` | Print the parsed AST |
| `--dump-bytecode` | Print the compiled bytecode |
| `--time` | Print execution time to stderr |
| `-h`, `--help` | Show usage |
| `-V`, `--version` | Show the language version |

`fairuz format <file.fa>` rewrites a file in place with canonical formatting.

## Project layout

```
fairuz/          Compiler and VM sources (lexer, parser, compiler, VM, GC, stdlib)
tests/           400+ unit and regression tests (GoogleTest), plus data-driven test_cases/
examples/        Sample .fa programs
editors/vscode/  VS Code syntax/language extension
packaging/       Homebrew formula template
main.cpp         CLI entry point
```

## Editor support

A VS Code extension is included in `editors/vscode/fairuz`.

To package and install it locally:

```bash
cd editors/vscode/fairuz
vsce package
code --install-extension fairuz-language-0.1.0.vsix
```

## Homebrew

A Homebrew formula template for release packaging is included at
`packaging/homebrew/fairuz.rb`. Before publishing it, replace the `sha256`
placeholder with the checksum of the `v0.1.0` release tarball.

## License

MIT