# Tutorial

These lessons use complete programs. Save each block in a UTF-8 `.fa` file and run it with `./build/fairuz file.fa`.

## 1. Values and variables

Learn assignment and arithmetic. `:=` assigns; `=` compares.

```fa
العدد := 7
العدد += 2
اكتب(العدد)
اكتب(العدد = 9)
```

Expected lines: `9`, `صحيح`. See [types](../language/types-and-values.md) and [expressions](../language/expressions.md).

## 2. Conditions and loops

Learn indentation, conditions, and list iteration. A colon introduces a body: use indentation for multiple statements, or put one statement on the same line. See [inline bodies](../language/statements.md).

```fa
المجموع := 0
لكل قيمة في [1، 2، 3]:
    المجموع += قيمة
اذا المجموع = 6:
    اكتب("تم")
غيره:
    اكتب("خطا")
```

Expected output: `تم`. See [statements](../language/statements.md).

## 3. Functions and collections

Learn parameters, return values, lists, and dictionaries.

```fa
دالة ضعف(قيمة):
    ارجع قيمة * 2

نتائج := [ضعف(2)، ضعف(3)]
سجل := {"اول": نتائج[0]}
اكتب(سجل["اول"])
```

Expected output: `4`. See [functions](../language/functions.md) and [types](../language/types-and-values.md).

## 4. Imports and errors

Learn an imported function and an assertion. Import executes the module at runtime.

```fa
من رياضيات استورد عاملي
القيمة := عاملي(5)
تاكد القيمة = 120
اكتب(القيمة)
```

Expected output: `120`. See [imports](../language/modules-and-imports.md) and [errors](../language/errors.md).
