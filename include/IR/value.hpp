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
        SizeType index;
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

    // Reference counting for memory management
    mutable std::int32_t RefCount_ = 0;

public:
    Value()
        : Type_(Type::NONE)
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

    // Copy constructor with COW (Copy-On-Write)
    Value(Value const& other)
        : Type_(other.Type_)
        , Data_(other.Data_)
    {
    }

    SizeType size()
    {
        if (isInt())
            return sizeof(asInt());
        if (isFloat())
            return sizeof(asFloat());
        if (isString())
            return asString().len();
        if (isBool())
            return sizeof(bool);
        if (isList())
            return asList().size();
        if (isDict())
            return asDict().size();
        if (isFunction())
            return sizeof(Function);
        return sizeof(ValueData);
    }

    Type getType() const
    {
        return Type_;
    }

    void setType(Type const type)
    {
        Type_ = type;
    }

    void setData(std::variant<std::monostate,                                // None
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
        > const data)
    {
        Data_ = data;
    }

    // Type checks

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

    // Getters with safety
    std::int64_t asInt() const;

    double asFloat() const;

    StringRef const& asString() const;

    bool asBool() const;

    std::vector<Value>& asList();
    std::vector<Value> const& asList() const;

    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>& asDict();
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> const& asDict() const;

    Function& asFunction();
    Function const& asFunction() const;

    NativeFunction& asNativeFunction();
    NativeFunction const& asNativeFunction() const;

    double toFloat() const; // Type conversions

    std::int64_t toInt() const;

    bool toBool() const;

    StringRef toString() const;

    std::string repr() const;

    SizeType hash() const; // Hash for use in dictionaries

    bool hasNext() const;

    // Comparison operators
    bool operator!=(Value const& other) const;
    bool operator==(Value const& other) const;

    bool operator<(Value const& other) const;
    bool operator>(Value const& other) const;

    bool operator<=(Value const& other) const;
    bool operator>=(Value const& other) const;

    // Arithmetic operators with type promotion
    Value operator+(Value const& other) const;
    Value operator-(Value const& other) const;

    Value operator*(Value const& other) const;
    Value operator/(Value const& other) const;
    Value operator%(Value const& other) const;

    Value operator-() const; // Unary operators
    Value operator!() const;

    Value pow(Value const& other) const;
    Value getItem(Value const& key) const; // Subscript operator

    void setItem(Value const& key, Value const& value);
    Value getIterator() const;
    Value next();

    static Value makeList(std::vector<Value> list);
};

class Environment {
private:
    std::unordered_map<StringRef, Value> variables_;
    Environment* parent_; // For nested scopes

public:
    Environment()
        : parent_(nullptr)
    {
    }

    explicit Environment(Environment* parent)
        : parent_(parent)
    {
    }

    // Define a variable in current scope
    void define(StringRef const& name, Value const& value) { variables_[name] = value; }

    // Get a variable (searches parent scopes)
    Value get(StringRef const& name) const
    {
        auto it = variables_.find(name);
        if (it != variables_.end())
            return it->second;

        if (parent_)
            return parent_->get(name);

        throw std::runtime_error("Undefined variable: " + std::string(name.data()));
    }

    // Assign to existing variable (searches parent scopes)
    void assign(StringRef const& name, Value const& value)
    {
        auto it = variables_.find(name);
        if (it != variables_.end()) {
            it->second = value;
            return;
        }

        if (parent_) {
            parent_->assign(name, value);
            return;
        }

        throw std::runtime_error("Undefined variable: " + std::string(name.data()));
    }

    bool exists(StringRef const& name) const
    {
        if (variables_.find(name) != variables_.end())
            return true;
        return parent_ && parent_->exists(name);
    }
};

}
}
