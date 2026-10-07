#include "../futf8.hpp"
#include "../fvm.hpp"

namespace fairuz::runtime {

/// --------------------------------------------------------------
///                      Container builtins
/// --------------------------------------------------------------

Value VM::list(int argc, Value* argv)
{
    Value ret = m_gc.make_list();
    ObjList* list_obj = ret.as_list();

    for (int i = 0; i < argc; i++)
        list_obj->elements.push(argv[i]);

    return ret;
}

Value VM::dict(int argc, Value* argv)
{
    Value ret = m_gc.make_dict();
    if (argc <= 0 || argv == nullptr)
        return ret;

    ObjDict* dict_obj = ret.as_dict();
    for (int i = 0; i + 1 < argc; i += 2)
        dict_obj->set(argv[i], argv[i + 1]);

    return ret;
}

Value VM::dict_contains(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dictionary contains expects a dictionary and key");
    return Value::from_bool(argv[0].as_dict()->data.contains(argv[1]));
}

Value VM::dict_delete(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dictionary delete expects a dictionary and key");
    Value removed = Value::nil();
    argv[0].as_dict()->erase(argv[1], &removed);
    return removed;
}

Value VM::dict_keys(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dictionary keys expects one dictionary");

    Value result = m_gc.make_list();
    ObjDict* dict = argv[0].as_dict();
    for (Value key : dict->insertion_order)
        result.as_list()->elements.push(key);
    return result;
}

Value VM::append(int argc, Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        raise_error(ErrorCode::APPEND_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value& list_v = argv[0];
    if (!list_v.is_list()) {
        raise_error(ErrorCode::APPEND_TYPE_ERROR);
        return Value::nil();
    }

    ObjList* list_obj = list_v.as_list();

    for (int i = 1; i < argc; i++)
        list_obj->elements.push(argv[i]);

    return Value::nil();
}

Value VM::pop(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::POP_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value& list_v = argv[0];
    if (!list_v.is_list()) {
        raise_error(ErrorCode::POP_TYPE_ERROR);
        return Value::nil();
    }

    ObjList* list_obj = list_v.as_list();

    if (list_obj->empty())
        raise_error(ErrorCode::POP_EMPTY_LIST);

    list_obj->elements.pop();
    return list_v;
}

Value VM::len(int argc, Value* argv)
{
    if (argc == 0 || argv == nullptr)
        return Value::nil();

    if (argc == 1) {
        if (argv[0].is_string()) {
            StringRef const& str = argv[0].as_string()->str;
            // Strings also serve as the language's byte container at native
            // boundaries (files, codecs, compression).  Preserve Unicode
            // code-point length for text, but never try to decode arbitrary
            // binary payloads such as gzip streams.
            if (!simdutf::validate_utf8(str.data(), str.len()))
                return Value::from_int(static_cast<i64>(str.len()), m_gc);
            size_t byte_pos = 0;
            i64 char_count = 0;

            while (byte_pos < str.len()) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_count++;
            }

            return Value::from_int(char_count, m_gc);
        }

        if (argv[0].is_list())
            return Value::from_int(argv[0].as_list()->elements.size(), m_gc);
        if (argv[0].is_dict())
            return Value::from_int(argv[0].as_dict()->data.size(), m_gc);
    }

    /// do not accept multiple args for len
    return Value::nil();
}

void VM::dict_put(Value* dict_ptr, Value k, Value v)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return;

    ObjDict* as_dict = dict_ptr->as_dict();
    as_dict->set(k, v);
}

Value VM::dict_get(Value* dict_ptr, Value k)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return Value::nil();

    ObjDict* as_dict = dict_ptr->as_dict();
    return as_dict->data[k];
}

Value VM::slice(int argc, Value* argv)
{
    /// cut a copy of a container, with inclusive indices
    /// accept [container, start, end]
    /// a, b are the indices
    /// if b is null then cut [start:]

    if (argc < 2 || argc > 3 || argv == nullptr) {
        raise_error(ErrorCode::SLICE_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value container = argv[0];
    Value start_value = argv[1];
    Value end_value = argc < 3 ? Value::nil() : argv[2];

    if (UNLIKELY(!container.is_string() && !container.is_list()))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "slice expects a string or list as its first argument");
    if (UNLIKELY(!start_value.is_int() || !(end_value.is_nil() || end_value.is_int())))
        raise_error(ErrorCode::INDEX_TYPE_ERROR);

    i64 start_i = start_value.as_int();
    size_t container_size = container.is_string()
        ? static_cast<size_t>(len(1, &container).as_int())
        : static_cast<size_t>(container.as_list()->size());

    if (container_size == 0) {
        if (start_i == 0 && end_value.is_nil())
            return container.is_string() ? m_gc.make_string("") : m_gc.make_list();
        raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);
    }

    i64 end_i = end_value.is_nil()
        ? static_cast<i64>(container_size - 1)
        : end_value.as_int();

    if (UNLIKELY(start_i < 0 || end_i < 0 || start_i > end_i
            || static_cast<u64>(start_i) >= container_size
            || static_cast<u64>(end_i) >= container_size))
        raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

    size_t start = static_cast<size_t>(start_i);
    size_t end = static_cast<size_t>(end_i);

    if (container.is_string()) {
        ObjString* str_obj = container.as_string();
        if (!simdutf::validate_utf8(str_obj->str.data(), str_obj->str.len()))
            return m_gc.make_string(str_obj->str.slice(start, end + 1));
        Value args[] = { container, Value::from_int(start_i, m_gc), Value::from_int(end_i + 1, m_gc) };
        return substr(3, args);
    } else if (container.is_list()) {
        ObjList* list_obj = container.as_list();
        ObjList* ret_list = m_gc.make_obj_list();
        for (size_t i = start; i <= end; i++)
            ret_list->elements.push(list_obj->elements[static_cast<u32>(i)]);

        return Value::from_list(ret_list);
    }

    return Value::nil();
}

} // namespace fairuz::runtime
