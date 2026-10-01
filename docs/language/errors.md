# Errors and diagnostics

Lexical and parse errors, such as malformed indentation, an invalid escape, a missing delimiter, or `1 := 2`, stop successful compilation. The parser can recover enough to report more than one syntax diagnostic; recovery does not make the program executable. The compiler rejects `ارجع` outside functions and loop control outside loops. Runtime errors include missing names/modules, wrong argument counts or types, invalid indexes, and division by zero.

`تاكد condition` or `تاكد condition، message` evaluates an assertion; false conditions raise a runtime error. `عطل(message)` raises an error directly. There is no source-level `try`/`catch` syntax in the parser, so programs cannot generally recover from a raised runtime error. CLI diagnostics are printed to standard error; `--diagnostics=json` selects structured output. Error message text is not a documented stable API.

```fa
تاكد 2 + 2 = 4
اكتب("ok")
```

Expected output: `ok`. See [running programs](../getting-started/running-programs.md).
