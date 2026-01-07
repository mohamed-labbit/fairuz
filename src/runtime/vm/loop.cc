#include "../../../include/runtime/vm/loop.hpp"


namespace mylang {
namespace runtime {

void LoopAnalyzer::detectLoops(const std::vector<bytecode::Instruction>& instructions)
{
  // Detect back-edges (jumps to earlier instructions)
  for (std::size_t i = 0; i < instructions.size(); i++)
  {
    const bytecode::Instruction& instr = instructions[i];
    if ((instr.op == bytecode::OpCode::JUMP_BACKWARD || instr.op == bytecode::OpCode::FOR_ITER) && instr.arg < i)
    {
      Loop loop;
      loop.HeaderPC = instr.arg;
      loop.ExitPC = i;
      loop.IsInnerLoop = true;
      loop.NestingLevel = 1;
      // Collect loop body
      for (std::int32_t pc = instr.arg; pc <= i; pc++) loop.BodyPCs.push_back(pc);
      Loops_.push_back(loop);
    }
  }

  // Calculate nesting levels
  for (Loop& outer : Loops_)
    for (const Loop& inner : Loops_)
      if (inner.HeaderPC > outer.HeaderPC && inner.ExitPC < outer.ExitPC) outer.IsInnerLoop = false;
  /// @todo: inner is nested in outer
}

void LoopAnalyzer::findInvariants(const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols)
{
  for (Loop& loop : Loops_)
  {
    std::unordered_set<std::int32_t> modifiedVars;
    std::unordered_set<std::int32_t> usedVars;
    for (std::int32_t pc : loop.BodyPCs)
    {
      const bytecode::Instruction& instr = instructions[pc];
      if (instr.op == bytecode::OpCode::STORE_VAR || instr.op == bytecode::OpCode::STORE_FAST) modifiedVars.insert(instr.arg);
      if (instr.op == bytecode::OpCode::LOAD_VAR || instr.op == bytecode::OpCode::LOAD_FAST) usedVars.insert(instr.arg);
    }
    for (std::int32_t var : usedVars)
      if (!modifiedVars.count(var)) loop.invariants.insert(var);
  }
}

const std::vector<typename LoopAnalyzer::Loop>& LoopAnalyzer::getLoops() const { return Loops_; }

}
}