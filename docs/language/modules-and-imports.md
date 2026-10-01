# Modules and imports

```fa
استورد مجموعات باسم مج
من رياضيات استورد عاملي باسم factorial
من نتيجة استورد (نجاح، فشل)
```

`استورد module` binds the module object under its last dotted component; `باسم alias` chooses another local name. `من module استورد name، other` binds selected exported values, each optionally followed by `باسم alias`. A parenthesized selected-name list permits line breaks. Dotted module names map to directory components and a `.ف` file.

Imports execute at runtime and initialize each resolved path once per VM. There is no explicit export declaration; module globals are looked up by name. An absent module or symbol raises an error. Search order and cycle behavior are in [module loading](../runtime/module-loading.md). `--check` compiles the importing file but does not execute its imports.
