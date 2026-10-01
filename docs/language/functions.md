# Functions and classes

```fa
دالة جمع(ا، ب):
    ارجع ا + ب

نوع عداد:
    دالة بداية(قيمة):
        .قيمة := قيمة
    دالة زد():
        هذا.قيمة += 1
```

Functions have positional parameters with no defaults or annotations. Calls require the declared number of arguments. A return transfers its value to the caller; reaching the end returns nil. Functions can refer to enclosing bindings through the compiler's closure machinery. A function body has its own local bindings; see [scope](scope-and-bindings.md).

`نوع Name:` defines a class. `نوع Child(Parent):` names a single parent. Class suites contain method definitions. Methods use `هذا` for the receiving instance; `.field := value` is a method-body shorthand for its field. `بداية` is the constructor method. Operator-named methods such as `دالة عملية+(...)` are recognized for arithmetic dispatch. Class field layout is inferred during compilation from assignments. A method can contain ordinary statements after the shorthand assignment.

**Current implementation.** The parser special-cases `.field` and operator method names in [fparser.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fparser.cc); the compiler resolves class fields in [fcompiler.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fcompiler.cc). Class layout and operator dispatch should not be treated as a general dynamic-property guarantee.

## Inline functions, methods, and classes

A function or method with one body statement can put that statement after `:`. A class with one method can put the method definition after its own colon. The class body still accepts methods, not arbitrary executable statements. Shorthand field assignments work in inline methods as well as directly in indented method bodies.

```fa
دالة جمع(ا، ب): ارجع ا + ب
نوع علبة: دالة بداية(قيمة): .قيمة := قيمة

نوع عداد:
    دالة بداية(): .قيمة := 0
    دالة زد(): .قيمة += 1
    دالة اقرأ(): ارجع هذا.قيمة

علبتي := علبة(جمع(3، 4))
عدادتي := عداد()
عدادتي.زد()
اكتب(علبتي.قيمة)
اكتب(عدادتي.اقرأ())
```

Output: `7`, then `1`. A class with multiple methods uses an indented class body; individual methods can mix inline and indented bodies. The final newline is optional. A bare `ارجع` returns `عدم` in either form. Blank lines between methods do not change class membership.

These forms preserve parameter, field, and scope behavior. See [statements](statements.md) for inline conditions and loops, clause alignment, and invalid forms.
