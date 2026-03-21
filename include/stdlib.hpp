#ifndef STDLIB_HPP
#define STDLIB_HPP

#include "value.hpp"

namespace mylang::runtime {

Value nativeLen(int argc, Value *argv);
Value nativePrint(int argc, Value *argv);
Value nativeType(int argc, Value *argv);
Value nativeInt(int argc, Value *argv);
Value nativeFloat(int argc, Value *argv);
Value nativeAppend(int argc, Value *argv);
Value nativePop(int argc, Value *argv);
Value nativeSlice(int argc, Value *argv);
Value nativeInput(int argc, Value *argv);
Value nativeStr(int argc, Value *argv);
Value nativeBool(int argc, Value *argv);
Value nativeList(int argc, Value *argv);
Value nativeSplit(int argc, Value *argv);
Value nativeJoin(int argc, Value *argv);
Value nativeSubstr(int argc, Value *argv);
Value nativeContains(int argc, Value *argv);
Value nativeTrim(int argc, Value *argv);
Value nativeFloor(int argc, Value *argv);
Value nativeCeil(int argc, Value *argv);
Value nativeRound(int argc, Value *argv);
Value nativeAbs(int argc, Value *argv);
Value nativeMin(int argc, Value *argv);
Value nativeMax(int argc, Value *argv);
Value nativePow(int argc, Value *argv);
Value nativeSqrt(int argc, Value *argv);
Value nativeAssert(int argc, Value *argv);
Value nativeClock(int argc, Value *argv);
Value nativeError(int argc, Value *argv);
Value nativeTime(int argc, Value *argv);

} // namespace mylang::runtime

#endif // STDLIB_HPP
