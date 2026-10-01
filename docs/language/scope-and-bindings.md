# Scope and bindings

Top-level assignments bind module globals. Functions have parameter and local bindings; nested functions can capture enclosing bindings. Imports bind the imported module or selected names in the importing module's scope. Imported functions retain their defining module's globals. Name lookup failure is a runtime or compile diagnostic depending on where the compiler can resolve it.

An `اذا`, `طالما`, or `لكل` body is either an indented statement group or one inline statement. Choosing the inline form does not change its binding behavior. It does not introduce a documented general block namespace. Reassignment of an existing name changes that binding. A loop variable is a name binding, not a copy of the list element. Aliasing of mutable values is described in [types](types-and-values.md).

**Current implementation.** Resolution and capture are in [fcompiler.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fcompiler.cc); module global environments are in [fvm.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc). Details of rebinding captured variables need focused tests before being stated as a broader guarantee; see [unresolved](../documentation/unresolved.md).
