
#include "../fbuiltins.hpp"
#include "../fvm.hpp"

#include <algorithm>

namespace fairuz::runtime {

namespace {

consteval auto make_builtin_registry()
{
    auto entries = std::array {
        BuiltinDefinition { "طول", &VM::len, 1 },
        BuiltinDefinition { "__نفس_الكائن__", &VM::same_object, 2 },
        BuiltinDefinition { "اضف", &VM::append, -1 },
        BuiltinDefinition { "احذف", &VM::pop, 1 },
        BuiltinDefinition { "مقطع", &VM::slice, -1 },
        BuiltinDefinition { "قائمة", &VM::list, -1 },
        BuiltinDefinition { "قاموس", &VM::dict, -1 },
        BuiltinDefinition { "__قاموس_مفاتيح__", &VM::dict_keys, 1 },
        BuiltinDefinition { "__قاموس_يحتوي__", &VM::dict_contains, 2 },
        BuiltinDefinition { "__قاموس_احذف__", &VM::dict_delete, 2 },
        BuiltinDefinition { "اكتب", &VM::print, -1 },
        BuiltinDefinition { "ادخل", &VM::input, 0 },
        BuiltinDefinition { "افتح", &VM::open, 2 },
        BuiltinDefinition { "اضف_ملف", &VM::append_file, 2 },
        BuiltinDefinition { "اغلق", &VM::close, 1 },
        BuiltinDefinition { "صنف", &VM::type, 1 },
        BuiltinDefinition { "طبيعي", &VM::Int, 1 },
        BuiltinDefinition { "حقيقي", &VM::Float, 1 },
        BuiltinDefinition { "سلسلة", &VM::str, -1 },
        BuiltinDefinition { "منطقي", &VM::Bool, 1 },
        BuiltinDefinition { "اقسم", &VM::split, 2 },
        BuiltinDefinition { "اجمع", &VM::join, 2 },
        BuiltinDefinition { "جزء", &VM::substr, 3 },
        BuiltinDefinition { "يحتوي", &VM::contains, 2 },
        BuiltinDefinition { "قص", &VM::trim, 1 },
        BuiltinDefinition { "__نص_من_رمز__", &VM::char_from_codepoint, 1 },
        BuiltinDefinition { "__عدد_من_نص__", &VM::number_from_text, 1 },
        BuiltinDefinition { "__عدد_منته__", &VM::number_finite, 1 },
        BuiltinDefinition { "__عدد_ليس_رقما__", &VM::number_is_nan, 1 },
        BuiltinDefinition { "__JSON_اهرب__", &VM::json_escape, 1 },
        BuiltinDefinition { "__JSON_اقرا_سلسلة__", &VM::json_read_string, 2 },
        BuiltinDefinition { "__استدعاء__", &VM::dynamic_call, 2 },
        BuiltinDefinition { "__منفذ_جديد__", &VM::executor_new, 1 },
        BuiltinDefinition { "__منفذ_اغلق__", &VM::executor_close, 2 },
        BuiltinDefinition { "__مهمة_ابدأ__", &VM::task_start, 3 },
        BuiltinDefinition { "__مهمة_تمت__", &VM::task_done, 1 },
        BuiltinDefinition { "__مهمة_نتيجة__", &VM::task_result, 2 },
        BuiltinDefinition { "__مهمة_الغ__", &VM::task_cancel, 1 },
        BuiltinDefinition { "__مهمة_انتظر_الكل__", &VM::task_wait_all, 2 },
        BuiltinDefinition { "__ملف_افتح__", &VM::file_open, 2 },
        BuiltinDefinition { "__ملف_اقرا__", &VM::file_read, 2 },
        BuiltinDefinition { "__ملف_اقرا_الكل__", &VM::file_read_all, 1 },
        BuiltinDefinition { "__ملف_اقرا_سطر__", &VM::file_read_line, 1 },
        BuiltinDefinition { "__ملف_اكتب__", &VM::file_write, 2 },
        BuiltinDefinition { "__ملف_اضف__", &VM::file_write, 2 },
        BuiltinDefinition { "__ملف_ادفع__", &VM::file_flush, 1 },
        BuiltinDefinition { "__ملف_اغلق__", &VM::close, 1 },
        BuiltinDefinition { "__مسار_احذف__", &VM::path_delete, 1 },
        BuiltinDefinition { "__مسار_glob__", &VM::path_glob, 2 },
        BuiltinDefinition { "__ملف_مؤقت__", &VM::temp_file, 3 },
        BuiltinDefinition { "__مجلد_مؤقت__", &VM::temp_directory, 2 },
        BuiltinDefinition { "__نظام_احذف_شجرة__", &VM::remove_tree, 1 },
        BuiltinDefinition { "__وقت_الان__", &VM::datetime_now, 0 },
        BuiltinDefinition { "__وقت_من_حقول__", &VM::datetime_from_fields, 7 },
        BuiltinDefinition { "__وقت_الى_حقول__", &VM::datetime_to_fields, 2 },
        BuiltinDefinition { "__وقت_حلل__", &VM::datetime_parse, 3 },
        BuiltinDefinition { "__وقت_نسق__", &VM::datetime_format, 3 },
        BuiltinDefinition { "__64_رمز__", &VM::base64_encode, 2 },
        BuiltinDefinition { "__64_فك__", &VM::base64_decode, 2 },
        BuiltinDefinition { "__16_رمز__", &VM::hex_encode, 1 },
        BuiltinDefinition { "__16_فك__", &VM::hex_decode, 1 },
        BuiltinDefinition { "__هاش_جديد__", &VM::hash_new, 1 },
        BuiltinDefinition { "__هاش_حدث__", &VM::hash_update, 2 },
        BuiltinDefinition { "__هاش_ناتج__", &VM::hash_digest, 2 },
        BuiltinDefinition { "__HMAC__", &VM::hmac, 3 },
        BuiltinDefinition { "__ضغط__", &VM::compress, 3 },
        BuiltinDefinition { "__فك_ضغط__", &VM::decompress, 3 },
        BuiltinDefinition { "ادنى", &VM::floor, 1 },
        BuiltinDefinition { "اعلى", &VM::ceil, 1 },
        BuiltinDefinition { "تقريب", &VM::round, 1 },
        BuiltinDefinition { "مطلق", &VM::abs, 1 },
        BuiltinDefinition { "اصغر", &VM::min, -1 },
        BuiltinDefinition { "اكبر", &VM::max, -1 },
        BuiltinDefinition { "قوة", &VM::pow, 2 },
        BuiltinDefinition { "جذر", &VM::sqrt, 1 },
        BuiltinDefinition { "__رياضيات__", &VM::math_unary, 2 },
        BuiltinDefinition { "__رياضيات2__", &VM::math_binary, 3 },
        BuiltinDefinition { "__URL_اهرب__", &VM::url_encode, 1 },
        BuiltinDefinition { "__URL_فك__", &VM::url_decode, 1 },
        BuiltinDefinition { "__URL_حلل__", &VM::url_parse, 1 },
        BuiltinDefinition { "__URL_ركب__", &VM::url_build, 1 },
        BuiltinDefinition { "__نمط_اجمع__", &VM::regex_compile, 2 },
        BuiltinDefinition { "__نمط_بحث__", &VM::regex_search, 3 },
        BuiltinDefinition { "__نمط_طابق__", &VM::regex_match, 3 },
        BuiltinDefinition { "__نمط_كامل__", &VM::regex_fullmatch, 2 },
        BuiltinDefinition { "__نمط_الكل__", &VM::regex_findall, 2 },
        BuiltinDefinition { "__نمط_اقسم__", &VM::regex_split, 3 },
        BuiltinDefinition { "__نمط_استبدل__", &VM::regex_replace, 4 },
        BuiltinDefinition { "تاكد", &VM::Assert, -1 },
        BuiltinDefinition { "ساعة", &VM::clock, 0 },
        BuiltinDefinition { "عطل", &VM::error, -1 },
        BuiltinDefinition { "وقت", &VM::time, 0 },
    };
    std::sort(entries.begin(), entries.end(), [](auto const& lhs, auto const& rhs) {
        return lhs.name < rhs.name;
    });
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name.empty() || entries[i].fn == nullptr || entries[i].arity < -1)
            throw "invalid builtin definition";
        if (i > 0 && entries[i - 1].name == entries[i].name)
            throw "duplicate builtin name";
    }
    return entries;
}

constexpr auto builtin_registry = make_builtin_registry();

} // anonymous namespace

BuiltinsList::BuiltinsList(VM& vm)
    : m_vm(vm)
    , m_values(builtin_registry.size(), Value::nil())
{
}

std::span<BuiltinDefinition const> BuiltinsList::definitions()
{
    return builtin_registry;
}

Value const* BuiltinsList::find(StringRef const& name) const
{
    std::string_view key(name.data(), name.len());
    auto found = std::lower_bound(builtin_registry.begin(), builtin_registry.end(), key,
        [](BuiltinDefinition const& entry, std::string_view value) { return entry.name < value; });
    if (found == builtin_registry.end() || found->name != key)
        return nullptr;

    Value& value = m_values[static_cast<size_t>(found - builtin_registry.begin())];
    if (value.is_nil()) {
        ObjString* native_name = m_vm.m_gc.make_obj_string(StringRef(found->name.data()));
        value = m_vm.m_gc.make_native(found->fn, native_name, found->arity);
    }
    return &value;
}

} // namespace fairuz::runtime
