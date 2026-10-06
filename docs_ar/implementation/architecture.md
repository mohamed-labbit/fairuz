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

## بناء AST والتحليل المؤجل لأجسام الدوال

ينشئ `Parser` عقد التعبيرات والتعليمات عبر `AST::ASTBuilder` في
`fairuz/fASTBuilder.hpp`. تستخدم العقد والرموز وأسماؤها عمر الذاكرة الحالي في
الساحة (arena)، ويعيد `parse_program()` تعليمات المستوى الأعلى. لا يملك الباني
برنامجًا آخر أو مخصص ذاكرة مستقلًا.

يحلل الوضع الافتراضي `BodyParsing::Lazy` اسم الدالة ومعاملاتها فورًا. يبقى تعريفها
عقدة `FuncDefStmt` ذات جسم غير فارغ، لكن الجسم يبدأ بعقدة `FunctionStub` من النوع
المستقل `FUNCTION_STUB`، فلا تختلط عقدة الجسم المؤجل بعقدة تعريف الدالة عند النسخ
أو الزيارة أو التحويل.

يمسح المحلل رموز الجسم لتحديد حدوده دون بناء شجرة تعليماته وتعبيراته. يدعم الأجسام
في السطر نفسه والكتل المزاحة والإزاحة المتداخلة والتعليقات والتعبيرات متعددة الأسطر
ونهاية الملف. تحتفظ العقدة المؤجلة بسلسلة رموز محدودة تنتهي بـ `ENDMARKER`، ونص
الجسم، ونسخة المصدر الثابتة. تبقى مواقع الرموز بإزاحات UTF-8 وأرقام الأسطر والأعمدة
الأصلية. التأجيل يخص التحليل النحوي، لا التجزئة المعجمية؛ لذا تظهر الأخطاء المعجمية
وأخطاء الإزاحة أثناء المرور الأول.

عند أول استدعاء، يطلب المترجم المؤجل من `ASTBuilder::materialize()` تحليل الرموز
المحفوظة. لا تُحفظ شجرة الجسم إلا بعد نجاح التحليل كاملًا، ثم تُبنى التعليمات
الوسيطة دون اعتماد نتيجة جزئية عند الفشل. فشل التحليل يبقي الجسم مؤجلًا، وفشل
الترجمة يبقي المقطع بلا تعليمات جزئية. يمكن إعادة استخدام الشجرة الصحيحة عند
إعادة محاولة الترجمة. لا يلزم بقاء المحلل أو مدير الملف أو المترجم الأصلي حيًا.

للحصول على شجرة كاملة استخدم `Parser(&file, BodyParsing::Eager)`، وهو الوضع الذي
يستخدمه التنسيق والتلوين الدلالي و`--dump-ast`. يستدعي `--check` الدالة `Compiler::compile_all()`
لتحليل وترجمة الأجسام غير المستعملة أيضًا دون تنفيذ البرنامج أو استيراداته.
تبقى طرائق الأنواع محللة فورًا لأن إسناداتها تحدد حقول النوع، لكن ترجمة تعليماتها
الوسيطة تؤجل إلى أول استدعاء. لا يدعم المترجم تعريف الدوال المتداخلة.

تغطي الاختبارات الجديدة هذه المسارات في `tests/test_lazy_parsing.cpp`، إلى جانب
`tests/test_lazy_compilation.cpp` واختبارات المحلل وسطر الأوامر والتنسيق:

```sh
cmake --build build --target fairuz_tests -j4
./build/fairuz_tests --gtest_filter='LazyParsing.*:LazyCompilation.*:ParserTest.*:CliE2E.*:Formatter.*'
ctest --test-dir build --output-on-failure
```
