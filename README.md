# My Programming Language

This is a scrappy, work-in-progress attempt to build the first Turing-complete programming language with Arabic-first syntax. It is meant to be easy to read, pythonic in spirit, and a fun playground for learning how lexers, parsers, and runtimes fit together.

## What's inside
- `src/lex` and `include/lexer`: tokenization utilities and experiments for Arabic-oriented text.
- `src/parser`: early parsing work and AST scaffolding.
- `src/runtime`: runtime building blocks and value handling.
- `tests/`: GoogleTest suites that cover the lexer and memory arena.
- C++20 code built with CMake; uses libc++, OpenMP, and GoogleTest for tooling and concurrency experiments.

## Testing and security
Thanks to Mohamed Labbit and Mohamed Salem Eddah for pushing on testing and tightening security edges.

## Build and test
```bash
bash test.sh -d
```
