# Expressions and assignment

The parser evaluates operands from left to right through bytecode. `و` and `او` short circuit. Calls evaluate the callee and positional arguments; there are no keyword or default arguments. Assignment is a statement-level expression and is rejected inside another expression.

| Tightest first | Operators | Association |
|---|---|---|
| Postfix | `(...)`, `[...]`, `.name` | Left |
| Power | `**` | Right |
| Unary | `+`, `-`, `~`, `ليس` | Right |
| Product | `*`, `/`, `%`, `٪` | Left |
| Sum | `+`, `-` | Left |
| Shift | `<<`, `>>` | Left |
| Ordering | `<`, `<=`, `>`, `>=` | Left, without chained-comparison semantics |
| Equality | `=`, `!=` | Left |
| Bitwise | `&`, `^`, `\|` | Left |
| Logical | `و`, `او` | Left |

The exact relative order of `&`, `^`, `|` is the order shown. Unary operators bind *outside* power on the left: `-2 ** 2` is `-(2 ** 2)`. `2 ** -2` is accepted. Parentheses override precedence. Comparisons are binary operations; `1 < 2 < 3` should not be used as a mathematical chain.

`:=` binds a name or writes an index or field. `a := b := 3` associates right. Compound `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, and `>>=` read, operate, then write. The parser clones the target expression for the read; an indexed target with side effects can therefore evaluate parts of the target more than once. See [unresolved](../documentation/unresolved.md). Invalid targets such as `1 := 2` fail during parsing.

Numeric operations require supported numeric operands; bitwise operations and shifts require integers. `+` also joins strings and lists. `[]` reads lists, strings, and dictionaries; list/string indexes must be integers and out-of-range indexes fail. A list index can be assigned; string indexes cannot. Slicing is supplied by the [`مقطع` builtin](../standard-library/builtins.md), not slice syntax. See [numeric behavior](../runtime/numeric-behavior.md) for division and overflow.

```fa
اكتب(2 + 3 * 4)
اكتب(2 ** -2)
```

Expected output: `14`, then `0.25`.
