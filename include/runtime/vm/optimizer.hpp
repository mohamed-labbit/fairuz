#pragma once

#include <algorithm>
#include <functional>
#include <string>

#include "../../macros.hpp"
#include "bytecode.hpp"


namespace mylang {
namespace runtime {

// ============================================================================
// BYTECODE OPTIMIZER - Cross-instruction optimization
// ============================================================================

class BytecodeOptimizer
{
 public:
  struct OptimizationPass
  {
    std::string                                              name;
    std::function<bool(std::vector<bytecode::Instruction>&)> apply;
    std::int32_t                                             ApplicationsCount = 0;
  };

 private:
  std::vector<OptimizationPass> Passes_;

 public:
  BytecodeOptimizer()
  {
    // Register optimization passes
    // Pass 1: Dead code elimination after returns
    Passes_.push_back({"Dead code after return", [](std::vector<bytecode::Instruction>& code) -> bool {
                         bool changed = false;
                         for (SizeType i = 0; i + 1 < code.size(); i++)
                         {
                           if (code[i].op == bytecode::OpCode::RETURN || code[i].op == bytecode::OpCode::HALT)
                           {
                             // Remove instructions until next label/jump target
                             SizeType toRemove = 0;
                             for (SizeType j = i + 1; j < code.size(); j++)
                             {
                               if (isJumpTarget(code, j))
                                 break;
                               toRemove++;
                             }
                             if (toRemove > 0)
                             {
                               code.erase(code.begin() + i + 1, code.begin() + i + 1 + toRemove);
                               changed = true;
                             }
                           }
                         }
                         return changed;
                       }});
    // Pass 2: Constant folding in bytecode
    Passes_.push_back({"Constant folding", [](std::vector<bytecode::Instruction>& code) -> bool {
                         bool changed = false;
                         // Find LOAD_CONST, LOAD_CONST, binary_op patterns
                         for (SizeType i = 0; i + 2 < code.size(); i++)
                         {
                           if (code[i].op == bytecode::OpCode::LOAD_CONST && code[i + 1].op == bytecode::OpCode::LOAD_CONST
                               && isBinaryOp(code[i + 2].op))
                             // Would fold constants here
                             changed = true;
                         }
                         return changed;
                       }});
    // Pass 3: Jump threading
    Passes_.push_back({"Jump threading", [](std::vector<bytecode::Instruction>& code) -> bool {
                         bool changed = false;
                         // If jump target is another jump, redirect
                         for (bytecode::Instruction& instr : code)
                         {
                           if (isJumpOp(instr.op))
                           {
                             std::int32_t target = instr.arg;
                             if (target < code.size() && code[target].op == bytecode::OpCode::JUMP)
                             {
                               instr.arg = code[target].arg;
                               changed   = true;
                             }
                           }
                         }
                         return changed;
                       }});
    // Pass 4: Remove NOPs
    Passes_.push_back({"NOP elimination", [](std::vector<bytecode::Instruction>& code) -> bool {
                         auto it      = std::remove_if(code.begin(), code.end(),
                                                       [](const bytecode::Instruction& instr) { return instr.op == bytecode::OpCode::NOP; });
                         bool changed = it != code.end();
                         code.erase(it, code.end());
                         return changed;
                       }});
  }

  void optimize(std::vector<bytecode::Instruction>& code, std::int32_t maxIterations = 10);

  void printReport(std::ostream& out) const;

 private:
  static bool isJumpTarget(const std::vector<bytecode::Instruction>& code, SizeType pos);

  static bool isJumpOp(bytecode::OpCode op);

  static bool isBinaryOp(bytecode::OpCode op);
};  // BytecodeOptimizer

}
}