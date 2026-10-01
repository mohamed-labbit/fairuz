# Coverage and verification

Documented Git commit: `86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4`. The inspected worktree also contained pre-existing uncommitted source changes. The docs describe that inspected state; the commit alone does not reproduce all observations. Generated module inventories cover `.ف` declarations, not all runtime behavior.

| Area | Documentation | Evidence / status |
|---|---|---|
| Tokens, Unicode, layout, literals | [Lexical structure](../language/lexical-structure.md) | `fairuz/flexer.cc`, `fctype.hpp`, `ftoken.cc`; source inspected. |
| Statements, classes, functions, imports, assertions | [Grammar](../language/grammar.ebnf), [statements](../language/statements.md), [functions](../language/functions.md), [imports](../language/modules-and-imports.md) | `fairuz/fparser.cc`, `fAST.hpp`; parser branches cross-checked manually. |
| Operators, assignment, values | [Expressions](../language/expressions.md), [types](../language/types-and-values.md) | `ftoken.cc`, `fparser.cc`, `fvm.cc`, `fvalue.cc`; selected examples executed by validator. |
| Numeric and runtime behavior | [Numeric](../runtime/numeric-behavior.md), [execution](../runtime/execution-model.md) | `finteger.cc`, `fvm.cc`; boundary cases require more targeted execution. |
| Memory and modules | [Memory](../runtime/memory-management.md), [module loading](../runtime/module-loading.md) | `fgc.cc`, `fvm.cc`; source inspected, cycles unresolved. |
| Public native functions | [Builtins](../standard-library/builtins.md) | All non-`__` registry names appear; select examples executed. |
| Standard-library modules and symbols | [Module index](../standard-library/index.md) and dedicated module pages | Every shipped `.ف` file inventoried by `generate-module-reference.py`; behavior of each symbol is **not** individually verified. |
| CLI and build | [Installation](../getting-started/installation.md), [running](../getting-started/running-programs.md) | `main.cpp`, CMake, `build.sh`; local binary used. |
| Arabic rendering, links, search | MkDocs site and [validation script](validate.py) | Strict build passed. The tutorial and a module page were inspected in the local browser; Arabic search returned the module page. A 390 px viewport kept code blocks within the page. DOM text retained logical order without direction-control characters. |

Validation commands and exact results belong in the completion report. This table is intentionally explicit about partial evidence.

On 30 September 2026, `ctest --test-dir build --output-on-failure` passed all 30 configured tests (one C++ aggregate and 29 stdlib programs). `python3 docs/documentation/validate.py` passed 9 guided examples and 40 module lookup examples; another 2 code blocks passed `--check`. The strict MkDocs build passed.

## 1 October 2026: inline block bodies

The current worktree accepts one inline statement after `:` for `اذا`, `غيره اذا`, `غيره`, `طالما`, `لكل`, functions, and methods, plus one inline method for a class. Inline and indented method bodies share parsing and field-assignment handling. Newline and dedent ownership keeps following declarations outside the body; `غيره` aligns with the original conditional header. Missing bodies and extra adjacent simple statements are rejected. A bare return also works at end-of-file inside an enclosing block.

The compiler now resets branch reachability before compiling `else`, so a return, break, or continue in the first branch does not suppress the alternative. The [statement reference](../language/statements.md), [function reference](../language/functions.md), lexical and scope pages, tutorial, and English and Arabic grammar copies document these changes. The README and root `grammar.ebnf` also describe inline bodies.

Validation for this change:

- 264 selected parser, compiler, VM, formatting, and regression tests passed. This was a targeted run, not the entire C++ suite.
- `./build.sh test-stdlib` passed the former parser failures and stopped at `اختبار_عقدية.ف:80`: `عقدي.من_قطبي(...)` reports a method call on a non-instance. Running every stdlib case with CTest gave 28 passes and this one runtime failure.
- The documentation validator now executes the statement and function examples. Both `python3 docs/documentation/validate.py --lang en` and `--lang ar` passed 14 guided examples and 40 module lookup examples each.

Both English and Arabic MkDocs builds passed with `--strict` after the documentation update.

The earlier 30 September results above describe that earlier snapshot, not a claim that the current full suite passes.
