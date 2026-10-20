//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements native value predicates and conversions.
//===----------------------------------------------------------------------===//

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include "util.hpp"

#include <iostream>
#include <string>

namespace fairuz::runtime {

VM::RuntimeResult VM::print(int argc, Value* argv)
{
    if (argc == 0 || argv == nullptr) {
        std::cout << '\n';
        return Value::from_bool(true);
    }

    for (int i = 0; i < argc; i++) {
        if (i > 0) std::cout << '\t';
        print_runtime_value(argv[i]);
    }
    std::cout << '\n';
    return Value::from_bool(true);
}

VM::RuntimeResult VM::type(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT, "got " + std::to_string(argc));

    Value& v = argv[0];
    TypeTag type = value_type_tag(v);
    switch (type) {
    case TypeTag::NONE: return m_gc.make_string("لاشيء");
    case TypeTag::NIL: return m_gc.make_string("عدم");
    case TypeTag::BOOL: return m_gc.make_string("منطقي");
    case TypeTag::INT: return m_gc.make_string("طبيعي");
    case TypeTag::DOUBLE: return m_gc.make_string("حقيقي");
    case TypeTag::STRING: return m_gc.make_string("سلسلة");
    case TypeTag::LIST: return m_gc.make_string("قائمة");
    case TypeTag::FUNCTION: return m_gc.make_string("دالة");
    case TypeTag::NATIVE: return m_gc.make_string("دالة");
    case TypeTag::CLASS: return m_gc.make_string(v.as_class()->name);
    case TypeTag::INSTANCE: return m_gc.make_string(v.as_instance()->klass->name);
    case TypeTag::DICT: return m_gc.make_string("قاموس");
    case TypeTag::FILE_HANDLE: return m_gc.make_string("ملف");
    case TypeTag::MODULE: return m_gc.make_string("وحدة");
    }

    return raise_runtime_error(ErrorCode::RUNTIME_ERROR); // unreachable
}

VM::RuntimeResult VM::same_object(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr) return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT);

    Value lhs = argv[0];
    Value rhs = argv[1];

    // Deep-copy memoization needs object identity, including for cyclic containers.
    if (lhs.is_obj() || rhs.is_obj()) return Value::from_bool(lhs.is_obj() && rhs.is_obj() && lhs.as_obj() == rhs.as_obj());
    return Value::from_bool(ValueEqual { }(lhs, rhs));
}

/// Type conversions

VM::RuntimeResult VM::Int(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT, "got " + std::to_string(argc));
    if (argv[0].is_int()) return argv[0];
    if (!argv[0].is_double()) return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    return integer::from_double(argv[0].as_double(), m_gc);
}

VM::RuntimeResult VM::Float(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT, "got " + std::to_string(argc));
    if (argv[0].is_number()) return Value::from_real(argv[0].as_double_any());
    return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
}

VM::RuntimeResult VM::str(int argc, Value* argv)
{
    if (argc > 1) return raise_runtime_error(ErrorCode::STR_ARG_COUNT, "got " + std::to_string(argc));

    StringRef output = "";

    if (argc == 0 || argv == nullptr) return m_gc.make_string(output); // return empty on no arg
    if (argv[0].is_string()) return m_gc.make_string(argv[0].as_string()->str);

    StringRef rendered = value_to_string(argv[0]);
    return m_gc.make_string(rendered);
}

VM::RuntimeResult VM::Bool(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) return raise_runtime_error(ErrorCode::BOOL_ARG_COUNT, "got " + std::to_string(argc));

    return argv[0].is_truthy() ? Value::from_bool(true) : Value::from_bool(false);
}

VM::RuntimeResult VM::Assert(int argc, Value* argv)
{
    if (argc < 1 || argc > 2 || argv == nullptr) return raise_runtime_error(ErrorCode::ASSERT_ARG_COUNT, "got" + std::to_string(argc));

    // Python-style assert accepts an optional diagnostic value; that second
    // argument describes a failed assertion and is not itself a condition.
    if (!argv[0].is_truthy()) {
        std::string detail;
        if (argc == 2) {
            StringRef rendered = value_to_string(argv[1]);
            detail.assign(rendered.data(), rendered.len());
        }
        return raise_runtime_error(ErrorCode::ASSERT_FAILED, detail);
    }

    return Value::nil(); // success
}

} // namespace fairuz::runtime
