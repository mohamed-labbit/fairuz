# بناء فيروز وتثبيتها

تحتاج فيروز إلى CMake 3.14 أو أحدث، ومترجم يدعم ++C23، وملفات تطوير zlib، ومكتبة simdutf. قد يجلب CMake مكتبتَي simdutf وGoogleTest إذا لم تكونا مخزّنتين محليًا. من جذر المستودع:

```sh
./build.sh
./build/fairuz examples/hello.fa
```

للتثبيت، حدّد البادئة **قبل** البناء حتى يُسجَّل مسار المكتبة القياسية الصحيح:

```sh
cmake -S . -B build-install -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/fairuz
cmake --build build-install --target fairuz -j4
cmake --install build-install
/tmp/fairuz/bin/fairuz examples/hello.fa
```

إذا نُقل التثبيت، عيّن `FAIRUZ_STDLIB` إلى مجلد `share/fairuz/stdlib` الخاص به. تشرح صفحة [تشغيل البرامج](running-programs.md) خيارات سطر الأوامر. لا يوفر الملف التنفيذي الحالي وضعًا تفاعليًا.
