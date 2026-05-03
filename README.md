# Fairuz

Fairuz is an Arabic-first experimental programming language with a register-based bytecode compiler and virtual machine written in C++23.

This `0.1.0` release is the first public source release. The interpreter currently supports UTF-8 source files, Arabic identifiers, functions, conditionals, loops, lists, dictionaries, indexing, and a small standard library.

## Build

Requirements:

- CMake 3.14+
- A C++23 compiler
- `simdutf` available via your package manager, or a working CMake network fetch path
- OpenMP support is optional but recommended

Configure and build:

```bash
cmake -S . -B build -DBUILD_TESTS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target fairuz -j4
```

Run the test suite:

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

## Language Example

```fa
دالة فيب(ن):
    اذا ن <= 1:
        ارجع ن

    ارجع فيب(ن - 1) + فيب(ن - 2)

اكتب(فيب(10))
```

## Command Line

```bash
fairuz <file.fa> [options]
fairuz format <file.fa>
```

Available options:

- `--check` parses and compiles without executing
- `--dump-ast` prints the parsed AST
- `--dump-bytecode` prints the compiled bytecode
- `--time` prints execution time to stderr
- `--help`
- `--version`

## Editor Support

A VS Code extension is included in `editors/vscode/fairuz`.

To package and install it locally:

```bash
cd editors/vscode/fairuz
vsce package
code --install-extension fairuz-language-0.1.0.vsix
```

## Homebrew

A Homebrew formula template for release packaging is included at `packaging/homebrew/fairuz.rb`.

Before publishing it, replace the `sha256` placeholder with the checksum of the `v0.1.0` release tarball.

## License

MIT
