#include "../../../include/runtime/vm/vm.hpp"
#include "../../../include/diag/diagnostic.hpp"
#include "../../../include/parser/ast/ast.hpp"
#include "../../../include/runtime/object/object.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>


namespace mylang {
namespace runtime {

void VirtualMachine::push(const object::Value& val)
{
  if (Stack_.size() >= 10000) diagnostic::engine.panic("Stack overflow");
  Stack_.push_back(val);
}

object::Value VirtualMachine::pop()
{
  if (Stack_.empty()) diagnostic::engine.panic("Stack underflow");
  object::Value val = Stack_.back();
  Stack_.pop_back();
  return val;
}

object::Value& VirtualMachine::top()
{
  if (Stack_.empty()) diagnostic::engine.panic("Stack empty");
  return Stack_.back();
}

object::Value& VirtualMachine::peek(std::int32_t offset)
{
  if (Stack_.size() < offset + 1) diagnostic::engine.panic("Stack underflow in peek");
  return Stack_[Stack_.size() - 1 - offset];
}

// Fast integer operations (avoid type checking overhead)
object::Value VirtualMachine::fastAdd(const object::Value& a, const object::Value& b)
{
  if (a.isInt() && b.isInt()) return object::Value(a.asInt() + b.asInt());
  return a + b;
}

object::Value VirtualMachine::fastSub(const object::Value& a, const object::Value& b)
{
  if (a.isInt() && b.isInt()) return object::Value(a.asInt() - b.asInt());
  return a - b;
}

object::Value VirtualMachine::fastMul(const object::Value& a, const object::Value& b)
{
  if (a.isInt() && b.isInt()) return object::Value(a.asInt() * b.asInt());
  return a * b;
}

void VirtualMachine::registerNativeFunctions()
{
  // print
  nativeFunctions["print"] = [](const std::vector<object::Value>& args) {
    for (std::size_t i = 0; i < args.size(); i++)
    {
      std::cout << args[i].repr();
      if (i + 1 < args.size()) std::cout << " ";
    }
    std::cout << "\n";
    return object::Value();
  };
  // len
  nativeFunctions["len"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("len() takes 1 argument");
    if (args[0].isList()) return object::Value(static_cast<std::int64_t>(args[0].asList().size()));
    if (args[0].isString()) return object::Value(static_cast<std::int64_t>(args[0].asString().size()));
    diagnostic::engine.panic("len() requires list or string");
  };
  // range
  nativeFunctions["range"] = [](const std::vector<object::Value>& args) {
    if (args.empty() || args.size() > 3) diagnostic::engine.panic("range() takes 1-3 arguments");

    std::int64_t start = 0, stop, step = 1;
    if (args.size() == 1) { stop = args[0].toInt(); }
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
    for (std::int64_t i = start; (step > 0) ? (i < stop) : (i > stop); i += step) result.push_back(object::Value(i));
    return object::Value(result);
  };
  // type
  nativeFunctions["type"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("type() takes 1 argument");
    string_type typeName;
    switch (args[0].getType())
    {
    case object::Value::Type::NONE : typeName = u"<class 'NoneType'>"; break;
    case object::Value::Type::INT : typeName = u"<class 'int'>"; break;
    case object::Value::Type::FLOAT : typeName = u"<class 'float'>"; break;
    case object::Value::Type::STRING : typeName = u"<class 'str'>"; break;
    case object::Value::Type::BOOL : typeName = u"<class 'bool'>"; break;
    case object::Value::Type::LIST : typeName = u"<class 'list'>"; break;
    case object::Value::Type::DICT : typeName = u"<class 'dict'>"; break;
    default : typeName = u"<class 'object'>"; break;
    }
    return object::Value(typeName);
  };
  // str
  nativeFunctions["str"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("str() takes 1 argument");
    return object::Value(args[0].asString());
  };
  // int
  nativeFunctions["int"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("int() takes 1 argument");
    return object::Value(args[0].toInt());
  };
  // float
  nativeFunctions["float"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("float() takes 1 argument");
    return object::Value(args[0].toFloat());
  };
  // sum
  nativeFunctions["sum"] = [](const std::vector<object::Value>& args) {
    if (args.empty()) diagnostic::engine.panic("sum() takes at least 1 argument");
    if (!args[0].isList()) diagnostic::engine.panic("sum() requires iterable");
    object::Value total = args.size() > 1 ? args[1] : object::Value(static_cast<std::int64_t>(0));
    for (const object::Value& item : args[0].asList()) total = total + item;
    return total;
  };
  // abs
  nativeFunctions["abs"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("abs() takes 1 argument");
    if (args[0].isInt()) return object::Value(std::abs(args[0].asInt()));
    return object::Value(std::abs(args[0].toFloat()));
  };
  // min
  nativeFunctions["min"] = [](const std::vector<object::Value>& args) {
    if (args.empty()) diagnostic::engine.panic("min() requires at least 1 argument");
    if (args.size() == 1 && args[0].isList())
    {
      const std::vector<object::Value>& list = args[0].asList();
      if (list.empty()) diagnostic::engine.panic("min() arg is empty sequence");
      object::Value minVal = list[0];
      for (std::size_t i = 1; i < list.size(); i++)
        if (list[i] < minVal) minVal = list[i];
      return minVal;
    }
    object::Value minVal = args[0];
    for (std::size_t i = 1; i < args.size(); i++)
      if (args[i] < minVal) minVal = args[i];
    return minVal;
  };
  // max
  nativeFunctions["max"] = [](const std::vector<object::Value>& args) {
    if (args.empty()) diagnostic::engine.panic("max() requires at least 1 argument");
    if (args.size() == 1 && args[0].isList())
    {
      const std::vector<object::Value>& list = args[0].asList();
      if (list.empty()) diagnostic::engine.panic("max() arg is empty sequence");
      object::Value maxVal = list[0];
      for (std::size_t i = 1; i < list.size(); i++)
        if (list[i] > maxVal) maxVal = list[i];
      return maxVal;
    }
    object::Value maxVal = args[0];
    for (std::size_t i = 1; i < args.size(); i++)
      if (args[i] > maxVal) maxVal = args[i];
    return maxVal;
  };
  // sorted
  nativeFunctions["sorted"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("sorted() takes 1 argument");
    if (!args[0].isList()) diagnostic::engine.panic("sorted() requires list");
    std::vector<object::Value> result = args[0].asList();
    std::sort(result.begin(), result.end());
    return object::Value(result);
  };
  // reversed
  nativeFunctions["reversed"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("reversed() takes 1 argument");
    if (!args[0].isList()) diagnostic::engine.panic("reversed() requires list");
    std::vector<object::Value> result = args[0].asList();
    std::reverse(result.begin(), result.end());
    return object::Value(result);
  };
  // enumerate
  nativeFunctions["enumerate"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("enumerate() takes 1 argument");
    if (!args[0].isList()) diagnostic::engine.panic("enumerate() requires list");
    std::vector<object::Value> result;
    const std::vector<object::Value>& list = args[0].asList();
    for (std::size_t i = 0; i < list.size(); i++)
    {
      std::vector<object::Value> pair = {object::Value(static_cast<std::int64_t>(i)), list[i]};
      result.push_back(object::Value(pair));
    }
    return object::Value(result);
  };
  // zip
  nativeFunctions["zip"] = [](const std::vector<object::Value>& args) {
    if (args.size() < 2) diagnostic::engine.panic("zip() requires at least 2 arguments");
    std::size_t minLen = SIZE_MAX;
    for (const object::Value& arg : args)
    {
      if (!arg.isList()) diagnostic::engine.panic("zip() requires lists");
      minLen = std::min(minLen, arg.asList().size());
    }
    std::vector<object::Value> result;
    for (std::size_t i = 0; i < minLen; i++)
    {
      std::vector<object::Value> tuple;
      for (const object::Value& arg : args) tuple.push_back(arg.asList()[i]);
      result.push_back(object::Value(tuple));
    }
    return object::Value(result);
  };
  // all
  nativeFunctions["all"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("all() takes 1 argument");
    if (!args[0].isList()) diagnostic::engine.panic("all() requires list");
    for (const object::Value& item : args[0].asList())
      if (!item.toBool()) return object::Value(false);
    return object::Value(true);
  };
  // any
  nativeFunctions["any"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("any() takes 1 argument");
    if (!args[0].isList()) diagnostic::engine.panic("any() requires list");
    for (const object::Value& item : args[0].asList())
      if (item.toBool()) return object::Value(true);
    return object::Value(false);
  };
  // map, filter
  nativeFunctions["map"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 2) diagnostic::engine.panic("map() takes 2 arguments");
    // Would need to handle function calls properly
    return object::Value();
  };
  // sqrt
  nativeFunctions["sqrt"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("sqrt() takes 1 argument");
    return object::Value(std::sqrt(args[0].toFloat()));
  };
  // pow
  nativeFunctions["pow"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 2) diagnostic::engine.panic("pow() takes 2 arguments");
    return object::Value(std::pow(args[0].toFloat(), args[1].toFloat()));
  };
  // round
  nativeFunctions["round"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) diagnostic::engine.panic("round() takes 1 argument");
    return object::Value(static_cast<std::int64_t>(std::round(args[0].toFloat())));
  };
}

void VirtualMachine::execute(const BytecodeCompiler::CompilationUnit& code)
{
  Globals_.resize(code.NumCellVars);
  Stack_.clear();
  Stack_.reserve(1000);  // Pre-allocate for performance
  Ip_ = 0;
  auto startTime = std::chrono::high_resolution_clock::now();
  // Main execution loop with computed goto (if supported)
  while (Ip_ < code.instructions.size())
  {
    const bytecode::Instruction& instr = code.instructions[Ip_];
    Stats_.InstructionsExecuted++;
    // JIT hot spot detection
    if (Stats_.InstructionsExecuted % 100 == 0) Jit_.recordExecution(Ip_);
    // Dispatch instruction
    try
    {
      executeInstruction(instr, code);
    } catch (const std::exception& e)
    {
      // std::cerr << "Runtime error at line " << instr.LineNumber << ": " << e.what() << "\n";
      diagnostic::engine.emit("runtime error at line" + std::to_string(instr.LineNumber) + ": " + e.what());
      throw;
    }
    Ip_++;
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  Stats_.ExecutionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void VirtualMachine::executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompilationUnit& code)
{
  switch (instr.op)
  {
  case bytecode::OpCode::LOAD_CONST : push(code.constants[instr.arg]); break;

  case bytecode::OpCode::LOAD_VAR :
    if (instr.arg >= Globals_.size()) diagnostic::engine.panic("Variable index out of range");
    push(Globals_[instr.arg]);
    break;

  case bytecode::OpCode::LOAD_GLOBAL : push(Globals_[instr.arg]); break;

  case bytecode::OpCode::STORE_VAR :
    if (instr.arg >= Globals_.size()) Globals_.resize(instr.arg + 1);
    Globals_[instr.arg] = pop();
    break;

  case bytecode::OpCode::STORE_GLOBAL : Globals_[instr.arg] = pop(); break;

  case bytecode::OpCode::POP : pop(); break;

  case bytecode::OpCode::DUP : push(top()); break;

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
    push(object::Value(static_cast<std::int64_t>(a.toFloat() / b.toFloat())));
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
      for (const object::Value& item : b.asList())
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
      push(object::Value(b.asString().find(a.toString()) != std::string::npos));
    else
      diagnostic::engine.panic("'in' requires list or string");
    break;
  }

  case bytecode::OpCode::NOT_IN : {
    object::Value b = pop();
    object::Value a = pop();
    Stack_.push_back(a);
    Stack_.push_back(b);
    executeInstruction({bytecode::OpCode::IN}, code);
    object::Value result = pop();
    push(!result);
    break;
  }

  // Logical operations
  case bytecode::OpCode::AND : push(object::Value(pop().toBool() && pop().toBool())); break;
  case bytecode::OpCode::OR : push(object::Value(pop().toBool() || pop().toBool())); break;
  case bytecode::OpCode::NOT : push(!pop()); break;

  // Control flow
  case bytecode::OpCode::JUMP : Ip_ = instr.arg - 1; break;  // -1 because ip++ at end of loop

  case bytecode::OpCode::JUMP_IF_FALSE : {
    if (!pop().toBool()) Ip_ = instr.arg - 1;
    break;
  }

  case bytecode::OpCode::JUMP_IF_TRUE : {
    if (pop().toBool()) Ip_ = instr.arg - 1;
    break;
  }

  case bytecode::OpCode::POP_JUMP_IF_FALSE : {
    if (!top().toBool()) Ip_ = instr.arg - 1;
    break;
  }

  case bytecode::OpCode::FOR_ITER : {
    object::Value& iterator = top();
    if (iterator.hasNext()) { push(iterator.next()); }
    else
    {
      pop();  // Remove exhausted iterator
      Ip_ = instr.arg - 1;
    }
    break;
  }

  // Function calls
  case bytecode::OpCode::CALL : {
    std::int32_t numArgs = instr.arg;
    std::vector<object::Value> args;
    for (std::int32_t i = 0; i < numArgs; i++) args.insert(args.begin(), pop());
    object::Value func = pop();
    // Check for native function
    if (func.isString())
    {
      std::string funcName = utf8::utf16to8(func.asString());
      auto it = nativeFunctions.find(funcName);
      if (it != nativeFunctions.end())
      {
        Stats_.FunctionsCalled++;
        push(it->second(args));
      }
      else
      {
        diagnostic::engine.panic("Unknown function: " + funcName);
      }
    }
    else if (func.isFunction())
    {
      // User-defined function (would need call frame management)
      Stats_.FunctionsCalled++;
      diagnostic::engine.panic("User functions not yet implemented");
    }
    else
    {
      diagnostic::engine.panic("Object is not callable");
    }
    break;
  }

  case bytecode::OpCode::RETURN : return;  // Exit function

  case bytecode::OpCode::YIELD :
    // Generator support
    diagnostic::engine.panic("Generators not yet implemented");

    // Collections
  case bytecode::OpCode::BUILD_LIST : {
    std::vector<object::Value> elements;
    for (std::int32_t i = 0; i < instr.arg; i++) elements.insert(elements.begin(), pop());
    push(object::Value(elements));
    break;
  }

  case bytecode::OpCode::BUILD_DICT : {
    std::unordered_map<string_type, object::Value> dict;
    for (std::int32_t i = 0; i < instr.arg; i++)
    {
      object::Value val = pop();
      object::Value key = pop();
      dict[key.asString()] = val;
    }
    std::shared_ptr<std::unordered_map<string_type, object::Value>> dict_ptr = std::make_shared<std::unordered_map<string_type, object::Value>>(dict);
    object::Value result;
    result.setType(object::Value::Type::DICT);
    result.setData(dict_ptr);
    push(result);
    break;
  }

  case bytecode::OpCode::BUILD_TUPLE :
    // Similar to BUILD_LIST but immutable
    executeInstruction({bytecode::OpCode::BUILD_LIST, instr.arg}, code);
    break;

  case bytecode::OpCode::UNPACK_SEQUENCE : {
    object::Value seq = pop();
    if (!seq.isList()) diagnostic::engine.panic("Cannot unpack non-sequence");
    const std::vector<object::Value>& list = seq.asList();
    if (list.size() != instr.arg) diagnostic::engine.panic("Unpack size mismatch");
    for (const object::Value& item : list) push(item);
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
    for (std::int32_t i = 0; i < instr.arg; i++) args.insert(args.begin(), pop());
    for (std::size_t i = 0; i < args.size(); i++)
    {
      std::cout << args[i].repr();
      if (i + 1 < args.size()) std::cout << " ";
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

  case bytecode::OpCode::HALT : return;

  default : diagnostic::engine.panic("Unknown opcode: " + std::to_string(static_cast<std::int32_t>(instr.op)));
  }
}

void VirtualMachine::printStatistics() const
{
  std::cout << '\n';
  std::cout << "╔═══════════════════════════════════════╗\n";
  std::cout << "║     VM Execution Statistics           ║\n";
  std::cout << "╚═══════════════════════════════════════╝\n\n";
  std::cout << "Instructions executed : " << Stats_.InstructionsExecuted << "\n";
  std::cout << "Functions called      : " << Stats_.FunctionsCalled << "\n";
  std::cout << "GC collections        : " << Stats_.GcCollections << "\n";
  std::cout << "JIT compilations      : " << Stats_.JitCompilations << "\n";
  std::cout << "Execution time        : " << Stats_.ExecutionTime.count() / 1000.0 << " ms\n";
  std::cout << "-----------------------------------------" << std::endl;
  if (Stats_.InstructionsExecuted > 0)
  {
    double ips = Stats_.InstructionsExecuted / (Stats_.ExecutionTime.count() / 1000000.0);
    std::cout << "Instructions/second : " << static_cast<std::int64_t>(ips) << "\n";
  }
  std::cout << "\nStack size          : " << Stack_.size() << "\n";
  std::cout << "Global variables      : " << Globals_.size() << "\n";
}

}
}