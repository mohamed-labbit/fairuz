#pragma once

#include <string>
#include <vector>

#include "btcompiler.hpp"
#include "bytecode.hpp"


namespace mylang {
namespace runtime {

// Ensure bytecode correctness
class BytecodeVerifier
{
 public:
  struct VerificationError
  {
    std::int32_t pc;
    std::string  message;
  };

 private:
  std::vector<VerificationError> Errors_;

 public:
  bool                                  verify(const BytecodeCompiler::CompilationUnit& unit);
  const std::vector<VerificationError>& getErrors() const;
  void                                  printErrors(std::ostream& out) const;

 private:
  void verifyStackDepth(const BytecodeCompiler::CompilationUnit& unit, std::int32_t pc, std::int32_t depth, std::vector<std::int32_t>& depths);
  std::int32_t getStackEffect(bytecode::OpCode op, std::int32_t arg) const;
  bool         isJumpInstruction(bytecode::OpCode op) const;
  bool         isUnconditionalJump(bytecode::OpCode op) const;
};  // ByteCodeVerifier

}
}