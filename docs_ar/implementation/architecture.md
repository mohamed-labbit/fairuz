# بنية المشروع

```text
ملف UTF-8 ← إدارة المصدر ← المحلل المعجمي ← المحلل النحوي/AST ← المترجم/Chunk ← الآلة الافتراضية
                                                                       ↓
                                                    القيم والكائنات ← الدوال الأصلية/المكتبة
                                                                       ↓
                                                             جامع القمامة
```

ينتج [المحلل المعجمي](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/flexer.cc) الرموز وأحداث الإزاحة بالاعتماد على [جدول المحارف](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fctype.hpp). يبني [المحلل النحوي](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fparser.cc) العقد المعرفة في [fAST.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fAST.hpp). يحل [المترجم](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fcompiler.cc) الأسماء ويصدر التعليمات المعرفة في [fopcode.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fopcode.hpp). تنفذها [الآلة الافتراضية](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvm.cc) وتحمل الوحدات وترفع أخطاء التشغيل.

تحدد [fvalue.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fvalue.hpp) و[fobject.hpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fobject.hpp) و[finteger.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/finteger.cc) القيم والكائنات ومسارات الأعداد. يجمع [fgc.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fgc.cc) غير القابل للوصول. تسجل [fbuiltins.cc](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/fairuz/fbuiltins.cc) الدوال الأصلية؛ وكل ملف `.ف` في [stdlib](https://github.com/mohamed-labbit/fairuz/tree/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/stdlib) وحدة فيروز. تنفذ [main.cpp](https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/main.cpp) أوضاع سطر الأوامر. تحتوي [tests](https://github.com/mohamed-labbit/fairuz/tree/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/tests) اختبارات ++C وحالات مصدرية، وفي `stdlib/tests` برامج اختبار قابلة للتنفيذ. بعد بناء الاختبارات شغّل `ctest --test-dir build --output-on-failure`.

يستخدم التنفيذ الحالي قيمًا مضغوطة بأسلوب NaN على المنصات المدعومة، مع تمثيل بديل عند الحاجة. هذه البنية وصيغة التعليمات الوسيطة تفاصيل داخلية.
