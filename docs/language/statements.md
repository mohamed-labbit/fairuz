# Statements

A header ending in `:` accepts either one statement on the same logical line or a nonempty indented body on subsequent lines. This applies to `اذا`, `غيره اذا`, `غيره`, `طالما`, `لكل`, functions, and methods. Classes accept a single inline method; see [functions and classes](functions.md). The [grammar](grammar.ebnf) describes these forms.

| Form | Effect |
|---|---|
| `name := expression` | Bind or replace a name. Index and field targets are also accepted. |
| `اذا condition: ... غيره: ...` | Execute one suite according to truthiness. `غيره اذا` adds a branch. |
| `طالما condition: ...` | Reevaluate the condition before each iteration. |
| `لكل name في expression: ...` | Evaluate the iterable and iterate a native list. |
| `اخرج`, `اكمل` | Leave the innermost loop or begin its next iteration. |
| `ارجع expression` | Leave a function with a value; bare `ارجع` returns nil. |
| `تاكد condition، message` | Call the assertion builtin; message is optional. |
| `استورد`, `من ... استورد ...` | Load and bind a module or symbols. |

`اخرج` and `اكمل` outside a loop, and `ارجع` outside a function, are compile errors. Import and assertion statements are described in [imports](modules-and-imports.md) and [errors](errors.md). Library iterator objects expose an explicit `التالي()` protocol; native `لكل` currently iterates lists.

## Inline bodies

Put the single body statement after the colon. Each `غيره اذا` or `غيره` clause starts on its own line, aligned with the original `اذا`.

```fa
المجموع := 0
لكل قيمة في [1، 2، 3]: المجموع += قيمة
اذا المجموع < 0: اكتب("سالب")
غيره اذا المجموع = 6: اكتب("تم")
غيره: اكتب("غير متوقع")
طالما المجموع < 8: المجموع += 1
اكتب(المجموع)
```

Output: `تم`, then `8`. A following statement at the header's indentation is outside the body.

Inline and indented bodies can be mixed in one conditional chain. `ارجع`, `اخرج`, `اكمل`, assertions, calls, and assignments are valid inline statements, subject to the same function and loop restrictions as indented code.

```fa
دالة اشارة(قيمة):
    اذا قيمة < 0: ارجع -1
    غيره اذا قيمة = 0:
        ارجع 0
    غيره: ارجع 1
تاكد اشارة(-5) = -1
تاكد اشارة(0) = 0
تاكد اشارة(5) = 1

لكل قيمة في [1، 2، 3، 4]:
    اذا قيمة = 2: اكمل
    اذا قيمة = 4: اخرج
    اكتب(قيمة)
```

Output: `1`, then `3`. Returning from one branch does not prevent other branches from running when their conditions are selected.

An inline body can itself be one compound statement. In the following nested conditional, the aligned `غيره` belongs to the outer `اذا`:

```fa
قيمة := 0
اذا خطا: اذا صحيح: قيمة := 1
غيره: قيمة := 2
اكتب(قيمة)
```

Output: `2`. Prefer indented bodies when nesting would make the structure hard to read.

## Layout limits

- A logical newline immediately after `:` requires an indented body. A trailing comment after the colon does not supply a body.
- Blank lines and comments between statements or methods are allowed; a body cannot contain only comments.
- An inline body contains one statement. Multiple adjacent statements or semicolon-separated statements on the same line are not supported.
- Inline syntax does not use a matching `DEDENT`; any following dedent closes an enclosing indented body. The final statement can end at end-of-file, including a bare `ارجع` inside a function.
- Parenthesized calls and collections can span physical lines; see [logical newlines](lexical-structure.md).

For example, these forms are invalid:

```text
اذا صحيح:
اكتب(1)

دالة مثال(): ارجع 1 ارجع 2
```
