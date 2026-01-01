#include "../../../include/runtime/vm/peephole.hpp"

#include <iostream>


namespace mylang {
namespace runtime {

bool PeepholeOptimizer::matchPattern(const std::vector<bytecode::Instruction>& code, std::size_t pos, const std::vector<bytecode::OpCode>& pattern)
{
  if (pos + pattern.size() > code.size()) return false;
  for (std::size_t i = 0; i < pattern.size(); i++)
    if (code[pos + i].op != pattern[i]) return false;
  return true;
}

void PeepholeOptimizer::optimize(std::vector<bytecode::Instruction>& instructions)
{
  std::int32_t replacements = 0;
  // Pattern 1: LOAD_CONST followed by POP -> remove both
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::LOAD_CONST && instructions[i + 1].op == bytecode::OpCode::POP)
    {
      instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Const-Pop elimination", replacements});
  // Pattern 2: LOAD x, STORE x -> remove (redundant load-store)
  replacements = 0;
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    bytecode::Instruction& instr1 = instructions[i];
    bytecode::Instruction& instr2 = instructions[i + 1];

    if ((instr1.op == bytecode::OpCode::LOAD_VAR && instr2.op == bytecode::OpCode::STORE_VAR)
        || (instr1.op == bytecode::OpCode::LOAD_FAST && instr2.op == bytecode::OpCode::STORE_FAST))
    {
      if (instr1.arg == instr2.arg)
      {
        instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
        replacements++;
        continue;
      }
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Load-Store elimination", replacements});
  // Pattern 3: DUP, POP -> remove both (useless dup)
  replacements = 0;
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::DUP && instructions[i + 1].op == bytecode::OpCode::POP)
    {
      instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Dup-Pop elimination", replacements});
  // Pattern 4: JUMP to next instruction -> remove
  replacements = 0;
  for (std::size_t i = 0; i < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::JUMP && instructions[i].arg == i + 1)
    {
      instructions.erase(instructions.begin() + i);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Redundant jump elimination", replacements});
  // Pattern 5: NOT, NOT -> remove both (double negation)
  replacements = 0;
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::NOT && instructions[i + 1].op == bytecode::OpCode::NOT)
    {
      instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Double negation elimination", replacements});
  // Pattern 6: Use fast opcodes for common operations
  replacements = 0;
  for (bytecode::Instruction& instr : instructions)
  {
    if (instr.op == bytecode::OpCode::ADD)
    {
      instr.op = bytecode::OpCode::ADD_FAST;
      replacements++;
    }
    else if (instr.op == bytecode::OpCode::SUB)
    {
      instr.op = bytecode::OpCode::SUB_FAST;
      replacements++;
    }
    else if (instr.op == bytecode::OpCode::MUL)
    {
      instr.op = bytecode::OpCode::MUL_FAST;
      replacements++;
    }
  }
  if (replacements > 0) optimizations_.push_back({"Fast opcode substitution", replacements});
}

const std::vector<typename PeepholeOptimizer::Optimization>& PeepholeOptimizer::getOptimizations() const { return optimizations_; }

void PeepholeOptimizer::printReport() const
{
  if (optimizations_.empty())
  {
    std::cout << "  No peephole optimizations applied\n";
    return;
  }
  std::cout << "  Peephole Optimizations:\n";
  for (const auto& opt : optimizations_)
    std::cout << "    • " << opt.name << ": " << opt.name << " replacements\n";
}

}
}