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
