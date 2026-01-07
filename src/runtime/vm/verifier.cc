#include "../../../include/runtime/vm/verifier.hpp"

#include <iostream>


namespace mylang {
namespace runtime {

bool BytecodeVerifier::verify(const BytecodeCompiler::CompilationUnit& unit)
{
  Errors_.clear();
  // Check 1: Valid jump targets
  for (std::size_t i = 0; i < unit.instructions.size(); i++)
  {
    const bytecode::Instruction& instr = unit.instructions[i];
    if (isJumpInstruction(instr.op))
      if (instr.arg < 0 || instr.arg >= unit.instructions.size())
        Errors_.push_back({static_cast<std::int32_t>(i), "Jump target out of bounds: " + std::to_string(instr.arg)});
  }
  // Check 2: Stack balance
  std::vector<std::int32_t> stackDepths(unit.instructions.size(), -1);
  verifyStackDepth(unit, 0, 0, stackDepths);
  // Check 3: Constant pool bounds
  for (std::size_t i = 0; i < unit.instructions.size(); i++)
  {
    const bytecode::Instruction& instr = unit.instructions[i];
    if (instr.op == bytecode::OpCode::LOAD_CONST)
      if (instr.arg < 0 || instr.arg >= unit.constants.size())
        Errors_.push_back({static_cast<std::int32_t>(i), "Constant index out of bounds: " + std::to_string(instr.arg)});
  }
  // Check 4: All paths return (for functions)
  // Would implement dataflow analysis here
  return Errors_.empty();
}

const std::vector<typename BytecodeVerifier::VerificationError>& BytecodeVerifier::getErrors() const { return Errors_; }

void BytecodeVerifier::printErrors(std::ostream& out) const
{
  if (Errors_.empty())
  {
    out << "✓ Bytecode verification passed\n";
    return;
  }
  out << "✗ Bytecode verification failed with " << Errors_.size() << " error(s):\n";
  for (const VerificationError& err : Errors_) out << "  PC " << err.pc << ": " << err.message << "\n";
}

void BytecodeVerifier::verifyStackDepth(const BytecodeCompiler::CompilationUnit& unit,
                                        std::int32_t pc,
                                        std::int32_t depth,
                                        std::vector<std::int32_t>& depths)
{
  if (pc >= unit.instructions.size()) return;
  // Already visited with same or greater depth
  if (depths[pc] >= depth) return;
  depths[pc] = depth;
  const bytecode::Instruction& instr = unit.instructions[pc];
  std::int32_t newDepth = depth + getStackEffect(instr.op, instr.arg);
  if (newDepth < 0)
  {
    Errors_.push_back({pc, "Stack underflow"});
    return;
  }
  if (newDepth > unit.StackSize)
    Errors_.push_back({pc, "Stack overflow (depth " + std::to_string(newDepth) + " > " + std::to_string(unit.StackSize) + ")"});
  // Follow control flow
  if (instr.op == bytecode::OpCode::RETURN || instr.op == bytecode::OpCode::HALT) return;
  if (isJumpInstruction(instr.op))
  {
    verifyStackDepth(unit, instr.arg, newDepth, depths);
    if (!isUnconditionalJump(instr.op)) verifyStackDepth(unit, pc + 1, newDepth, depths);
  }
  else
  {
    verifyStackDepth(unit, pc + 1, newDepth, depths);
  }
}

std::int32_t BytecodeVerifier::getStackEffect(bytecode::OpCode op, std::int32_t arg) const
{
  switch (op)
  {
  case bytecode::OpCode::LOAD_CONST :
  case bytecode::OpCode::LOAD_VAR :
  case bytecode::OpCode::LOAD_FAST :
  case bytecode::OpCode::LOAD_GLOBAL :
  case bytecode::OpCode::DUP : return 1;
  case bytecode::OpCode::POP :
  case bytecode::OpCode::STORE_VAR :
  case bytecode::OpCode::STORE_FAST :
  case bytecode::OpCode::STORE_GLOBAL : return -1;
  case bytecode::OpCode::ADD :
  case bytecode::OpCode::SUB :
  case bytecode::OpCode::MUL :
  case bytecode::OpCode::DIV :
  case bytecode::OpCode::MOD :
  case bytecode::OpCode::POW :
  case bytecode::OpCode::EQ :
  case bytecode::OpCode::NE :
  case bytecode::OpCode::LT :
  case bytecode::OpCode::GT :
  case bytecode::OpCode::LE :
  case bytecode::OpCode::GE : return -1;  // 2 operands -> 1 result
  case bytecode::OpCode::BUILD_LIST :
  case bytecode::OpCode::BUILD_TUPLE :
  case bytecode::OpCode::BUILD_SET : return 1 - arg;  // n items -> 1 collection
  case bytecode::OpCode::CALL : return -arg;          // func + n args -> result
  default : return 0;
  }
}

bool BytecodeVerifier::isJumpInstruction(bytecode::OpCode op) const
{
  return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD
    || op == bytecode::OpCode::JUMP_IF_FALSE || op == bytecode::OpCode::JUMP_IF_TRUE || op == bytecode::OpCode::POP_JUMP_IF_FALSE
    || op == bytecode::OpCode::POP_JUMP_IF_TRUE || op == bytecode::OpCode::FOR_ITER;
}

bool BytecodeVerifier::isUnconditionalJump(bytecode::OpCode op) const
{
  return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD;
}

}
}