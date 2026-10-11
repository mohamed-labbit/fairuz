//
// opcode.cc
//

#include "Compiler/opcode.hpp"
#include "Interpreter/gc.hpp"
#include "Interpreter/object.hpp"
#include "Interpreter/value.hpp"
#include "Support/array.hpp"

#include <cstdint>
#include <iostream>

namespace fairuz::runtime {

Value ObjBigInt::add(Value const& a, Value const& b, GarbageCollector& gc) { return integer::add(a, b, gc); }
Value ObjBigInt::sub(Value const& a, Value const& b, GarbageCollector& gc) { return integer::sub(a, b, gc); }
Value ObjBigInt::mul(Value const& a, Value const& b, GarbageCollector& gc) { return integer::mul(a, b, gc); }
Value ObjBigInt::div(Value const& a, Value const& b, GarbageCollector& gc) { return integer::div(a, b, gc); }

} // namespace fairuz::runtime
