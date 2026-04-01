# My Programming Language

This is a scrappy, work-in-progress attempt to build the first Turing-complete programming language with Arabic-first syntax. It is meant to be easy to read, pythonic in spirit, and a fun playground for learning how lexers, parsers, and runtimes fit together.

## What's inside
- `src/lex` and `include/lexer`: tokenization utilities and experiments for Arabic-oriented text.
- `src/parser`: early parsing work and AST scaffolding.
- `src/runtime`: runtime building blocks and value handling.
- `tests/`: GoogleTest suites that cover the lexer and memory arena.
- C++23 code built with CMake; uses `simdutf`, OpenMP, and GoogleTest for tooling and runtime validation.

## Command line
After installation, run programs with:

```bash
fairuz file.fa
```

Options can appear before or after the file:

```bash
fairuz file.fa --time
fairuz --check file.fa
fairuz file.fa --dump-bytecode
```

Useful flags:
- `--check` parses and compiles without executing
- `--dump-ast` prints the parsed AST
- `--dump-bytecode` prints compiled bytecode
- `--time` prints execution time to stderr
- `--help` and `--version`

## Testing and security
Thanks to Mohamed Labbit and Mohamed Salem Eddah for pushing on testing and tightening security edges.

## Build and test
```bash
bash build.sh test
```

### cleanup and rebuild
```bash
bash build.sh --clean test
```

### run a program
```bash
bash build.sh run demo_factorial.fa
```

Fairuz source files use the `.fa` extension by default.

## Syntax Highlighting
- A VS Code syntax package is included at [editors/vscode/fairuz](/Users/mohamedrabbit/code/fairuz/editors/vscode/fairuz).
- It registers the `fairuz` language and associates it with `.fa` files.

### install locally from CMake
```bash
bash build.sh install /tmp/fairuz-prefix
```

This installs:
- `bin/fairuz`
- docs under `share/doc`
- example programs under `share/fairuz/examples`

## Homebrew
A Homebrew formula template is included at:

- [packaging/homebrew/fairuz.rb](/Users/mohamedrabbit/code/fairuz/packaging/homebrew/fairuz.rb)

Typical release flow:
1. Create a tagged source release, for example `v0.0.0`
2. Fill in the release tarball `sha256` in the formula
3. Install with `brew install --build-from-source ./packaging/homebrew/fairuz.rb`
