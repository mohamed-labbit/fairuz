# Builtin functions

These names are available without import. Signatures show accepted argument positions; `...` means variable arity. Invalid types sometimes return `عدم` and sometimes raise a runtime error, as stated below. They are registered in [fbuiltins.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fbuiltins.cc). Names beginning `__` in that registry are library hooks and are not public APIs.

| Signature | Result and effects | Invalid input / boundary behavior |
|---|---|---|
| `طول(قيمة)` | Integer length of string (Unicode code points), list, or dictionary. | Other types return `عدم`; invalid UTF-8 strings count bytes. |
| `اضف(قائمة، قيمة، ...)` | Appends each value to the same list; returns `عدم`. | Fewer than two args or a non-list raises. |
| `احذف(قائمة)` | Removes the last list item and returns the list. | Empty/non-list raises. |
| `مقطع(قيمة، بداية، نهاية)` | New string or list, with **inclusive** end; end may be omitted. | Integer nonnegative indexes required; out of range raises. |
| `قائمة(قيمة، ...)` | New list containing the arguments, including an empty list for no args. | Values are stored by reference. |
| `قاموس(مفتاح، قيمة، ...)` | New dictionary from consecutive key/value pairs. | An unmatched last argument is ignored by the current implementation. |
| `اكتب(قيمة، ...)` | Prints arguments separated by tabs and a trailing newline; returns `صحيح`. | Zero args prints a blank line. Cycles are shown as `<cycle>`. |
| `ادخل()` | Reads one line from standard input as a string. | EOF returns `عدم`. |
| `افتح(مسار، وضع)` | Opens a file handle. | File and mode errors propagate. |
| `اضف_ملف(ملف، نص)` | Writes to an open handle. | Wrong handle/type or I/O failure raises. |
| `اغلق(ملف)` | Closes a file handle. | Invalid handle raises. |
| `صنف(قيمة)` | Arabic string naming the runtime type. | Exactly one argument. |
| `طبيعي(قيمة)` | Integer unchanged, or real converted to integer. | Other types return `عدم`; nonfinite/out-of-range reals can fail. |
| `حقيقي(قيمة)` | Real conversion of a number. | Other types return `عدم`; large integer precision may be lost. |
| `سلسلة()` / `سلسلة(قيمة)` | New string; no argument gives `""`, one argument renders the value. | More than one argument raises. |
| `منطقي(قيمة)` | Boolean truthiness result. | Wrong argument count raises. |
| `اقسم(نص، فاصل)` | List of substrings split on the exact separator bytes. | Non-strings return `عدم`; empty separator returns a one-item list. |
| `اجمع(قائمة، فاصل)` | Joins rendered list items into a string. | Non-list/non-string returns `عدم`. |
| `جزء(نص، بداية، نهاية)` | String substring; end is **exclusive**, positions count code points. | Wrong types/count or negative range raises. |
| `يحتوي(نص، جزء)` | Boolean substring test. | Non-strings return `عدم`; empty needle is true. |
| `قص(نص)` | New string with ASCII space, tab, CR, LF removed at both ends. | Non-string returns `عدم`. |
| `ادنى(رقم)`, `اعلى(رقم)` | Floor or ceiling. | Numeric types required. |
| `تقريب(رقم)` | Rounded numeric value. | Numeric types required. |
| `مطلق(رقم)` | Absolute value. | Numeric types required. |
| `اصغر(رقم، ...)`, `اكبر(رقم، ...)` | Minimum or maximum of arguments. | Empty input or incompatible values can fail. |
| `قوة(أساس، أس)` | Numeric exponentiation, like `**`. | Numeric types required; integer negative exponents produce real. |
| `جذر(رقم)` | Square root as real. | Invalid type/domain follows native error handling. |
| `تاكد(شرط)` / `تاكد(شرط، رسالة)` | Returns on truthy condition; otherwise raises an assertion error. | Other arities fail. |
| `عطل(رسالة)` | Raises a runtime error. | No source-level catch mechanism. |
| `ساعة()` | Clock value from the process clock. | Platform-dependent resolution. |
| `وقت()` | Currently returns `عدم`; no wall-clock contract. | See [unresolved](../documentation/unresolved.md). |

```fa
القيم := قائمة(1، 2)
اضف(القيم، 3)
تاكد طول(القيم) = 3
اكتب(جزء("فيروز"، 0، 2))
```

Expected output: `في`. For imported helpers, use the [module index](index.md). File and numeric boundary behavior should be tested for the target platform before depending on it.
