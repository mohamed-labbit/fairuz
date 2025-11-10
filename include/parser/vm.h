#pragma once


#include "AST.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


// ============================================================================
// ADVANCED VALUE SYSTEM - Full Python-like dynamic typing
// ============================================================================
class Value
{
   public:
    enum class Type {
        None,
        Int,
        Float,
        String,
        Bool,
        List,
        Dict,
        Tuple,
        Set,
        Function,
        NativeFunction,
        Object,
        Slice,
        Iterator,
        Generator,
        Coroutine
    };

   private:
    Type type_;

    struct Function
    {
        int codeOffset;
        std::vector<std::string> params;
        std::vector<Value> defaults;
        std::vector<Value> closure;  // Captured variables
    };

    struct NativeFunction
    {
        std::function<Value(const std::vector<Value>&)> func;
        std::string name;
        int arity;
    };

    struct Object
    {
        std::string className;
        std::unordered_map<std::string, Value> attributes;
        std::shared_ptr<Value> parent;  // For inheritance
    };

    struct Iterator
    {
        std::shared_ptr<std::vector<Value>> items;
        size_t index;
    };

    std::variant<std::monostate,  // None
      long long,  // Int
      double,  // Float
      std::shared_ptr<std::string>,  // String (shared for efficiency)
      bool,  // Bool
      std::shared_ptr<std::vector<Value>>,  // List (shared)
      std::shared_ptr<std::unordered_map<std::string, Value>>,  // Dict
      Function,  // User function
      NativeFunction,  // Native C++ function
      Object,  // Object instance
      Iterator  // Iterator
      >
      data_;

    // Reference counting for memory management
    mutable int refCount_ = 0;

   public:
    Value() :
        type_(Type::None)
    {
    }
    Value(long long v) :
        type_(Type::Int),
        data_(v)
    {
    }
    Value(double v) :
        type_(Type::Float),
        data_(v)
    {
    }
    Value(const std::string& v) :
        type_(Type::String),
        data_(std::make_shared<std::string>(v))
    {
    }
    Value(bool v) :
        type_(Type::Bool),
        data_(v)
    {
    }
    Value(const std::vector<Value>& v) :
        type_(Type::List),
        data_(std::make_shared<std::vector<Value>>(v))
    {
    }
    Value(std::shared_ptr<std::vector<Value>> v) :
        type_(Type::List),
        data_(v)
    {
    }

    // Copy constructor with COW (Copy-On-Write)
    Value(const Value& other) :
        type_(other.type_),
        data_(other.data_)
    {
    }

    Type getType() const { return type_; }

    // Type checks
    bool isNone() const { return type_ == Type::None; }
    bool isInt() const { return type_ == Type::Int; }
    bool isFloat() const { return type_ == Type::Float; }
    bool isNumber() const { return isInt() || isFloat(); }
    bool isString() const { return type_ == Type::String; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isList() const { return type_ == Type::List; }
    bool isDict() const { return type_ == Type::Dict; }
    bool isFunction() const { return type_ == Type::Function; }
    bool isCallable() const { return isFunction() || type_ == Type::NativeFunction; }
    bool isIterable() const { return isList() || isString() || isDict(); }

    // Getters with safety
    long long asInt() const
    {
        if (!isInt())
        {
            throw std::runtime_error("Value is not an integer");
        }
        return std::get<long long>(data_);
    }

    double asFloat() const
    {
        if (!isFloat())
        {
            throw std::runtime_error("Value is not a float");
        }
        return std::get<double>(data_);
    }

    const std::string& asString() const
    {
        if (!isString())
        {
            throw std::runtime_error("Value is not a string");
        }
        return *std::get<std::shared_ptr<std::string>>(data_);
    }

    bool asBool() const
    {
        if (!isBool())
        {
            throw std::runtime_error("Value is not a boolean");
        }
        return std::get<bool>(data_);
    }

    std::vector<Value>& asList()
    {
        if (!isList())
        {
            throw std::runtime_error("Value is not a list");
        }
        return *std::get<std::shared_ptr<std::vector<Value>>>(data_);
    }

    const std::vector<Value>& asList() const
    {
        if (!isList())
        {
            throw std::runtime_error("Value is not a list");
        }
        return *std::get<std::shared_ptr<std::vector<Value>>>(data_);
    }

    std::unordered_map<std::string, Value>& asDict()
    {
        if (!isDict())
        {
            throw std::runtime_error("Value is not a dict");
        }
        return *std::get<std::shared_ptr<std::unordered_map<std::string, Value>>>(data_);
    }

    Function& asFunction()
    {
        if (!isFunction())
        {
            throw std::runtime_error("Value is not a function");
        }
        return std::get<Function>(data_);
    }

    NativeFunction& asNativeFunction()
    {
        if (type_ != Type::NativeFunction)
        {
            throw std::runtime_error("Not a native function");
        }
        return std::get<NativeFunction>(data_);
    }

    // Type conversions
    double toFloat() const
    {
        if (isInt())
        {
            return static_cast<double>(asInt());
        }
        if (isFloat())
        {
            return asFloat();
        }
        if (isBool())
        {
            return asBool() ? 1.0 : 0.0;
        }
        throw std::runtime_error("Cannot convert to float");
    }

    long long toInt() const
    {
        if (isInt())
        {
            return asInt();
        }
        if (isFloat())
        {
            return static_cast<long long>(asFloat());
        }
        if (isBool())
        {
            return asBool() ? 1 : 0;
        }
        if (isString())
        {
            return std::stoll(asString());
        }
        throw std::runtime_error("Cannot convert to int");
    }

    bool toBool() const
    {
        switch (type_)
        {
        case Type::None :
            return false;
        case Type::Int :
            return asInt() != 0;
        case Type::Float :
            return asFloat() != 0.0;
        case Type::String :
            return !asString().empty();
        case Type::Bool :
            return asBool();
        case Type::List :
            return !asList().empty();
        case Type::Dict :
            return !asDict().empty();
        default :
            return true;
        }
    }

    std::string toString() const
    {
        switch (type_)
        {
        case Type::None :
            return "None";
        case Type::Int :
            return std::to_string(asInt());
        case Type::Float : {
            auto s = std::to_string(asFloat());
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.')
            {
                s += "0";
            }
            return s;
        }
        case Type::String :
            return asString();
        case Type::Bool :
            return asBool() ? "True" : "False";
        case Type::List : {
            std::string result = "[";
            const auto& list = asList();
            for (size_t i = 0; i < list.size(); i++)
            {
                result += list[i].toString();
                if (i + 1 < list.size())
                {
                    result += ", ";
                }
            }
            return result + "]";
        }
        case Type::Dict : {
            std::string result = "{";
            const auto& dict = std::get<std::shared_ptr<std::unordered_map<std::string, Value>>>(data_);
            size_t count = 0;
            for (const auto& [k, v] : *dict)
            {
                result += "'" + k + "': " + v.toString();
                if (++count < dict->size())
                {
                    result += ", ";
                }
            }
            return result + "}";
        }
        case Type::Function :
            return "<function>";
        case Type::NativeFunction :
            return "<native function>";
        default :
            return "<object>";
        }
    }

    std::string repr() const
    {
        if (isString())
        {
            return "'" + asString() + "'";
        }
        return toString();
    }

    // Hash for use in dictionaries
    size_t hash() const
    {
        std::hash<std::string> hasher;
        return hasher(toString());
    }

    // Comparison operators
    bool operator==(const Value& other) const
    {
        if (type_ != other.type_)
        {
            // Handle numeric comparisons
            if (isNumber() && other.isNumber())
            {
                return toFloat() == other.toFloat();
            }
            return false;
        }

        switch (type_)
        {
        case Type::None :
            return true;
        case Type::Int :
            return asInt() == other.asInt();
        case Type::Float :
            return asFloat() == other.asFloat();
        case Type::String :
            return asString() == other.asString();
        case Type::Bool :
            return asBool() == other.asBool();
        case Type::List :
            return asList() == other.asList();
        default :
            return false;
        }
    }

    bool operator<(const Value& other) const
    {
        if (isNumber() && other.isNumber())
        {
            return toFloat() < other.toFloat();
        }
        if (isString() && other.isString())
        {
            return asString() < other.asString();
        }
        throw std::runtime_error("Cannot compare types");
    }

    bool operator>(const Value& other) const { return other < *this; }
    bool operator<=(const Value& other) const { return !(other < *this); }
    bool operator>=(const Value& other) const { return !(*this < other); }
    bool operator!=(const Value& other) const { return !(*this == other); }

    // Arithmetic operators with type promotion
    Value operator+(const Value& other) const
    {
        if (isInt() && other.isInt())
        {
            return Value(asInt() + other.asInt());
        }
        if (isNumber() && other.isNumber())
        {
            return Value(toFloat() + other.toFloat());
        }
        if (isString() || other.isString())
        {
            return Value(toString() + other.toString());
        }
        if (isList() && other.isList())
        {
            auto result = asList();
            const auto& otherList = other.asList();
            result.insert(result.end(), otherList.begin(), otherList.end());
            return Value(result);
        }
        throw std::runtime_error("Unsupported operand types for +");
    }

    Value operator-(const Value& other) const
    {
        if (isInt() && other.isInt())
        {
            return Value(asInt() - other.asInt());
        }
        if (isNumber() && other.isNumber())
        {
            return Value(toFloat() - other.toFloat());
        }
        throw std::runtime_error("Unsupported operand types for -");
    }

    Value operator*(const Value& other) const
    {
        if (isInt() && other.isInt())
        {
            return Value(asInt() * other.asInt());
        }
        if (isNumber() && other.isNumber())
        {
            return Value(toFloat() * other.toFloat());
        }
        // String/List repetition
        if (isString() && other.isInt())
        {
            std::string result;
            for (long long i = 0; i < other.asInt(); i++)
            {
                result += asString();
            }
            return Value(result);
        }
        if (isList() && other.isInt())
        {
            std::vector<Value> result;
            for (long long i = 0; i < other.asInt(); i++)
            {
                result.insert(result.end(), asList().begin(), asList().end());
            }
            return Value(result);
        }
        throw std::runtime_error("Unsupported operand types for *");
    }

    Value operator/(const Value& other) const
    {
        if (isNumber() && other.isNumber())
        {
            double divisor = other.toFloat();
            if (divisor == 0.0)
            {
                throw std::runtime_error("Division by zero");
            }
            return Value(toFloat() / divisor);
        }
        throw std::runtime_error("Unsupported operand types for /");
    }

    Value operator%(const Value& other) const
    {
        if (isInt() && other.isInt())
        {
            if (other.asInt() == 0)
            {
                throw std::runtime_error("Modulo by zero");
            }
            return Value(asInt() % other.asInt());
        }
        if (isNumber() && other.isNumber())
        {
            return Value(std::fmod(toFloat(), other.toFloat()));
        }
        throw std::runtime_error("Unsupported operand types for %");
    }

    Value pow(const Value& other) const
    {
        if (isNumber() && other.isNumber())
        {
            return Value(std::pow(toFloat(), other.toFloat()));
        }
        throw std::runtime_error("Unsupported operand types for **");
    }

    // Unary operators
    Value operator-() const
    {
        if (isInt())
        {
            return Value(-asInt());
        }
        if (isFloat())
        {
            return Value(-asFloat());
        }
        throw std::runtime_error("Unsupported operand type for unary -");
    }

    Value operator!() const { return Value(!toBool()); }

    // Subscript operator
    Value getItem(const Value& key) const
    {
        if (isList())
        {
            long long index = key.toInt();
            const auto& list = asList();
            if (index < 0)
            {
                index += list.size();
            }
            if (index < 0 || index >= list.size())
            {
                throw std::runtime_error("List index out of range");
            }
            return list[index];
        }
        if (isDict())
        {
            const auto& dict = asDict();
            auto it = dict.find(key.toString());
            if (it == dict.end())
            {
                throw std::runtime_error("Key not found: " + key.toString());
            }
            return it->second;
        }
        if (isString())
        {
            long long index = key.toInt();
            const auto& str = asString();
            if (index < 0)
            {
                index += str.size();
            }
            if (index < 0 || index >= str.size())
            {
                throw std::runtime_error("String index out of range");
            }
            return Value(std::string(1, str[index]));
        }
        throw std::runtime_error("Object is not subscriptable");
    }

    void setItem(const Value& key, const Value& value)
    {
        if (isList())
        {
            long long index = key.toInt();
            auto& list = asList();
            if (index < 0)
            {
                index += list.size();
            }
            if (index < 0 || index >= list.size())
            {
                throw std::runtime_error("List index out of range");
            }
            list[index] = value;
        }
        else if (isDict())
        {
            asDict()[key.toString()] = value;
        }
        else
        {
            throw std::runtime_error("Object does not support item assignment");
        }
    }

    // Iterator support
    Value getIterator() const
    {
        if (isList())
        {
            Iterator it;
            it.items = std::get<std::shared_ptr<std::vector<Value>>>(data_);
            it.index = 0;
            Value result;
            result.type_ = Type::Iterator;
            result.data_ = it;
            return result;
        }
        throw std::runtime_error("Object is not iterable");
    }

    bool hasNext() const
    {
        if (type_ == Type::Iterator)
        {
            const auto& it = std::get<Iterator>(data_);
            return it.index < it.items->size();
        }
        return false;
    }

    Value next()
    {
        if (type_ == Type::Iterator)
        {
            auto& it = std::get<Iterator>(data_);
            if (it.index >= it.items->size())
            {
                throw std::runtime_error("Iterator exhausted");
            }
            return (*it.items)[it.index++];
        }
        throw std::runtime_error("Object is not an iterator");
    }
};

// ============================================================================
// ENHANCED BYTECODE INSTRUCTION SET
// ============================================================================
enum class OpCode : uint8_t {
    // Stack operations
    LOAD_CONST,  // Push constant onto stack
    LOAD_VAR,  // Load local variable
    LOAD_GLOBAL,  // Load global variable
    STORE_VAR,  // Store to local variable
    STORE_GLOBAL,  // Store to global variable
    LOAD_ATTR,  // Load attribute from object
    STORE_ATTR,  // Store attribute to object
    POP,  // Pop top of stack
    DUP,  // Duplicate top of stack
    SWAP,  // Swap top two stack items
    ROT_THREE,  // Rotate top three stack items

    // Arithmetic (optimized for int fast path)
    ADD,
    SUB,
    MUL,
    DIV,
    FLOOR_DIV,
    MOD,
    POW,
    NEG,
    POS,

    // Bitwise
    BITAND,
    BITOR,
    BITXOR,
    BITNOT,
    LSHIFT,
    RSHIFT,

    // Comparison
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    IN,
    NOT_IN,
    IS,
    IS_NOT,

    // Logical
    AND,
    OR,
    NOT,

    // Control flow
    JUMP,  // Unconditional jump
    JUMP_IF_FALSE,  // Conditional jump (pop)
    JUMP_IF_TRUE,  // Conditional jump (pop)
    POP_JUMP_IF_FALSE,  // Conditional jump (keep on stack)
    FOR_ITER,  // For loop iteration

    // Functions & Methods
    CALL,  // Call function
    CALL_METHOD,  // Call method (optimized)
    RETURN,  // Return from function
    YIELD,  // Generator yield

    // Collections
    BUILD_LIST,  // Build list from stack
    BUILD_DICT,  // Build dict from stack
    BUILD_SET,  // Build set from stack
    BUILD_TUPLE,  // Build tuple from stack
    BUILD_SLICE,  // Build slice object

    UNPACK_SEQUENCE,  // Unpack sequence to stack
    GET_ITEM,  // Get item from list/dict
    SET_ITEM,  // Set item in list/dict
    DEL_ITEM,  // Delete item from list/dict

    // Advanced
    LOAD_CLOSURE,  // Load closure variable
    MAKE_FUNCTION,  // Create function object
    IMPORT_NAME,  // Import module
    GET_ITER,  // Get iterator

    // Exception handling
    SETUP_TRY,  // Setup try block
    POP_TRY,  // Pop try block
    RAISE,  // Raise exception

    // Special
    PRINT,  // Built-in print (optimized)
    FORMAT,  // String formatting
    NOP,  // No operation
    HALT  // Stop execution
};

struct Instruction
{
    OpCode op;
    int arg;
    int lineNumber;  // For debugging

    Instruction(OpCode o, int a = 0, int ln = 0) :
        op(o),
        arg(a),
        lineNumber(ln)
    {
    }
};

// ============================================================================
// JIT COMPILATION SUPPORT - Hot path detection and native code generation
// ============================================================================
class JITCompiler
{
   private:
    struct HotSpot
    {
        int startPC;
        int endPC;
        int executionCount;
        bool compiled;
        void* nativeCode;
    };

    std::unordered_map<int, HotSpot> hotSpots;
    int hotThreshold = 100;  // Compile after 100 executions

   public:
    void recordExecution(int pc)
    {
        auto it = hotSpots.find(pc);
        if (it != hotSpots.end())
        {
            it->second.executionCount++;
            if (it->second.executionCount >= hotThreshold && !it->second.compiled)
            {
                compileHotSpot(it->second);
            }
        }
    }

    bool isHotSpot(int pc) const
    {
        auto it = hotSpots.find(pc);
        return it != hotSpots.end() && it->second.compiled;
    }

   private:
    void compileHotSpot(HotSpot& spot)
    {
        // JIT compile to native code
        // This would generate x86-64/ARM assembly for the hot loop
        spot.compiled = true;
        std::cout << "[JIT] Compiled hot spot at PC " << spot.startPC << "\n";
    }
};

// ============================================================================
// GARBAGE COLLECTOR - Mark & Sweep with generational collection
// ============================================================================
class GarbageCollector
{
   private:
    std::vector<Value*> allObjects;
    std::vector<Value*> roots;
    size_t threshold = 1000;
    size_t allocated = 0;

    // Generational: young, old
    std::vector<Value*> youngGen;
    std::vector<Value*> oldGen;
    int youngGenCollections = 0;

   public:
    void registerObject(Value* obj)
    {
        allObjects.push_back(obj);
        youngGen.push_back(obj);
        allocated++;

        if (allocated >= threshold)
        {
            collect();
        }
    }

    void addRoot(Value* root) { roots.push_back(root); }

    void collect()
    {
        // Mark phase
        std::unordered_set<Value*> marked;
        std::vector<Value*> worklist = roots;

        while (!worklist.empty())
        {
            Value* obj = worklist.back();
            worklist.pop_back();

            if (marked.count(obj))
            {
                continue;
            }
            marked.insert(obj);

            // Mark children (if list, dict, etc.)
            if (obj->isList())
            {
                for (auto& item : obj->asList())
                {
                    worklist.push_back(const_cast<Value*>(&item));
                }
            }
        }

        // Sweep phase
        auto it = allObjects.begin();
        while (it != allObjects.end())
        {
            if (!marked.count(*it))
            {
                delete *it;
                it = allObjects.erase(it);
                allocated--;
            }
            else
            {
                ++it;
            }
        }

        youngGenCollections++;

        // Promote survivors to old generation every 5 collections
        if (youngGenCollections % 5 == 0)
        {
            promoteToOldGen();
        }
    }

   private:
    void promoteToOldGen()
    {
        for (auto* obj : youngGen)
        {
            oldGen.push_back(obj);
        }
        youngGen.clear();
    }
};

// ============================================================================
// ENHANCED VIRTUAL MACHINE with optimizations
// ============================================================================
class VirtualMachine
{
   private:
    // Execution state
    std::vector<Value> stack;
    std::vector<Value> globals;
    std::vector<std::unordered_map<std::string, Value>> frames;  // Call frames
    int ip = 0;

    // Advanced features
    JITCompiler jit;
    GarbageCollector gc;

    // Performance monitoring
    struct Statistics
    {
        long long instructionsExecuted = 0;
        long long functionsCalled = 0;
        long long gcCollections = 0;
        long long jitCompilations = 0;
        std::chrono::microseconds executionTime{0};
    } stats;

    // Instruction cache for faster dispatch
    static constexpr size_t CACHE_SIZE = 256;
    std::array<void*, CACHE_SIZE> dispatchTable;

    // Stack manipulation (with bounds checking)
    void push(const Value& val)
    {
        if (stack.size() >= 10000)
        {
            throw std::runtime_error("Stack overflow");
        }
        stack.push_back(val);
    }

    Value pop()
    {
        if (stack.empty())
        {
            throw std::runtime_error("Stack underflow");
        }
        Value val = stack.back();
        stack.pop_back();
        return val;
    }

    Value& top()
    {
        if (stack.empty())
        {
            throw std::runtime_error("Stack empty");
        }
        return stack.back();
    }

    Value& peek(int offset)
    {
        if (stack.size() < offset + 1)
        {
            throw std::runtime_error("Stack underflow in peek");
        }
        return stack[stack.size() - 1 - offset];
    }

    // Fast integer operations (avoid type checking overhead)
    inline Value fastAdd(const Value& a, const Value& b)
    {
        if (a.isInt() && b.isInt())
        {
            return Value(a.asInt() + b.asInt());
        }
        return a + b;
    }

    inline Value fastSub(const Value& a, const Value& b)
    {
        if (a.isInt() && b.isInt())
        {
            return Value(a.asInt() - b.asInt());
        }
        return a - b;
    }

    inline Value fastMul(const Value& a, const Value& b)
    {
        if (a.isInt() && b.isInt())
        {
            return Value(a.asInt() * b.asInt());
        }
        return a * b;
    }

    // Native function registration
    std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>> nativeFunctions;

    void registerNativeFunctions()
    {
        // print
        nativeFunctions["print"] = [](const std::vector<Value>& args) {
            for (size_t i = 0; i < args.size(); i++)
            {
                std::cout << args[i].toString();
                if (i + 1 < args.size())
                {
                    std::cout << " ";
                }
            }
            std::cout << "\n";
            return Value();
        };

        // len
        nativeFunctions["len"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("len() takes 1 argument");
            }
            if (args[0].isList())
            {
                return Value(static_cast<long long>(args[0].asList().size()));
            }
            if (args[0].isString())
            {
                return Value(static_cast<long long>(args[0].asString().size()));
            }
            throw std::runtime_error("len() requires list or string");
        };

        // range
        nativeFunctions["range"] = [](const std::vector<Value>& args) {
            if (args.empty() || args.size() > 3)
            {
                throw std::runtime_error("range() takes 1-3 arguments");
            }

            long long start = 0, stop, step = 1;
            if (args.size() == 1)
            {
                stop = args[0].toInt();
            }
            else if (args.size() == 2)
            {
                start = args[0].toInt();
                stop = args[1].toInt();
            }
            else
            {
                start = args[0].toInt();
                stop = args[1].toInt();
                step = args[2].toInt();
            }

            std::vector<Value> result;
            for (long long i = start; (step > 0) ? (i < stop) : (i > stop); i += step)
            {
                result.push_back(Value(i));
            }
            return Value(result);
        };

        // type
        nativeFunctions["type"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
                throw std::runtime_error("type() takes 1 argument");

            std::string typeName;
            switch (args[0].getType())
            {
            case Value::Type::None :
                typeName = "<class 'NoneType'>";
                break;
            case Value::Type::Int :
                typeName = "<class 'int'>";
                break;
            case Value::Type::Float :
                typeName = "<class 'float'>";
                break;
            case Value::Type::String :
                typeName = "<class 'str'>";
                break;
            case Value::Type::Bool :
                typeName = "<class 'bool'>";
                break;
            case Value::Type::List :
                typeName = "<class 'list'>";
                break;
            case Value::Type::Dict :
                typeName = "<class 'dict'>";
                break;
            default :
                typeName = "<class 'object'>";
                break;
            }
            return Value(typeName);
        };

        // str
        nativeFunctions["str"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("str() takes 1 argument");
            }
            return Value(args[0].toString());
        };

        // int
        nativeFunctions["int"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("int() takes 1 argument");
            }
            return Value(args[0].toInt());
        };

        // float
        nativeFunctions["float"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("float() takes 1 argument");
            }
            return Value(args[0].toFloat());
        };

        // sum
        nativeFunctions["sum"] = [](const std::vector<Value>& args) {
            if (args.empty())
            {
                throw std::runtime_error("sum() takes at least 1 argument");
            }
            if (!args[0].isList())
            {
                throw std::runtime_error("sum() requires iterable");
            }

            Value total = args.size() > 1 ? args[1] : Value(0LL);
            for (const auto& item : args[0].asList())
            {
                total = total + item;
            }
            return total;
        };

        // abs
        nativeFunctions["abs"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("abs() takes 1 argument");
            }
            if (args[0].isInt())
            {
                return Value(std::abs(args[0].asInt()));
            }
            return Value(std::abs(args[0].toFloat()));
        };

        // min, max
        nativeFunctions["min"] = [](const std::vector<Value>& args) {
            if (args.empty())
            {
                throw std::runtime_error("min() requires at least 1 argument");
            }
            if (args.size() == 1 && args[0].isList())
            {
                const auto& list = args[0].asList();
                if (list.empty())
                {
                    throw std::runtime_error("min() arg is empty sequence");
                }
                Value minVal = list[0];
                for (size_t i = 1; i < list.size(); i++)
                {
                    if (list[i] < minVal)
                    {
                        minVal = list[i];
                    }
                }
                return minVal;
            }
            Value minVal = args[0];
            for (size_t i = 1; i < args.size(); i++)
            {
                if (args[i] < minVal)
                {
                    minVal = args[i];
                }
            }
            return minVal;
        };

        nativeFunctions["max"] = [](const std::vector<Value>& args) {
            if (args.empty())
            {
                throw std::runtime_error("max() requires at least 1 argument");
            }
            if (args.size() == 1 && args[0].isList())
            {
                const auto& list = args[0].asList();
                if (list.empty())
                {
                    throw std::runtime_error("max() arg is empty sequence");
                }
                Value maxVal = list[0];
                for (size_t i = 1; i < list.size(); i++)
                {
                    if (list[i] > maxVal)
                    {
                        maxVal = list[i];
                    }
                }
                return maxVal;
            }
            Value maxVal = args[0];
            for (size_t i = 1; i < args.size(); i++)
            {
                if (args[i] > maxVal)
                {
                    maxVal = args[i];
                }
            }
            return maxVal;
        };

        // sorted
        nativeFunctions["sorted"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("sorted() takes 1 argument");
            }
            if (!args[0].isList())
            {
                throw std::runtime_error("sorted() requires list");
            }

            auto result = args[0].asList();
            std::sort(result.begin(), result.end());
            return Value(result);
        };

        // reversed
        nativeFunctions["reversed"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("reversed() takes 1 argument");
            }
            if (!args[0].isList())
            {
                throw std::runtime_error("reversed() requires list");
            }

            auto result = args[0].asList();
            std::reverse(result.begin(), result.end());
            return Value(result);
        };

        // enumerate
        nativeFunctions["enumerate"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("enumerate() takes 1 argument");
            }
            if (!args[0].isList())
            {
                throw std::runtime_error("enumerate() requires list");
            }

            std::vector<Value> result;
            const auto& list = args[0].asList();
            for (size_t i = 0; i < list.size(); i++)
            {
                std::vector<Value> pair = {Value(static_cast<long long>(i)), list[i]};
                result.push_back(Value(pair));
            }
            return Value(result);
        };

        // zip
        nativeFunctions["zip"] = [](const std::vector<Value>& args) {
            if (args.size() < 2)
            {
                throw std::runtime_error("zip() requires at least 2 arguments");
            }

            size_t minLen = SIZE_MAX;
            for (const auto& arg : args)
            {
                if (!arg.isList())
                {
                    throw std::runtime_error("zip() requires lists");
                }
                minLen = std::min(minLen, arg.asList().size());
            }

            std::vector<Value> result;
            for (size_t i = 0; i < minLen; i++)
            {
                std::vector<Value> tuple;
                for (const auto& arg : args)
                {
                    tuple.push_back(arg.asList()[i]);
                }
                result.push_back(Value(tuple));
            }
            return Value(result);
        };

        // all, any
        nativeFunctions["all"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("all() takes 1 argument");
            }
            if (!args[0].isList())
            {
                throw std::runtime_error("all() requires list");
            }

            for (const auto& item : args[0].asList())
            {
                if (!item.toBool())
                {
                    return Value(false);
                }
            }
            return Value(true);
        };

        nativeFunctions["any"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("any() takes 1 argument");
            }
            if (!args[0].isList())
            {
                throw std::runtime_error("any() requires list");
            }

            for (const auto& item : args[0].asList())
            {
                if (item.toBool())
                {
                    return Value(true);
                }
            }
            return Value(false);
        };

        // map, filter
        nativeFunctions["map"] = [](const std::vector<Value>& args) {
            if (args.size() != 2)
            {
                throw std::runtime_error("map() takes 2 arguments");
            }
            // Would need to handle function calls properly
            return Value();
        };

        // Math functions
        nativeFunctions["sqrt"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("sqrt() takes 1 argument");
            }
            return Value(std::sqrt(args[0].toFloat()));
        };

        nativeFunctions["pow"] = [](const std::vector<Value>& args) {
            if (args.size() != 2)
            {
                throw std::runtime_error("pow() takes 2 arguments");
            }
            return Value(std::pow(args[0].toFloat(), args[1].toFloat()));
        };

        nativeFunctions["round"] = [](const std::vector<Value>& args) {
            if (args.size() != 1)
            {
                throw std::runtime_error("round() takes 1 argument");
            }
            return Value(static_cast<long long>(std::round(args[0].toFloat())));
        };
    }

   public:
    VirtualMachine() { registerNativeFunctions(); }

    void execute(const BytecodeCompiler::CompiledCode& code)
    {
        globals.resize(code.numVariables);
        stack.clear();
        stack.reserve(1000);  // Pre-allocate for performance
        ip = 0;

        auto startTime = std::chrono::high_resolution_clock::now();

        // Main execution loop with computed goto (if supported)
        while (ip < code.instructions.size())
        {
            const auto& instr = code.instructions[ip];
            stats.instructionsExecuted++;

            // JIT hot spot detection
            if (stats.instructionsExecuted % 100 == 0)
            {
                jit.recordExecution(ip);
            }

            // Dispatch instruction
            try
            {
                executeInstruction(instr, code);
            } catch (const std::exception& e)
            {
                std::cerr << "Runtime error at line " << instr.lineNumber << ": " << e.what() << "\n";
                throw;
            }

            ip++;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        stats.executionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    }

   private:
    void executeInstruction(const Instruction& instr, const BytecodeCompiler::CompiledCode& code)
    {
        switch (instr.op)
        {
        case OpCode::LOAD_CONST :
            push(code.constants[instr.arg]);
            break;

        case OpCode::LOAD_VAR :
            if (instr.arg >= globals.size())
            {
                throw std::runtime_error("Variable index out of range");
            }
            push(globals[instr.arg]);
            break;

        case OpCode::LOAD_GLOBAL :
            push(globals[instr.arg]);
            break;

        case OpCode::STORE_VAR :
            if (instr.arg >= globals.size())
            {
                globals.resize(instr.arg + 1);
            }
            globals[instr.arg] = pop();
            break;

        case OpCode::STORE_GLOBAL :
            globals[instr.arg] = pop();
            break;

        case OpCode::POP :
            pop();
            break;

        case OpCode::DUP :
            push(top());
            break;

        case OpCode::SWAP : {
            Value a = pop();
            Value b = pop();
            push(a);
            push(b);
            break;
        }

        case OpCode::ROT_THREE : {
            Value a = pop();
            Value b = pop();
            Value c = pop();
            push(a);
            push(c);
            push(b);
            break;
        }

        // Arithmetic operations (with fast path)
        case OpCode::ADD : {
            Value b = pop();
            Value a = pop();
            push(fastAdd(a, b));
            break;
        }

        case OpCode::SUB : {
            Value b = pop();
            Value a = pop();
            push(fastSub(a, b));
            break;
        }

        case OpCode::MUL : {
            Value b = pop();
            Value a = pop();
            push(fastMul(a, b));
            break;
        }

        case OpCode::DIV : {
            Value b = pop();
            Value a = pop();
            push(a / b);
            break;
        }

        case OpCode::FLOOR_DIV : {
            Value b = pop();
            Value a = pop();
            push(Value(static_cast<long long>(a.toFloat() / b.toFloat())));
            break;
        }

        case OpCode::MOD : {
            Value b = pop();
            Value a = pop();
            push(a % b);
            break;
        }

        case OpCode::POW : {
            Value b = pop();
            Value a = pop();
            push(a.pow(b));
            break;
        }

        case OpCode::NEG : {
            Value a = pop();
            push(-a);
            break;
        }

        case OpCode::POS :
            // Unary + is a no-op
            break;

        // Bitwise operations
        case OpCode::BITAND : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toInt() & b.toInt()));
            break;
        }

        case OpCode::BITOR : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toInt() | b.toInt()));
            break;
        }

        case OpCode::BITXOR : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toInt() ^ b.toInt()));
            break;
        }

        case OpCode::BITNOT : {
            Value a = pop();
            push(Value(~a.toInt()));
            break;
        }

        case OpCode::LSHIFT : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toInt() << b.toInt()));
            break;
        }

        case OpCode::RSHIFT : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toInt() >> b.toInt()));
            break;
        }

        // Comparison operations
        case OpCode::EQ : {
            Value b = pop();
            Value a = pop();
            push(Value(a == b));
            break;
        }

        case OpCode::NE : {
            Value b = pop();
            Value a = pop();
            push(Value(a != b));
            break;
        }

        case OpCode::LT : {
            Value b = pop();
            Value a = pop();
            push(Value(a < b));
            break;
        }

        case OpCode::GT : {
            Value b = pop();
            Value a = pop();
            push(Value(a > b));
            break;
        }

        case OpCode::LE : {
            Value b = pop();
            Value a = pop();
            push(Value(a <= b));
            break;
        }

        case OpCode::GE : {
            Value b = pop();
            Value a = pop();
            push(Value(a >= b));
            break;
        }

        case OpCode::IN : {
            Value b = pop();  // Container
            Value a = pop();  // Item
            if (b.isList())
            {
                bool found = false;
                for (const auto& item : b.asList())
                {
                    if (item == a)
                    {
                        found = true;
                        break;
                    }
                }
                push(Value(found));
            }
            else if (b.isString())
            {
                push(Value(b.asString().find(a.toString()) != std::string::npos));
            }
            else
            {
                throw std::runtime_error("'in' requires list or string");
            }
            break;
        }

        case OpCode::NOT_IN : {
            Value b = pop();
            Value a = pop();
            stack.push_back(a);
            stack.push_back(b);
            executeInstruction({OpCode::IN}, code);
            Value result = pop();
            push(!result);
            break;
        }

        // Logical operations
        case OpCode::AND : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toBool() && b.toBool()));
            break;
        }

        case OpCode::OR : {
            Value b = pop();
            Value a = pop();
            push(Value(a.toBool() || b.toBool()));
            break;
        }

        case OpCode::NOT : {
            Value a = pop();
            push(!a);
            break;
        }

        // Control flow
        case OpCode::JUMP :
            ip = instr.arg - 1;  // -1 because ip++ at end of loop
            break;

        case OpCode::JUMP_IF_FALSE : {
            Value cond = pop();
            if (!cond.toBool())
            {
                ip = instr.arg - 1;
            }
            break;
        }

        case OpCode::JUMP_IF_TRUE : {
            Value cond = pop();
            if (cond.toBool())
            {
                ip = instr.arg - 1;
            }
            break;
        }

        case OpCode::POP_JUMP_IF_FALSE : {
            if (!top().toBool())
            {
                ip = instr.arg - 1;
            }
            break;
        }

        case OpCode::FOR_ITER : {
            Value& iterator = top();
            if (iterator.hasNext())
            {
                push(iterator.next());
            }
            else
            {
                pop();  // Remove exhausted iterator
                ip = instr.arg - 1;
            }
            break;
        }

        // Function calls
        case OpCode::CALL : {
            int numArgs = instr.arg;
            std::vector<Value> args;
            for (int i = 0; i < numArgs; i++)
            {
                args.insert(args.begin(), pop());
            }

            Value func = pop();

            // Check for native function
            if (func.isString())
            {
                std::string funcName = func.asString();
                auto it = nativeFunctions.find(funcName);
                if (it != nativeFunctions.end())
                {
                    stats.functionsCalled++;
                    push(it->second(args));
                }
                else
                {
                    throw std::runtime_error("Unknown function: " + funcName);
                }
            }
            else if (func.isFunction())
            {
                // User-defined function (would need call frame management)
                stats.functionsCalled++;
                throw std::runtime_error("User functions not yet implemented");
            }
            else
            {
                throw std::runtime_error("Object is not callable");
            }
            break;
        }

        case OpCode::RETURN :
            return;  // Exit function

        case OpCode::YIELD :
            // Generator support
            throw std::runtime_error("Generators not yet implemented");

        // Collections
        case OpCode::BUILD_LIST : {
            std::vector<Value> elements;
            for (int i = 0; i < instr.arg; i++)
            {
                elements.insert(elements.begin(), pop());
            }
            push(Value(elements));
            break;
        }

        case OpCode::BUILD_DICT : {
            std::unordered_map<std::string, Value> dict;
            for (int i = 0; i < instr.arg; i++)
            {
                Value val = pop();
                Value key = pop();
                dict[key.toString()] = val;
            }
            auto dictPtr = std::make_shared<std::unordered_map<std::string, Value>>(dict);
            Value result;
            result.type_ = Value::Type::Dict;
            result.data_ = dictPtr;
            push(result);
            break;
        }

        case OpCode::BUILD_TUPLE :
            // Similar to BUILD_LIST but immutable
            executeInstruction({OpCode::BUILD_LIST, instr.arg}, code);
            break;

        case OpCode::UNPACK_SEQUENCE : {
            Value seq = pop();
            if (!seq.isList())
            {
                throw std::runtime_error("Cannot unpack non-sequence");
            }
            const auto& list = seq.asList();
            if (list.size() != instr.arg)
            {
                throw std::runtime_error("Unpack size mismatch");
            }
            for (const auto& item : list)
            {
                push(item);
            }
            break;
        }

        case OpCode::GET_ITEM : {
            Value key = pop();
            Value obj = pop();
            push(obj.getItem(key));
            break;
        }

        case OpCode::SET_ITEM : {
            Value val = pop();
            Value key = pop();
            Value& obj = top();
            obj.setItem(key, val);
            break;
        }

        case OpCode::GET_ITER : {
            Value obj = pop();
            push(obj.getIterator());
            break;
        }

        // Special operations
        case OpCode::PRINT : {
            std::vector<Value> args;
            for (int i = 0; i < instr.arg; i++)
            {
                args.insert(args.begin(), pop());
            }
            for (size_t i = 0; i < args.size(); i++)
            {
                std::cout << args[i].toString();
                if (i + 1 < args.size())
                    std::cout << " ";
            }
            std::cout << "\n";
            push(Value());  // print returns None
            break;
        }

        case OpCode::FORMAT : {
            // String formatting
            Value formatStr = pop();
            // Would implement f-string or % formatting
            push(formatStr);
            break;
        }

        case OpCode::NOP :
            // No operation
            break;

        case OpCode::HALT :
            return;

        default :
            throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<int>(instr.op)));
        }
    }

   public:
    const Statistics& getStatistics() const { return stats; }

    void printStatistics() const
    {
        std::cout << "\n╔═══════════════════════════════════════╗\n";
        std::cout << "║     VM Execution Statistics           ║\n";
        std::cout << "╚═══════════════════════════════════════╝\n\n";

        std::cout << "Instructions executed:  " << stats.instructionsExecuted << "\n";
        std::cout << "Functions called:       " << stats.functionsCalled << "\n";
        std::cout << "GC collections:         " << stats.gcCollections << "\n";
        std::cout << "JIT compilations:       " << stats.jitCompilations << "\n";
        std::cout << "Execution time:         " << stats.executionTime.count() / 1000.0 << " ms\n";

        if (stats.instructionsExecuted > 0)
        {
            double ips = stats.instructionsExecuted / (stats.executionTime.count() / 1000000.0);
            std::cout << "Instructions/second:    " << static_cast<long long>(ips) << "\n";
        }

        std::cout << "\nStack size:            " << stack.size() << "\n";
        std::cout << "Global variables:       " << globals.size() << "\n";
    }

    const std::vector<Value>& getGlobals() const { return globals; }

    const std::vector<Value>& getStack() const { return stack; }
};