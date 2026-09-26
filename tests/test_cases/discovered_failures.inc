// Confirmed failures from the September 2026 interpreter bug hunt.
// Structural regressions for existing interpreter and standard-library behavior.
// Fairuz sources run independently in the existing process-isolated harness.
// See discovered_failures.md for the oracle and reproduction command.

// NaN equality.
// NaN compares unequal to itself and independently computed NaNs.
{ "NaNSelfEquality", R"fa(ن := قوة(-1.0، 0.5)
اكتب(ن = ن)
)fa",
    "خطا\n" },

{ "NaNSelfInequality", R"fa(ن := قوة(-1.0، 0.5)
اكتب(ن != ن)
)fa",
    "صحيح\n" },

{ "NaNSeparateEquality", R"fa(ن := قوة(-1.0، 0.5)
اكتب(ن = قوة(-1.0، 0.5))
)fa",
    "خطا\n" },

// Decimal literal parsing.
// Arabic digits and underscores must retain the full numeric value.
{ "ArabicFraction", R"fa(اكتب(١.٥)
)fa",
    "1.5\n" },

{ "ArabicNegativeFraction", R"fa(اكتب(-١٢.٥)
)fa",
    "-12.5\n" },

{ "DecimalSeparators", R"fa(اكتب(1_234.5)
)fa",
    "1234.5\n" },

{ "ArabicFractionWithoutLeadingDigits", R"fa(اكتب(٠.٥)
)fa",
    "0.5\n" },

{ "FractionSeparator", R"fa(اكتب(12.3_4)
)fa",
    "12.34\n" },

// Exponentiation syntax.
// The POWER production in grammar.ebnf supports ** and right associativity.
{ "PowerOperator", R"fa(اكتب(2 ** 10)
)fa",
    "1024\n" },

{ "PowerRightAssociative", R"fa(اكتب(2 ** 3 ** 2)
)fa",
    "512\n" },

{ "PowerNegativeExponent", R"fa(اكتب(2 ** -3)
)fa",
    "0.125\n" },

// NUL codepoints and JSON.
// U+0000 is a character; decoding and round trips must preserve it, including dictionary keys.
{ "EmbeddedNulCodepointLength", R"fa(اكتب(طول(__نص_من_رمز__(0)))
)fa",
    "1\n" },

{ "JsonNulLength", R"fa(من جيسون استورد JSON_حلل
اكتب(طول(JSON_حلل("\"\\u0000\"")))
)fa",
    "1\n" },

{ "JsonInteriorNulLength", R"fa(من جيسون استورد JSON_حلل
اكتب(طول(JSON_حلل("\"a\\u0000b\"")))
)fa",
    "3\n" },

{ "JsonNulAndEmojiLength", R"fa(من جيسون استورد JSON_حلل
اكتب(طول(JSON_حلل("\"\\u0000\\ud83d\\ude42\"")))
)fa",
    "2\n" },

{ "JsonNulRoundTrip", R"fa(من جيسون استورد JSON_حلل
من جيسون استورد JSON_رمز
ق := JSON_حلل("\"a\\u0000b\"")
اكتب(JSON_رمز(ق))
)fa",
    "\"a\\u0000b\"\n" },

{ "JsonNulKeyDistinct", R"fa(من جيسون استورد JSON_حلل
من جيسون استورد JSON_رمز
ق := JSON_حلل("{\"a\\u0000b\":1,\"ab\":2}")
اكتب(طول(ق))
)fa",
    "2\n" },

{ "JsonNulKeyLength", R"fa(من جيسون استورد JSON_حلل
من جيسون استورد JSON_رمز
ق := JSON_حلل("{\"\\u0000\":1}")
اكتب(طول(__قاموس_مفاتيح__(ق)[0]))
)fa",
    "1\n" },

{ "BinaryJsonNulRoundTrip", R"fa(من جيسون استورد JSON_حلل
من جيسون استورد JSON_رمز
ق := __16_فك__("610062")
اكتب(__16_رمز__(JSON_حلل(JSON_رمز(ق))))
)fa",
    "610062\n" },

{ "NulStringConcatenation", R"fa(اكتب(__16_رمز__("a" + __نص_من_رمز__(0) + "b"))
)fa",
    "610062\n" },

// Unicode slicing.
// Slice indexes refer to characters, consistently with string indexing, length, and substring. Hex output exposes split UTF-8 sequences.
{ "UnicodeSliceSingleEmoji", R"fa(اكتب(__16_رمز__(مقطع("أ🙂ب"، 1، 1)))
)fa",
    "f09f9982\n" },

{ "UnicodeSliceToEnd", R"fa(اكتب(__16_رمز__(مقطع("أ🙂ب"، 1)))
)fa",
    "f09f9982d8a8\n" },

{ "UnicodeSliceLastCharacter", R"fa(اكتب(__16_رمز__(مقطع("أ🙂ب"، 2، 2)))
)fa",
    "d8a8\n" },

// Calls through module values.
// Moving a module through aliases, parameters, returns, or containers must preserve callable attributes.
{ "ModuleCopiedAlias", R"fa(استورد رياضيات باسم ر
نسخة := ر
اكتب(نسخة.عاملي(5))
)fa",
    "120\n" },

{ "ModulePassedAsArgument", R"fa(استورد رياضيات باسم ر
دالة ناد(م):
    ارجع م.عاملي(5)
اكتب(ناد(ر))
)fa",
    "120\n" },

{ "ModuleInList", R"fa(استورد رياضيات باسم ر
ق := [ر]
اكتب(ق[0].عاملي(5))
)fa",
    "120\n" },

{ "ModuleReturnedFromFunction", R"fa(استورد رياضيات باسم ر
دالة خذ():
    ارجع ر
اكتب(خذ().عاملي(5))
)fa",
    "120\n" },

{ "ModuleInDictionary", R"fa(استورد رياضيات باسم ر
ق := {"module": ر}
اكتب(ق["module"].عاملي(5))
)fa",
    "120\n" },

{ "ModuleInInstanceField", R"fa(استورد رياضيات باسم ر
نوع علبة:
    دالة بداية(م):
        هذا.م := م
س := علبة(ر)
اكتب(س.م.عاملي(5))
)fa",
    "120\n" },

// Import-name shadowing.
// A local parameter or reassigned value must determine method dispatch, regardless of an earlier import name.
{ "ModuleNameShadowedByInstance", R"fa(استورد رياضيات باسم ر
نوع علبة:
    دالة عاملي(س):
        ارجع س + 1
دالة ناد(ر):
    ارجع ر.عاملي(5)
اكتب(ناد(علبة()))
)fa",
    "6\n" },

{ "ModuleNameReboundToInstance", R"fa(استورد رياضيات باسم ر
نوع علبة:
    دالة عاملي(س):
        ارجع س + 1
ر := علبة()
اكتب(ر.عاملي(5))
)fa",
    "6\n" },

// Callable instance fields.
// Externally calling a function stored in a field must agree with the supported internal this.field call.
{ "ExternalCallableFieldFunction", R"fa(دالة ضاعف(س):
    ارجع س * 2
نوع علبة:
    دالة بداية(الدالة):
        هذا.الدالة := الدالة
س := علبة(ضاعف)
اكتب(س.الدالة(5))
)fa",
    "10\n" },

{ "ExternalCallableFieldNative", R"fa(دالة ضاعف(س):
    ارجع س * 2
نوع علبة:
    دالة بداية(الدالة):
        هذا.الدالة := الدالة
س := علبة(طول)
اكتب(س.الدالة([1، 2، 3]))
)fa",
    "3\n" },

{ "ExternalCallableFieldConstructor", R"fa(دالة ضاعف(س):
    ارجع س * 2
نوع علبة:
    دالة بداية(الدالة):
        هذا.الدالة := الدالة
س := علبة(قائمة)
اكتب(س.الدالة(1، 2))
)fa",
    "[1, 2]\n" },

{ "CallableFieldInternalExternalAgree", R"fa(نوع علبة:
    دالة بداية(الدالة):
        هذا.الدالة := الدالة
    دالة ناد(ق):
        ارجع هذا.الدالة(ق)
س := علبة(طول)
اكتب(س.ناد([1، 2]))
اكتب(س.الدالة([1، 2]))
)fa",
    "2\n2\n" },

// Deep-copy identity.
// Equal but distinct mutable children must remain distinct after a deep copy; the original also stays unchanged.
{ "DeepCopyEqualDistinctLists", R"fa(من نسخ استورد نسخ_عميق
اصل := [[1]، [1]]
نسخة := نسخ_عميق(اصل)
نسخة[0][0] := 9
اكتب(نسخة[1][0])
اكتب(اصل[0][0])
)fa",
    "1\n1\n" },

{ "DeepCopyEqualDistinctDicts", R"fa(من نسخ استورد نسخ_عميق
اصل := [{"x": 1}، {"x": 1}]
نسخة := نسخ_عميق(اصل)
نسخة[0]["x"] := 9
اكتب(نسخة[1]["x"])
)fa",
    "1\n" },

{ "DeepCopyEqualEmptyLists", R"fa(من نسخ استورد نسخ_عميق
نسخة := نسخ_عميق([[]، []])
اضف(نسخة[0]، 1)
اكتب(طول(نسخة[1]))
)fa",
    "0\n" },

// Fraction normalization.
// The fraction constructor reduces common factors, moves the sign to the numerator, and normalizes zero.
{ "FractionReducesSmall", R"fa(من أعداد استورد كسر
اكتب(كسر(2، 6).نص())
)fa",
    "1/3\n" },

{ "FractionNormalizesSign", R"fa(من أعداد استورد كسر
اكتب(كسر(1، -3).نص())
)fa",
    "-1/3\n" },

{ "RationalReciprocalCancellation", R"fa(من أعداد استورد كسر
اكتب(كسر(3، 5).اضرب(كسر(5، 3)).نص())
)fa",
    "1\n" },

{ "FractionZeroDenominatorNormalized", R"fa(من أعداد استورد كسر
اكتب(كسر(0، -9).المقام)
)fa",
    "1\n" },

{ "FractionCancellation", R"fa(من أعداد استورد كسر
اكتب(كسر(1، 3).اطرح(كسر(1، 3)).نص())
)fa",
    "0\n" },

{ "FractionAdditionReduced", R"fa(من أعداد استورد كسر
اكتب(كسر(1، 3).اجمع(كسر(1، 6)).نص())
)fa",
    "1/2\n" },

{ "FractionDivisionReduced", R"fa(من أعداد استورد كسر
اكتب(كسر(2، 3).اقسم(كسر(4، 9)).نص())
)fa",
    "3/2\n" },

// JSON number grammar.
// Reject leading zeros, missing integer digits, and missing fractional digits.
{ "JsonRejectsLeadingZero", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("01"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsNegativeLeadingZero", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("-01"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsLeadingDot", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل(".5"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsTrailingDot", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("1."))
)fa",
    "", Expectation::Error },

{ "JsonRejectsExponentLeadingDot", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل(".5e2"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsNegativeLeadingDot", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("-.5"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsTrailingDotExponent", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("1.e2"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsNegativeLeadingZeros", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("-000"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsLeadingZeroExponent", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("01e2"))
)fa",
    "", Expectation::Error },

{ "JsonRejectsLeadingZeroFraction", R"fa(من جيسون استورد JSON_حلل
اكتب(JSON_حلل("01.5"))
)fa",
    "", Expectation::Error },

// CSV preservation.
// Preserve quoted empty fields and carriage returns, including encode/decode round trips.
{ "CsvEmptyQuotedField", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
اكتب(CSV_حلل("\"\""، ","))
)fa",
    "[[\"\"]]\n" },

{ "CsvEmptyFieldRoundTrip", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
اكتب(CSV_حلل(CSV_رمز([[""]]، ",")، ",") = [[""]])
)fa",
    "صحيح\n" },

{ "CsvTwoEmptyRowsRoundTrip", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
اكتب(CSV_حلل(CSV_رمز([[""]، [""]]، ",")، ",") = [[""]، [""]])
)fa",
    "صحيح\n" },

{ "CsvLastQuotedEmptyRow", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
اكتب(CSV_حلل("a\n\"\""، ",") = [["a"], [""]])
)fa",
    "صحيح\n" },

{ "CsvQuotedCrBeforeLf", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
اكتب(CSV_حلل("\"a\r\"\n"، ",") = [["a\r"]])
)fa",
    "صحيح\n" },

{ "CsvCrRoundTripBetweenRows", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
ق := [["a\r"]، ["b"]]
اكتب(CSV_حلل(CSV_رمز(ق، ",")، ",") = ق)
)fa",
    "صحيح\n" },

{ "CsvEncoderQuotesCarriageReturn", R"fa(من قيم_مفصولة استورد CSV_حلل
من قيم_مفصولة استورد CSV_رمز
اكتب(__16_رمز__(CSV_رمز([["a\r"]]، ",")))
)fa",
    "22610d22\n" },

// CSV quote validation.
// The module explicitly advertises RFC 4180: quotes may only surround fields, and closed quoted fields cannot have trailing data.
{ "CsvRejectsQuoteInsideUnquoted", R"fa(من قيم_مفصولة استورد CSV_حلل
اكتب(CSV_حلل("a\"b\""، ","))
)fa",
    "", Expectation::Error },

{ "CsvRejectsDataAfterClosingQuote", R"fa(من قيم_مفصولة استورد CSV_حلل
اكتب(CSV_حلل("\"a\"b"، ","))
)fa",
    "", Expectation::Error },

// Binary file reading.
// Counted, whole-file, and line reads must retain NUL bytes already accepted by the matching write API.
{ "FileReadPreservesNul", R"fa(مسار := __ملف_مؤقت__("fairuz-regression-"، ".bin"، عدم)["path"]
م := __ملف_افتح__(مسار، "كتابة")
تاكد(__ملف_اكتب__(م، __16_فك__("610062")))
تاكد(__ملف_اغلق__(م))
م := __ملف_افتح__(مسار، "قراءة")
ق := __ملف_اقرا__(م، 3)
تاكد(__ملف_اغلق__(م))
تاكد(__مسار_احذف__(مسار))
اكتب(__16_رمز__(ق))
)fa",
    "610062\n" },

{ "FileReadAllPreservesNul", R"fa(مسار := __ملف_مؤقت__("fairuz-regression-"، ".bin"، عدم)["path"]
م := __ملف_افتح__(مسار، "كتابة")
تاكد(__ملف_اكتب__(م، __16_فك__("610062")))
تاكد(__ملف_اغلق__(م))
م := __ملف_افتح__(مسار، "قراءة")
ق := __ملف_اقرا_الكل__(م)
تاكد(__ملف_اغلق__(م))
تاكد(__مسار_احذف__(مسار))
اكتب(__16_رمز__(ق))
)fa",
    "610062\n" },

{ "FileReadLinePreservesNul", R"fa(مسار := __ملف_مؤقت__("fairuz-regression-"، ".bin"، عدم)["path"]
م := __ملف_افتح__(مسار، "كتابة")
تاكد(__ملف_اكتب__(م، __16_فك__("610062")))
تاكد(__ملف_اغلق__(م))
م := __ملف_افتح__(مسار، "قراءة")
ق := __ملف_اقرا_سطر__(م)
تاكد(__ملف_اغلق__(م))
تاكد(__مسار_احذف__(مسار))
اكتب(__16_رمز__(ق))
)fa",
    "610062\n" },

// Approximate assertions with NaN.
// An unordered NaN operand cannot satisfy an approximate-equality assertion.
{ "AssertionNearRejectsNaNActual", R"fa(من اختبار استورد تاكد_قريب
اكتب(تاكد_قريب(قوة(-1.0، 0.5)، 1.0، 0.001))
)fa",
    "", Expectation::Error },

{ "AssertionNearRejectsNaNExpected", R"fa(من اختبار استورد تاكد_قريب
اكتب(تاكد_قريب(1.0، قوة(-1.0، 0.5)، 0.001))
)fa",
    "", Expectation::Error },

// Identity must not collapse primitive or distinct reference values.
{ "NilAndBooleanEquality", R"fa(اكتب(عدم = عدم)
اكتب(صحيح = صحيح)
اكتب(خطا = خطا)
اكتب(صحيح = خطا)
اكتب(صحيح = 1)
)fa",
    "صحيح\nصحيح\nصحيح\nخطا\nخطا\n" },

{ "DistinctClassIdentity", R"fa(نوع اول:
    دالة قيمة():
        ارجع 1
نوع ثان:
    دالة قيمة():
        ارجع 1
اكتب(اول = اول)
اكتب(اول = ثان)
)fa",
    "صحيح\nخطا\n" },

{ "DeepCopyPreservesSharedChild", R"fa(من نسخ استورد نسخ_عميق
طفل := [1]
نسخة := نسخ_عميق([طفل، طفل])
نسخة[0][0] := 7
اكتب(نسخة[1][0])
اكتب(طفل[0])
)fa",
    "7\n1\n" },

{ "DeepCopyPreservesCycle", R"fa(من نسخ استورد نسخ_عميق
اصل := []
اضف(اصل، اصل)
نسخة := نسخ_عميق(اصل)
اضف(نسخة، 7)
اكتب(طول(نسخة[0]))
اكتب(طول(اصل))
)fa",
    "2\n1\n" },
