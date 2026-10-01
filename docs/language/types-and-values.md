# Types and values

| Literal | Value | Notes |
|---|---|---|
| `عدم` | nil | Printed as `nil` by the current printer. |
| `صحيح`, `خطا` | Boolean | Distinct from numbers. |
| `42`, `0b101`, `0o12`, `0x2a`, `٤٢` | Integer | Arithmetic can grow beyond a machine word. |
| `3.5` | Real | IEEE double in the current runtime. |
| `"نص"`, `'text'` | String | UTF-8 text; indexing counts Unicode code points. |
| `[1، 2]`, `(1، 2)`, `()` | List | Parenthesized comma forms construct lists, not tuples. |
| `{"ا": 1}` | Dictionary | Keys and values are expressions. |

Functions, classes, instances, modules, native callables, and file handles are runtime values. Lists and dictionaries are mutable reference objects; assigning one to another name aliases it. A plain list constructor or copy helper is needed for a separate container. Strings are immutable. Dictionary keys use the runtime's key equality and hashing, which differ from full recursive list/dictionary equality; use scalar keys for predictable behavior.

**Language behavior.** False values are `عدم`, `خطا`, and numeric zero. Objects are true, including empty strings, lists, and dictionaries. `=` and `!=` compare numbers by value and strings by content. Lists and dictionaries compare recursively, including cyclic containers. Distinct functions, classes, instances, modules, and resources compare unequal unless they are the same object. See [expressions](expressions.md) and [numeric behavior](../runtime/numeric-behavior.md).

**Current implementation.** Value tags, object types, and equality are in [fvalue.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvalue.hpp) and [fvm.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc). The representation is not a source-language guarantee.
