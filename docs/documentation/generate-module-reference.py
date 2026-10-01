#!/usr/bin/env python3
"""Refresh source-derived symbol inventories; review semantics by hand afterward."""
from pathlib import Path
import argparse
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lang", choices=("en", "ar"), default="en")
LANG = parser.parse_args().lang
ROOT = Path(__file__).resolve().parents[2]
LIB = ROOT / "stdlib"
DOC_ROOT = ROOT / ("docs_ar" if LANG == "ar" else "docs")
OUT = DOC_ROOT / "standard-library" / "modules"
OUT.mkdir(parents=True, exist_ok=True)
SOURCE_BASE = "https://github.com/mohamed-labbit/fairuz/blob/86d706f83f9adc67cf2f4828a40dd6c11a3fb9b4/stdlib/"
INDEX = (["# وحدات المكتبة القياسية", "", "تحصي كل صفحة التصريحات الموجودة في ملف `.ف` المقابل. وجود التصريح لا يثبت توافر كل دالة أصلية يستدعيها. الوسائط موضعية، ولا توجد قيم افتراضية في نحو اللغة. الأسماء التي تبدأ بـ`__` داخلية، لذلك حُذفت من الفهرس. راجع [المسائل غير المحسومة](../documentation/unresolved.md).", ""] if LANG == "ar" else ["# Standard library modules", "", "Each page lists source-level declarations in its `.ف` file. A declaration does not prove that every runtime hook it calls exists. Signatures contain positional parameters only; there are no source-level defaults. `__` names are internal and omitted here. See [unresolved behavior](../documentation/unresolved.md).", ""])

for path in sorted(LIB.glob("*.ف")):
    name = path.stem
    lines = path.read_text(encoding="utf-8").splitlines()
    declarations = []
    current_class = None
    for i, line in enumerate(lines):
        top = re.match(r"^(دالة|نوع)\s+([^(:]+)(?:\(([^)]*)\))?:", line)
        if top:
            current_class = top.group(2) if top.group(1) == "نوع" else None
            if not top.group(2).startswith("__"):
                declarations.append((i, top.group(1), top.group(2), top.group(3) or "", None))
            continue
        method = re.match(r"^    دالة\s+([^(:]+)(?:\(([^)]*)\))?:", line)
        if current_class and method and not method.group(1).startswith("__"):
            declarations.append((i, "دالة", method.group(1), method.group(2) or "", current_class))
    # Include module-level bindings: these are import-visible even without an export list.
    constants = []
    for i, line in enumerate(lines):
        match = re.match(r"^([^\s#:=()]+)\s*:=", line)
        if match and not match.group(1).startswith("__"):
            constants.append((i, match.group(1)))
    source_url = SOURCE_BASE + path.name
    if LANG == "ar":
        text = [f"# `{name}`", "", f"استورد الوحدة بـ`استورد {name} باسم م`، أو استورد اسمًا منها بـ`من {name} استورد اسم`. [الشيفرة المصدرية]({source_url}).", "", "**تفصيل التنفيذ الحالي.** تسرد الصفحة التصريحات الموجودة في الملف؛ لا تضمن عمل كل تبعية أو دالة أصلية. الاستدعاء يستخدم وسائط موضعية، وينتقل خطأ الدالة المستدعاة إلى المستدعي لأن اللغة لا توفر التقاط الاستثناءات. الترميز `Class.method(...)` في الجدول توضيحي لطريقة داخل صنف.", ""]
    else:
        text = [f"# `{name}`", "", f"Import with `استورد {name} باسم م` or `من {name} استورد symbol`. [Source]({source_url}).", "", "**Current implementation.** The entries below are declarations in the current module, not a guarantee that every dependency or native hook works. Calls use positional arguments. A runtime error in a called builtin or dependent module propagates; source-level exception recovery is unavailable. In the table, `Class.method(...)` is explanatory notation for a method declared inside that class.", ""]
    if declarations:
        text += (["| الاسم والتوقيع | المصدر | قرائن من المتن |", "|---|---|---|"] if LANG == "ar" else ["| Symbol and source signature | Source | Observable body clues |", "|---|---|---|"])
        for n, (i, kind, symbol, params, owner) in enumerate(declarations):
            end = declarations[n + 1][0] if n + 1 < len(declarations) else len(lines)
            body = lines[i + 1:end]
            # A short factual summary helps readers find the exact code path without inventing semantics.
            returns = [s.strip()[5:].strip() for s in body if s.lstrip().startswith("ارجع ")]
            calls_error = any("عطل(" in s for s in body)
            clue = (("يعيد في أحد المسارات: `" if LANG == "ar" else "Returns: `") + returns[0].replace("|", "\\|")[:90] + "`" if returns else ("راجع المصدر لتحديد مسار الإرجاع" if LANG == "ar" else "See source for return path"))
            if calls_error:
                clue += ("؛ يستدعي `عطل` في أحد المسارات" if LANG == "ar" else "; calls `عطل` on some path")
            signature = f"`{kind} {owner + '.' if owner else ''}{symbol}({params})`" if kind == "دالة" else f"`نوع {symbol}`"
            line_label = "السطر" if LANG == "ar" else "line"
            text.append(f"| {signature} | [{line_label} {i+1}]({source_url}#L{i+1}) | {clue} |")
    else:
        text += ["لم يُعثر على تصريحات دوال أو أصناف عامة في المستوى الأعلى." if LANG == "ar" else "No public top-level function or class declarations were found."]
    if constants:
        text += (["", "## ارتباطات المستوى الأعلى", "", "تُسند هذه القيم أثناء تهيئة الوحدة. يحدد المصدر المرتبط تعبير الإسناد ونوع القيمة.", "", "| الاسم | المصدر |", "|---|---|"] if LANG == "ar" else ["", "## Module-level bindings", "", "These values are assigned at module initialization. The assignment expression and its type are defined by the linked source.", "", "| Name | Source |", "|---|---|"])
        for i, symbol in constants:
            line_label = "السطر" if LANG == "ar" else "line"
            text.append(f"| `{symbol}` | [{line_label} {i+1}]({source_url}#L{i+1}) |")
    if declarations or constants:
        sample = declarations[0][2] if declarations else constants[0][1]
        text += ["", "## " + ("مثال الاستيراد" if LANG == "ar" else "Import example"), "", "```fa", f"استورد {name} باسم م", f"اكتب(صنف(م.{sample}))", "```", "", ("يوضح المثال البحث عن الاسم وفحص نوعه. يتحقق الفاحص من تشغيله، لكن نجاحه لا يثبت عمل الدوال في جميع مساراتها. راجع [التغطية](../../documentation/coverage.md)." if LANG == "ar" else "This example demonstrates lookup and type inspection. The validator checks that it runs, but that does not establish every behavior of the module. See [coverage](../../documentation/coverage.md).")]
    (OUT / f"{name}.md").write_text("\n".join(text) + "\n", encoding="utf-8")
    INDEX.append(f"- [`{name}`](modules/{name}.md) — {len(declarations)} " + (f"تصريحًا عامًا و{len(constants)} ارتباطًا في المستوى الأعلى" if LANG == "ar" else f"public declarations, {len(constants)} module-level bindings"))

(DOC_ROOT / "standard-library" / "index.md").write_text("\n".join(INDEX) + "\n", encoding="utf-8")

if LANG == "ar":
    sections = [
        ("البدء", [("البناء والتثبيت", "getting-started/installation.md"), ("البرنامج الأول", "getting-started/first-program.md"), ("تشغيل البرامج", "getting-started/running-programs.md")]),
        ("الدروس", [("دروس فيروز", "tutorial/index.md")]),
        ("اللغة", [("المدخل", "language/index.md"), ("البنية المعجمية", "language/lexical-structure.md"), ("القيم والأنواع", "language/types-and-values.md"), ("التعبيرات", "language/expressions.md"), ("التعليمات", "language/statements.md"), ("الدوال والأصناف", "language/functions.md"), ("النطاقات", "language/scope-and-bindings.md"), ("الوحدات والاستيراد", "language/modules-and-imports.md"), ("الأخطاء", "language/errors.md")]),
        ("وقت التشغيل", [("المدخل", "runtime/index.md"), ("آلية التنفيذ", "runtime/execution-model.md"), ("سلوك الأعداد", "runtime/numeric-behavior.md"), ("الذاكرة والمراجع", "runtime/memory-management.md"), ("تحميل الوحدات", "runtime/module-loading.md")]),
        ("المكتبة القياسية", [("فهرس الوحدات", "standard-library/index.md"), ("الدوال المدمجة", "standard-library/builtins.md")] + [(f"`{p.stem}`", f"standard-library/modules/{p.name}") for p in sorted(OUT.glob("*.md"))]),
        ("التنفيذ الداخلي", [("المدخل", "implementation/index.md"), ("البنية", "implementation/architecture.md")]),
        ("صيانة التوثيق", [("المساهمة", "documentation/contributing.md"), ("التغطية", "documentation/coverage.md"), ("المسائل غير المحسومة", "documentation/unresolved.md")]),
    ]
    nav = ["# BEGIN GENERATED NAV", "nav:", "  - الرئيسية: index.md"]
    for section, pages in sections:
        nav.append(f"  - {section}:")
        for title, target in pages:
            nav.append(f"      - '{title}': {target}")
    nav.append("# END GENERATED NAV")
    config = ROOT / "mkdocs_ar.yml"
    contents = config.read_text(encoding="utf-8")
    contents = re.sub(r"\n?# BEGIN GENERATED NAV.*?# END GENERATED NAV\n?", "\n", contents, flags=re.S).rstrip()
    config.write_text(contents + "\n" + "\n".join(nav) + "\n", encoding="utf-8")
