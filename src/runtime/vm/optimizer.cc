#include "../../../include/runtime/vm/optimizer.hpp"

#include <iostream>


namespace mylang {
namespace runtime {

void BytecodeOptimizer::optimize(std::vector<bytecode::Instruction>& code, std::int32_t maxIterations)
{
  bool         changed   = true;
  std::int32_t iteration = 0;
  while (changed && iteration < maxIterations)
  {
    changed = false;
    for (OptimizationPass& pass : Passes_)
    {
      if (pass.apply(code))
      {
        pass.ApplicationsCount++;
        changed = true;
      }
    }
    iteration++;
  }
}

void BytecodeOptimizer::printReport(std::ostream& out) const
{
  out << "\nBytecode Optimizer Report:\n";
  for (const OptimizationPass& pass : Passes_)
    if (pass.ApplicationsCount > 0)
      out << "  • " << pass.name << ": " << pass.ApplicationsCount << " applications\n";
}

bool BytecodeOptimizer::isJumpTarget(const std::vector<bytecode::Instruction>& code, SizeType pos)
{
  // Check if any instruction jumps to this position
  for (const bytecode::Instruction& instr : code)
    if (isJumpOp(instr.op) && instr.arg == pos)
      return true;
  return false;
}

bool BytecodeOptimizer::isJumpOp(bytecode::OpCode op)
{
  return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD
         || op == bytecode::OpCode::JUMP_IF_FALSE || op == bytecode::OpCode::JUMP_IF_TRUE || op == bytecode::OpCode::POP_JUMP_IF_FALSE
         || op == bytecode::OpCode::POP_JUMP_IF_TRUE;
}

bool BytecodeOptimizer::isBinaryOp(bytecode::OpCode op)
{
  return op == bytecode::OpCode::ADD || op == bytecode::OpCode::SUB || op == bytecode::OpCode::MUL || op == bytecode::OpCode::DIV
         || op == bytecode::OpCode::MOD || op == bytecode::OpCode::POW;
}

}
}