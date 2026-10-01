# Memory, references, and mutation

Lists, dictionaries, instances, modules, and other objects are references. Assigning a value to a second name does not copy a mutable object:

```fa
ا := [1]
ب := ا
اضف(ب، 2)
اكتب(طول(ا))
```

Expected output: `2`. Use a library copy helper such as `مجموعات.انسخ_قائمة` when a distinct list is needed. String values are immutable. Nested containers can form cycles; equality and printing have cycle handling in the current runtime.

**Current implementation.** Allocation is managed by a mark-and-sweep collector in [fgc.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fgc.cc). It tracks roots from VM state and objects. Object headers are defined in [fobj_header.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fobj_header.hpp). Collection timing, layout, and identity addresses are implementation details. There is no source-level destructor or manual free operation.
