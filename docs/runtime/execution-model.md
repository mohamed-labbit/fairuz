# Execution model

The CLI reads a UTF-8 source file, lexes and parses it, compiles it, then runs bytecode. Top-level statements execute in order. A function runs only when called. A module's top-level statements execute when its import is first reached. `--check` stops after compilation.

Operands and call arguments are evaluated in source order. Logical `و` and `او` skip the right operand when the left decides the result. `اذا` and `طالما` test [truthiness](../language/types-and-values.md); native `لكل` traverses a list. An uncaught runtime error terminates execution and produces a diagnostic. Recursion, allocation, container sizes, and source nesting have implementation limits rather than unlimited guarantees.

**Current implementation.** [main.cpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/main.cpp) drives the pipeline; [fcompiler.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fcompiler.cc) emits register bytecode; [fvm.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc) dispatches it. The last top-level call can be emitted as a return for internal execution, which does not alter its printed output.
