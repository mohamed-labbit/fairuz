# Architecture

```text
UTF-8 file → FileManager/SourceManager → Lexer → Parser/AST → Compiler/Chunk → VM
                                                                     ↓
                                                   Value/Object ← Builtins/stdlib
                                                                     ↓
                                                       GarbageCollector
```

The [lexer](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/flexer.cc) produces tokens and indentation events using the character table in [fctype.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fctype.hpp). The [parser](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fparser.cc) builds nodes from [fAST.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fAST.hpp). The [compiler](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fcompiler.cc) resolves names, emits instructions defined in [fopcode.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fopcode.hpp), and stores them in chunks. The [VM](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc) runs those chunks, raises runtime errors, and loads modules.

[fvalue.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvalue.hpp), [fobject.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fobject.hpp), and [finteger.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/finteger.cc) define runtime values and numeric paths. [fgc.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fgc.cc) collects unreachable objects. [fbuiltins.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fbuiltins.cc) registers native functions; each `.ف` file in [stdlib](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/stdlib/) is a Fairuz module. [main.cpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/main.cpp) implements CLI modes. [tests](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/tests/) contain GoogleTest and source fixtures; `stdlib/tests` contains executable Fairuz programs. Run `ctest --test-dir build --output-on-failure` after building tests.

The VM currently has NaN-boxed values on supported platforms and a fallback representation. This layout and the bytecode format are internal details.

## AST construction and lazy function bodies

`Parser` creates expression and statement nodes through `AST::ASTBuilder`
(`fairuz/fASTBuilder.hpp`). Nodes, token pointers, and their spellings use the
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

Regression coverage lives in `tests/test_lazy_parsing.cpp`,
`tests/test_lazy_compilation.cpp`, and the parser/CLI/formatter suites. Run:

```sh
cmake --build build --target fairuz_tests -j4
./build/fairuz_tests --gtest_filter='LazyParsing.*:LazyCompilation.*:ParserTest.*:CliE2E.*:Formatter.*'
ctest --test-dir build --output-on-failure
```
