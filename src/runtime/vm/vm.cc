#include "../../../include/runtime/vm/vm.h"
#include "../../../include/runtime/object/object.h"

#include <iostream>


namespace mylang {
namespace runtime {

void VirtualMachine::push(const object::Value& val)
{
    if (stack_.size() >= 10000)
    {
        throw std::runtime_error("Stack overflow");
    }
    stack_.push_back(val);
}

object::Value VirtualMachine::pop()
{
    if (stack_.empty())
    {
        throw std::runtime_error("Stack underflow");
    }
    object::Value val = stack_.back();
    stack_.pop_back();
    return val;
}

object::Value& VirtualMachine::top()
{
    if (stack_.empty())
    {
        throw std::runtime_error("Stack empty");
    }
    return stack_.back();
}

object::Value& VirtualMachine::peek(int offset)
{
    if (stack_.size() < offset + 1)
    {
        throw std::runtime_error("Stack underflow in peek");
    }
    return stack_[stack_.size() - 1 - offset];
}

// Fast integer operations (avoid type checking overhead)
object::Value VirtualMachine::fastAdd(const object::Value& a, const object::Value& b)
{
    if (a.isInt() && b.isInt())
    {
        return object::Value(a.asInt() + b.asInt());
    }
    return a + b;
}

object::Value VirtualMachine::fastSub(const object::Value& a, const object::Value& b)
{
    if (a.isInt() && b.isInt())
    {
        return object::Value(a.asInt() - b.asInt());
    }
    return a - b;
}

object::Value VirtualMachine::fastMul(const object::Value& a, const object::Value& b)
{
    if (a.isInt() && b.isInt())
    {
        return object::Value(a.asInt() * b.asInt());
    }
    return a * b;
}

void VirtualMachine::registerNativeFunctions()
{
    // print
    nativeFunctions["print"] = [](const std::vector<object::Value>& args) {
        for (size_t i = 0; i < args.size(); i++)
        {
            std::cout << args[i].toString();
            if (i + 1 < args.size())
            {
                std::cout << " ";
            }
        }
        std::cout << "\n";
        return object::Value();
    };

    // len
    nativeFunctions["len"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("len() takes 1 argument");
        }
        if (args[0].isList())
        {
            return object::Value(static_cast<long long>(args[0].asList().size()));
        }
        if (args[0].isString())
        {
            return object::Value(static_cast<long long>(args[0].asString().size()));
        }
        throw std::runtime_error("len() requires list or string");
    };

    // range
    nativeFunctions["range"] = [](const std::vector<object::Value>& args) {
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

        std::vector<object::Value> result;
        for (long long i = start; (step > 0) ? (i < stop) : (i > stop); i += step)
        {
            result.push_back(object::Value(i));
        }
        return object::Value(result);
    };

    // type
    nativeFunctions["type"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("type() takes 1 argument");
        }

        std::u16string typeName;
        switch (args[0].getType())
        {
        case object::Value::Type::NONE :
            typeName = u"<class 'NoneType'>";
            break;
        case object::Value::Type::INT :
            typeName = u"<class 'int'>";
            break;
        case object::Value::Type::FLOAT :
            typeName = u"<class 'float'>";
            break;
        case object::Value::Type::STRING :
            typeName = u"<class 'str'>";
            break;
        case object::Value::Type::BOOL :
            typeName = u"<class 'bool'>";
            break;
        case object::Value::Type::LIST :
            typeName = u"<class 'list'>";
            break;
        case object::Value::Type::DICT :
            typeName = u"<class 'dict'>";
            break;
        default :
            typeName = u"<class 'object'>";
            break;
        }
        return object::Value(typeName);
    };

    // str
    nativeFunctions["str"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("str() takes 1 argument");
        }
        return object::Value(args[0].asString());
    };

    // int
    nativeFunctions["int"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("int() takes 1 argument");
        }
        return object::Value(args[0].toInt());
    };

    // float
    nativeFunctions["float"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("float() takes 1 argument");
        }
        return object::Value(args[0].toFloat());
    };

    // sum
    nativeFunctions["sum"] = [](const std::vector<object::Value>& args) {
        if (args.empty())
        {
            throw std::runtime_error("sum() takes at least 1 argument");
        }

        if (!args[0].isList())
        {
            throw std::runtime_error("sum() requires iterable");
        }

        object::Value total = args.size() > 1 ? args[1] : object::Value(0LL);
        for (const auto& item : args[0].asList())
        {
            total = total + item;
        }
        return total;
    };

    // abs
    nativeFunctions["abs"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("abs() takes 1 argument");
        }
        if (args[0].isInt())
        {
            return object::Value(std::abs(args[0].asInt()));
        }
        return object::Value(std::abs(args[0].toFloat()));
    };

    // min, max
    nativeFunctions["min"] = [](const std::vector<object::Value>& args) {
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
            object::Value minVal = list[0];
            for (size_t i = 1; i < list.size(); i++)
            {
                if (list[i] < minVal)
                {
                    minVal = list[i];
                }
            }
            return minVal;
        }
        object::Value minVal = args[0];
        for (size_t i = 1; i < args.size(); i++)
        {
            if (args[i] < minVal)
            {
                minVal = args[i];
            }
        }
        return minVal;
    };

    nativeFunctions["max"] = [](const std::vector<object::Value>& args) {
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
            object::Value maxVal = list[0];
            for (size_t i = 1; i < list.size(); i++)
            {
                if (list[i] > maxVal)
                {
                    maxVal = list[i];
                }
            }
            return maxVal;
        }
        object::Value maxVal = args[0];
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
    nativeFunctions["sorted"] = [](const std::vector<object::Value>& args) {
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
        return object::Value(result);
    };

    // reversed
    nativeFunctions["reversed"] = [](const std::vector<object::Value>& args) {
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
        return object::Value(result);
    };

    // enumerate
    nativeFunctions["enumerate"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("enumerate() takes 1 argument");
        }
        if (!args[0].isList())
        {
            throw std::runtime_error("enumerate() requires list");
        }

        std::vector<object::Value> result;
        const auto& list = args[0].asList();
        for (size_t i = 0; i < list.size(); i++)
        {
            std::vector<object::Value> pair = {object::Value(static_cast<long long>(i)), list[i]};
            result.push_back(object::Value(pair));
        }
        return object::Value(result);
    };

    // zip
    nativeFunctions["zip"] = [](const std::vector<object::Value>& args) {
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

        std::vector<object::Value> result;
        for (size_t i = 0; i < minLen; i++)
        {
            std::vector<object::Value> tuple;
            for (const auto& arg : args)
            {
                tuple.push_back(arg.asList()[i]);
            }
            result.push_back(object::Value(tuple));
        }
        return object::Value(result);
    };

    // all, any
    nativeFunctions["all"] = [](const std::vector<object::Value>& args) {
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
                return object::Value(false);
            }
        }
        return object::Value(true);
    };

    nativeFunctions["any"] = [](const std::vector<object::Value>& args) {
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
                return object::Value(true);
            }
        }
        return object::Value(false);
    };

    // map, filter
    nativeFunctions["map"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 2)
        {
            throw std::runtime_error("map() takes 2 arguments");
        }
        // Would need to handle function calls properly
        return object::Value();
    };

    // Math functions
    nativeFunctions["sqrt"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("sqrt() takes 1 argument");
        }
        return object::Value(std::sqrt(args[0].toFloat()));
    };

    nativeFunctions["pow"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 2)
        {
            throw std::runtime_error("pow() takes 2 arguments");
        }
        return object::Value(std::pow(args[0].toFloat(), args[1].toFloat()));
    };

    nativeFunctions["round"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("round() takes 1 argument");
        }
        return object::Value(static_cast<long long>(std::round(args[0].toFloat())));
    };
}

void VirtualMachine::execute(const BytecodeCompiler::CompiledCode& code)
{
    globals_.resize(code.numVariables);
    stack_.clear();
    stack_.reserve(1000);  // Pre-allocate for performance
    ip_ = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Main execution loop with computed goto (if supported)
    while (ip_ < code.instructions.size())
    {
        const auto& instr = code.instructions[ip_];
        stats_.instructionsExecuted++;

        // JIT hot spot detection
        if (stats_.instructionsExecuted % 100 == 0)
        {
            jit_.recordExecution(ip_);
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

        ip_++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    stats_.executionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void VirtualMachine::executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompiledCode& code)
{
    switch (instr.op)
    {
    case bytecode::OpCode::LOAD_CONST :
        push(code.constants[instr.arg]);
        break;

    case bytecode::OpCode::LOAD_VAR :
        if (instr.arg >= globals_.size())
        {
            throw std::runtime_error("Variable index out of range");
        }
        push(globals_[instr.arg]);
        break;

    case bytecode::OpCode::LOAD_GLOBAL :
        push(globals_[instr.arg]);
        break;

    case bytecode::OpCode::STORE_VAR :
        if (instr.arg >= globals_.size())
        {
            globals_.resize(instr.arg + 1);
        }
        globals_[instr.arg] = pop();
        break;

    case bytecode::OpCode::STORE_GLOBAL :
        globals_[instr.arg] = pop();
        break;

    case bytecode::OpCode::POP :
        pop();
        break;

    case bytecode::OpCode::DUP :
        push(top());
        break;

    case bytecode::OpCode::SWAP : {
        object::Value a = pop();
        object::Value b = pop();
        push(a);
        push(b);
        break;
    }

    case bytecode::OpCode::ROT_THREE : {
        object::Value a = pop();
        object::Value b = pop();
        object::Value c = pop();
        push(a);
        push(c);
        push(b);
        break;
    }

    // Arithmetic operations (with fast path)
    case bytecode::OpCode::ADD : {
        object::Value b = pop();
        object::Value a = pop();
        push(fastAdd(a, b));
        break;
    }

    case bytecode::OpCode::SUB : {
        object::Value b = pop();
        object::Value a = pop();
        push(fastSub(a, b));
        break;
    }

    case bytecode::OpCode::MUL : {
        object::Value b = pop();
        object::Value a = pop();
        push(fastMul(a, b));
        break;
    }

    case bytecode::OpCode::DIV : {
        object::Value b = pop();
        object::Value a = pop();
        push(a / b);
        break;
    }

    case bytecode::OpCode::FLOOR_DIV : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(static_cast<long long>(a.toFloat() / b.toFloat())));
        break;
    }

    case bytecode::OpCode::MOD : {
        object::Value b = pop();
        object::Value a = pop();
        push(a % b);
        break;
    }

    case bytecode::OpCode::POW : {
        object::Value b = pop();
        object::Value a = pop();
        push(a.pow(b));
        break;
    }

    case bytecode::OpCode::NEG : {
        object::Value a = pop();
        push(-a);
        break;
    }

    case bytecode::OpCode::POS :
        // Unary + is a no-op
        break;

    // Bitwise operations
    case bytecode::OpCode::BITAND : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() & b.toInt()));
        break;
    }

    case bytecode::OpCode::BITOR : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() | b.toInt()));
        break;
    }

    case bytecode::OpCode::BITXOR : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() ^ b.toInt()));
        break;
    }

    case bytecode::OpCode::BITNOT : {
        object::Value a = pop();
        push(object::Value(~a.toInt()));
        break;
    }

    case bytecode::OpCode::LSHIFT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() << b.toInt()));
        break;
    }

    case bytecode::OpCode::RSHIFT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() >> b.toInt()));
        break;
    }

    // Comparison operations
    case bytecode::OpCode::EQ : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a == b));
        break;
    }

    case bytecode::OpCode::NE : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a != b));
        break;
    }

    case bytecode::OpCode::LT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a < b));
        break;
    }

    case bytecode::OpCode::GT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a > b));
        break;
    }

    case bytecode::OpCode::LE : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a <= b));
        break;
    }

    case bytecode::OpCode::GE : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a >= b));
        break;
    }

    case bytecode::OpCode::IN : {
        object::Value b = pop();  // Container
        object::Value a = pop();  // Item
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
            push(object::Value(found));
        }
        else if (b.isString())
        {
            push(object::Value(b.asString().find(utf8::utf8to16(a.toString())) != std::string::npos));
        }
        else
        {
            throw std::runtime_error("'in' requires list or string");
        }
        break;
    }

    case bytecode::OpCode::NOT_IN : {
        object::Value b = pop();
        object::Value a = pop();
        stack_.push_back(a);
        stack_.push_back(b);
        executeInstruction({bytecode::OpCode::IN}, code);
        object::Value result = pop();
        push(!result);
        break;
    }

    // Logical operations
    case bytecode::OpCode::AND : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toBool() && b.toBool()));
        break;
    }

    case bytecode::OpCode::OR : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toBool() || b.toBool()));
        break;
    }

    case bytecode::OpCode::NOT : {
        object::Value a = pop();
        push(!a);
        break;
    }

    // Control flow
    case bytecode::OpCode::JUMP :
        ip_ = instr.arg - 1;  // -1 because ip++ at end of loop
        break;

    case bytecode::OpCode::JUMP_IF_FALSE : {
        object::Value cond = pop();
        if (!cond.toBool())
        {
            ip_ = instr.arg - 1;
        }
        break;
    }

    case bytecode::OpCode::JUMP_IF_TRUE : {
        object::Value cond = pop();
        if (cond.toBool())
        {
            ip_ = instr.arg - 1;
        }
        break;
    }

    case bytecode::OpCode::POP_JUMP_IF_FALSE : {
        if (!top().toBool())
        {
            ip_ = instr.arg - 1;
        }
        break;
    }

    case bytecode::OpCode::FOR_ITER : {
        object::Value& iterator = top();
        if (iterator.hasNext())
        {
            push(iterator.next());
        }
        else
        {
            pop();  // Remove exhausted iterator
            ip_ = instr.arg - 1;
        }
        break;
    }

    // Function calls
    case bytecode::OpCode::CALL : {
        int numArgs = instr.arg;
        std::vector<object::Value> args;
        for (int i = 0; i < numArgs; i++)
        {
            args.insert(args.begin(), pop());
        }

        object::Value func = pop();

        // Check for native function
        if (func.isString())
        {
            std::string funcName = utf8::utf16to8(func.asString());
            auto it = nativeFunctions.find(funcName);
            if (it != nativeFunctions.end())
            {
                stats_.functionsCalled++;
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
            stats_.functionsCalled++;
            throw std::runtime_error("User functions not yet implemented");
        }
        else
        {
            throw std::runtime_error("Object is not callable");
        }
        break;
    }

    case bytecode::OpCode::RETURN :
        return;  // Exit function

    case bytecode::OpCode::YIELD :
        // Generator support
        throw std::runtime_error("Generators not yet implemented");

    // Collections
    case bytecode::OpCode::BUILD_LIST : {
        std::vector<object::Value> elements;
        for (int i = 0; i < instr.arg; i++)
        {
            elements.insert(elements.begin(), pop());
        }
        push(object::Value(elements));
        break;
    }

    case bytecode::OpCode::BUILD_DICT : {
        std::unordered_map<std::u16string, object::Value> dict;
        for (int i = 0; i < instr.arg; i++)
        {
            object::Value val = pop();
            object::Value key = pop();
            dict[key.toString()] = val;
        }
        auto dictPtr = std::make_shared<std::unordered_map<std::string, object::Value>>(dict);
        object::Value result;
        result.setType(object::Value::Type::DICT);
        result.setData(dictPtr);
        push(result);
        break;
    }

    case bytecode::OpCode::BUILD_TUPLE :
        // Similar to BUILD_LIST but immutable
        executeInstruction({bytecode::OpCode::BUILD_LIST, instr.arg}, code);
        break;

    case bytecode::OpCode::UNPACK_SEQUENCE : {
        object::Value seq = pop();
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

    case bytecode::OpCode::GET_ITEM : {
        object::Value key = pop();
        object::Value obj = pop();
        push(obj.getItem(key));
        break;
    }

    case bytecode::OpCode::SET_ITEM : {
        object::Value val = pop();
        object::Value key = pop();
        object::Value& obj = top();
        obj.setItem(key, val);
        break;
    }

    case bytecode::OpCode::GET_ITER : {
        object::Value obj = pop();
        push(obj.getIterator());
        break;
    }

    // Special operations
    case bytecode::OpCode::PRINT : {
        std::vector<object::Value> args;
        for (int i = 0; i < instr.arg; i++)
        {
            args.insert(args.begin(), pop());
        }
        for (size_t i = 0; i < args.size(); i++)
        {
            std::cout << args[i].toString();
            if (i + 1 < args.size())
            {
                std::cout << " ";
            }
        }
        std::cout << "\n";
        push(object::Value());  // print returns None
        break;
    }

    case bytecode::OpCode::FORMAT : {
        // String formatting
        object::Value formatStr = pop();
        // Would implement f-string or % formatting
        push(formatStr);
        break;
    }

    case bytecode::OpCode::NOP :
        // No operation
        break;

    case bytecode::OpCode::HALT :
        return;

    default :
        throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<int>(instr.op)));
    }
}

void VirtualMachine::printStatistics() const
{
    std::cout << "\n╔═══════════════════════════════════════╗\n";
    std::cout << "║     VM Execution Statistics           ║\n";
    std::cout << "╚═══════════════════════════════════════╝\n\n";

    std::cout << "Instructions executed:  " << stats_.instructionsExecuted << "\n";
    std::cout << "Functions called:       " << stats_.functionsCalled << "\n";
    std::cout << "GC collections:         " << stats_.gcCollections << "\n";
    std::cout << "JIT compilations:       " << stats_.jitCompilations << "\n";
    std::cout << "Execution time:         " << stats_.executionTime.count() / 1000.0 << " ms\n";

    if (stats_.instructionsExecuted > 0)
    {
        double ips = stats_.instructionsExecuted / (stats_.executionTime.count() / 1000000.0);
        std::cout << "Instructions/second:    " << static_cast<long long>(ips) << "\n";
    }

    std::cout << "\nStack size:            " << stack_.size() << "\n";
    std::cout << "Global variables:       " << globals_.size() << "\n";
}

}
}
