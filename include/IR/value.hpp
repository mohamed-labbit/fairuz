#pragma once

#include "../ast/ast.hpp"
#include "../macros.hpp"
#include "../types/string.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mylang {
namespace IR {

class Environment;

class Value {
public:
    enum class Type {
        NONE,
        INT,
        FLOAT,
        STRING,
        BOOL,
        LIST,
        DICT,
        TUPLE,
        SET,
        FUNCTION,
        NATIVE_FUNCTION,
        OBJECT,
        SLICE,
        ITERATOR,
        GENERATOR,
        COROUTINE
    };

    struct Function {
        std::int32_t CodeOffset;
        std::vector<StringRef> params;
        std::vector<Value> defaults;
        Environment* closure; // Captured variables
        ast::ASTNode* body;
    };

    struct NativeFunction {
        std::function<Value(std::vector<Value> const&)> func;
        StringRef name;
        std::int32_t arity;
    };

    struct Object {
        StringRef className;
        std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> attributes;
        std::shared_ptr<Value> parent; // For inheritance
    };

    struct Iterator {
        std::vector<Value> items;
        std::size_t index;
    };

    using ValueData = std::variant<std::monostate,                           // None
        std::int64_t,                                                        // Int
        double,                                                              // Float
        StringRef,                                                           // String (shared for efficiency)
        bool,                                                                // Bool
        std::vector<Value>,                                                  // List (shared)
        std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>, // Dict
        Function,                                                            // User function
        NativeFunction,                                                      // Native C++ function
        Object,                                                              // Object instance
        Iterator                                                             // Iterator
        >;

private:
    Type Type_;
    ValueData Data_;

    // reference counter
    /// NOTE: not used yet
    mutable std::int32_t RefCount_ = 0;

public:
    Value()
        : Type_(Type::NONE)
    {
    }

    Value(int v)
        : Value(static_cast<std::int64_t>(v))
    {
    }

    Value(std::int64_t const v)
        : Type_(Type::INT)
        , Data_(std::in_place_type<std::int64_t>, v)
    {
    }

    Value(double const v)
        : Type_(Type::FLOAT)
        , Data_(std::in_place_type<double>, v)
    {
    }

    Value(StringRef const& v)
        : Type_(Type::STRING)
        , Data_(std::in_place_type<StringRef>, v)
    {
    }

    Value(bool const v)
        : Type_(Type::BOOL)
        , Data_(std::in_place_type<bool>, v)
    {
    }

    Value(std::vector<Value> const v)
        : Type_(Type::LIST)
        , Data_(std::in_place_type<std::vector<Value>>, v)
    {
    }

    Value(std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> const v)
        : Type_(Type::DICT)
        , Data_(std::in_place_type<std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>>, v)
    {
    }

    Value(Function const v)
        : Type_(Type::FUNCTION)
        , Data_(std::in_place_type<Function>, v)
    {
    }

    Value(Value const& other)
        : Type_(other.Type_)
        , Data_(other.Data_)
    {
    }

    // raw held data size (handle not included)
    std::size_t size();

    Type getType() const
    {
        return Type_;
    }

    void setType(Type const type)
    {
        Type_ = type;
    }

    void setData(ValueData const data)
    {
        Data_ = data;
    }

    void setData(std::int64_t v)
    {
        Data_ = ValueData(std::in_place_type<std::int64_t>, v);
        Type_ = Type::INT;
    }

    // type checks
    bool isNone() const
    {
        return Type_ == Type::NONE;
    }

    bool isInt() const
    {
        return Type_ == Type::INT;
    }

    bool isFloat() const
    {
        return Type_ == Type::FLOAT;
    }

    bool isNumber() const
    {
        return isInt() || isFloat();
    }

    bool isString() const
    {
        return Type_ == Type::STRING;
    }

    bool isBool() const
    {
        return Type_ == Type::BOOL;
    }

    bool isList() const
    {
        return Type_ == Type::LIST;
    }

    bool isDict() const
    {
        return Type_ == Type::DICT;
    }

    bool isFunction() const
    {
        return Type_ == Type::FUNCTION;
    }

    bool isCallable() const
    {
        return isFunction() || Type_ == Type::NATIVE_FUNCTION;
    }

    bool isIterable() const
    {
        return isList() || isString() || isDict();
    }

    // getters
    std::int64_t* asInt();
    std::int64_t const* asInt() const;

    double* asFloat();
    double const* asFloat() const;

    StringRef* asString();
    StringRef const* asString() const;

    bool* asBool();
    bool const* asBool() const;

    std::vector<Value>* asList();
    std::vector<Value> const* asList() const;

    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>* asDict();
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> const* asDict() const;

    Function* asFunction();
    Function const* asFunction() const;

    NativeFunction* asNativeFunction();
    NativeFunction const* asNativeFunction() const;

    // type convs
    std::int64_t toInt() const;

    double toFloat() const;

    bool toBool() const;

    StringRef toString() const;

    std::string repr() const; // for debugging (for now)

    std::size_t hash() const;

    bool hasNext() const;

    // operators (arithmetic ops have type promotion)
    bool operator!=(Value const& other) const;
    bool operator==(Value const& other) const;

    bool operator<(Value const& other) const;
    bool operator>(Value const& other) const;

    bool operator<=(Value const& other) const;
    bool operator>=(Value const& other) const;

    Value operator+(Value const& other) const;
    Value operator-(Value const& other) const;
    Value operator*(Value const& other) const;
    Value operator/(Value const& other) const;
    Value operator%(Value const& other) const;

    Value operator-() const;
    Value operator!() const;

    Value pow(Value const& other) const;

    Value getItem(Value const& key) const;
    void setItem(Value const& key, Value const& value);

    Value getIterator() const;
    Value next();

    static constexpr Value makeNone()
    {
        return Value();
    }

    static constexpr Value makeInt(int const v)
    {
        return Value(v);
    }

    static constexpr Value makeFloat(double const v)
    {
        return Value(v);
    }

    static constexpr Value makeString(StringRef const v)
    {
        return Value(v);
    }

    static constexpr Value makeBool(bool const v)
    {
        return Value(v);
    }

    static constexpr Value makeList(std::vector<Value> const v)
    {
        return Value(v);
    }

    static constexpr Value makeDict(std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> const v)
    {
        return Value(v);
    }

    static constexpr Value makeFunction(Function const v)
    {
        return Value(v);
    }
};

}
}
