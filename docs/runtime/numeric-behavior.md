# Numeric behavior

Integers expand to heap-backed arbitrary-size values for arithmetic, bitwise operations, shifts, and nonnegative integer powers. Native APIs that require a machine-sized index or count can reject a large integer. Real numbers use double precision in the current runtime. Mixed integer/real arithmetic converts to real when required; conversion of very large integers can lose precision.

| Operation | Observed behavior |
|---|---|
| `6 / 3` | Integer `2` when division is exact. |
| `7 / 2` | Real `3.5` when not exact. |
| `-7 % 3` | Remainder has the dividend's sign. |
| `2 ** 100` | Arbitrary-size integer. |
| `2 ** -2` | Real `0.25`. |
| `x / 0`, `x % 0` | Runtime error. |
| `<<`, `>>`, `&`, `\|`, `^`, `~` | Integer operations; invalid types or shift counts fail. |

`طبيعي`, `حقيقي`, and `سلسلة` perform explicit conversions; their accepted inputs and failure behavior are in [builtins](../standard-library/builtins.md). Real arithmetic inherits finite precision and nonfinite values from the current double implementation. Exact wording and platform library edge cases are not guarantees. See [unresolved](../documentation/unresolved.md).

**Current implementation.** [finteger.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/finteger.cc) handles big integers and [fvm.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc) selects numeric operations.
