# Module loading

An imported `اسم` resolves to `اسم.ف`; `حساب.ادوات` resolves to `حساب/ادوات.ف`. The current loader checks, in order, `FAIRUZ_STDLIB` when set, the build-time source-tree stdlib, the configured installation stdlib, the importing file's directory, then the process working directory. Bundled modules can therefore take precedence over a local file of the same name.

Each resolved path is loaded once per VM and has its own global environment. Initialization is reached at the import statement, not at parse time. Imported functions keep the globals of their defining module. A failed lookup or import produces a runtime error. Cyclic-import behavior has not been established as a stable contract; see [unresolved](../documentation/unresolved.md).

**Current implementation.** Search, caching, and import execution are in [fvm.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc). Relocated installations should set `FAIRUZ_STDLIB` explicitly.
