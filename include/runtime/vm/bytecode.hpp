#pragma once


#include <cstdint>


namespace mylang {
namespace runtime {
namespace bytecode {

enum class OpCode : std::uint8_t {
  // Stack operations
  LOAD_CONST,  // Push constant onto stack
  LOAD_VAR,    // Load local variable
  LOAD_FAST,
  LOAD_GLOBAL,   // Load global variable
  STORE_VAR,     // Store to local variable
  STORE_GLOBAL,  // Store to global variable
  STORE_FAST,
  LOAD_ATTR,   // Load attribute from object
  STORE_ATTR,  // Store attribute to object
  POP,         // Pop top of stack
  DUP,         // Duplicate top of stack
  SWAP,        // Swap top two stack items
  ROT_THREE,   // Rotate top three stack items

  // Arithmetic (optimized for int fast path)
  ADD,
  ADD_FAST,
  SUB,
  SUB_FAST,
  MUL,
  MUL_FAST,
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
  JUMP_FORWARD,
  JUMP_BACKWARD,
  JUMP_IF_FALSE,  // Conditional jump (pop)
  JUMP_IF_TRUE,   // Conditional jump (pop)
  POP_JUMP_IF_TRUE,
  POP_JUMP_IF_FALSE,  // Conditional jump (keep on stack)
  FOR_ITER,           // For loop iteration
  FOR_ITER_FAST,
  HOT_LOOP_END,
  HOT_LOOP_START,

  // Functions & Methods
  CALL,  // Call function
  CALL_FAST,
  CALL_METHOD,  // Call method (optimized)
  RETURN,       // Return from function
  YIELD,        // Generator yield

  // Collections
  BUILD_LIST,   // Build list from stack
  BUILD_DICT,   // Build dict from stack
  BUILD_SET,    // Build set from stack
  BUILD_TUPLE,  // Build tuple from stack
  BUILD_SLICE,  // Build slice object

  UNPACK_SEQUENCE,  // Unpack sequence to stack
  GET_ITEM,         // Get item from list/dict
  SET_ITEM,         // Set item in list/dict
  DEL_ITEM,         // Delete item from list/dict

  // Advanced
  LOAD_CLOSURE,   // Load closure variable
  MAKE_FUNCTION,  // Create function object
  IMPORT_NAME,    // Import module
  GET_ITER,       // Get iterator

  // Exception handling
  SETUP_TRY,  // Setup try block
  POP_TRY,    // Pop try block
  RAISE,      // Raise exception

  // Special
  PRINT,   // Built-in print (optimized)
  FORMAT,  // String formatting
  NOP,     // No operation
  HALT     // Stop execution
};

struct Instruction
{
  OpCode op;
  std::int8_t arg;
  std::int32_t LineNumber;  // For debugging

  Instruction(OpCode o, std::int32_t a = 0, std::int32_t ln = 0) :
      op(o),
      arg(static_cast<std::int8_t>(a)),
      LineNumber(static_cast<std::int32_t>(ln))
  {
  }
};

}
}
}