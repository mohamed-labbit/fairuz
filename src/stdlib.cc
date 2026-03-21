/// stdlib.cc

#include "../include/stdlib.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace mylang::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

Value nativeLen(int argc, Value *argv) {
  if (argc == 0 || !argv)
    return NIL_VAL;

  if (argc == 1) {
    if (IS_STRING(argv[0]))
      return MAKE_INTEGER(AS_STRING(argv[0])->str.len());
    if (IS_LIST(argv[0]))
      return MAKE_INTEGER(AS_LIST(argv[0])->elements.size());
  }

  /// do not accept multiple args for len
  return NIL_VAL;
}

static void printValue(Value v, int depth = 0) {
  if (IS_NIL(v)) {
    std::cout << "nil";
    return;
  }

  if (IS_BOOL(v)) {
    std::cout << (AS_BOOL(v) ? "true" : "false");
    return;
  }

  if (IS_INTEGER(v)) {
    std::cout << AS_INTEGER(v);
    return;
  }

  if (IS_OBJECT(v)) {
    ObjHeader *obj = AS_OBJECT(v);

    switch (obj->type) {
    case ObjType::STRING:
      std::cout << static_cast<ObjString *>(obj)->str;
      return;

    case ObjType::LIST: {
      ObjList *list = static_cast<ObjList *>(obj);
      std::cout << '[';
      for (uint32_t i = 0; i < list->elements.size(); ++i) {
        if (i > 0)
          std::cout << ", ";
        Value elem = list->elements[i];
        if (IS_OBJECT(elem) && AS_OBJECT(elem)->type == ObjType::STRING) {
          std::cout << '"';
          std::cout << AS_STRING((elem))->str;
          std::cout << '"';
        } else {
          printValue(elem, depth + 1);
        }
      }
      std::cout << ']';
      return;
    }

    case ObjType::CLOSURE: {
      ObjClosure *cl = static_cast<ObjClosure *>(obj);
      std::cout << "<function ";
      if (cl->function && cl->function->name)
        std::cout << cl->function->name->str;
      else
        std::cout << "?";
      std::cout << '>';
      return;
    }

    case ObjType::NATIVE: {
      ObjNative *nat = static_cast<ObjNative *>(obj);
      std::cout << "<native ";
      if (nat->name)
        std::cout << nat->name->str;
      else
        std::cout << "?";
      std::cout << '>';
      return;
    }

    case ObjType::FUNCTION: {
      ObjFunction *fn = static_cast<ObjFunction *>(obj);
      std::cout << "<function ";
      if (fn->name)
        std::cout << fn->name->str;
      else
        std::cout << "?";
      std::cout << '>';
      return;
    }

    case ObjType::UPVALUE:
      std::cout << "<upvalue>";
      return;
    }
  }

  double d = AS_DOUBLE(v);
  if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15) {
    std::cout << static_cast<int64_t>(d);
  } else {
    std::ostringstream oss;
    oss << std::setprecision(14) << std::noshowpoint << d;
    std::cout << oss.str();
  }
}

Value nativePrint(int argc, Value *argv) {
  if (argc == 0 || !argv) {
    std::cout << '\n';
    return NIL_VAL;
  }

  for (int i = 0; i < argc; ++i) {
    if (i > 0)
      std::cout << '\t';
    printValue(argv[i]);
  }
  std::cout << '\n';
  return NIL_VAL;
}

Value nativeType(int argc, Value *argv) {
  if (argc != 1 || !argv)
    return NIL_VAL;

  return MAKE_INTEGER(static_cast<int64_t>(valueTypeTag(argv[0])));
}

Value nativeInt(int argc, Value *argv) {
  if (argc != 1 || !argv)
    return NIL_VAL;

  if (IS_INTEGER(argv[0]))
    return MAKE_INTEGER(AS_DOUBLE(static_cast<int64_t>(argv[0])));

  return NIL_VAL;
}

Value nativeFloat(int argc, Value *argv) {
  if (argc != 1 || !argv)
    return NIL_VAL;

  if (IS_INTEGER(argv[0]))
    return MAKE_REAL(AS_DOUBLE(argv[0]));

  return NIL_VAL;
}

Value nativeAppend(int argc, Value *argv) {
  if (argc < 2 || !argv) {
    diagnostic::emit("append() : expected two arguments got : " + std::to_string(argc));
    diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
    return NIL_VAL;
  }

  Value &list_v = argv[0];
  if (!IS_LIST(list_v)) {
    diagnostic::emit("append() called on a non list value");
    diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
    return NIL_VAL;
  }

  ObjList *list_obj = AS_LIST(list_v);

  for (int i = 1; i < argc; ++i)
    list_obj->elements.push(argv[i]);

  return NIL_VAL;
}

Value nativePop(int argc, Value *argv) {
  if (argc != 1 || !argv) {
    diagnostic::emit("pop() called with no value attatched");
    diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
    return NIL_VAL;
  }

  Value &list_v = argv[0];
  if (!IS_LIST(list_v)) {
    diagnostic::emit("pop() called on a non list value");
    diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
    return NIL_VAL;
  }

  AS_LIST(list_v)->elements.pop();
  return NIL_VAL;
}

Value nativeSlice(int argc, Value *argv) {
  /// cut a copy of a list, with inclusive indices
  /// accept [list, a, b]
  /// a, b are the indices
  /// if b is null then cut [a:]

  if (argc < 2) {
    diagnostic::emit("slice() expects at least 2 arguments, got : " + std::to_string(argc));
    diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
    return NIL_VAL;
  }

  ObjList *list_obj = AS_LIST(argv[0]);
  Value ret = MAKE_OBJECT(MAKE_OBJ_LIST());
  ObjList *ret_list = AS_LIST(ret);
  /// Expects indices to be ints
  uint32_t a = AS_INTEGER(argv[1]);
  uint32_t b = argc == 3 ? AS_INTEGER(argv[2]) : list_obj->size() - 1;

  for (int i = a; i <= b; ++i)
    ret_list->elements.push(list_obj->elements[i]);

  return ret;
}

Value nativeInput(int argc, Value *argv) { return NIL_VAL; }

Value nativeStr(int argc, Value *argv) {
  if (argc > 1) {
    diagnostic::emit("str() is called with no arguments");
    diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
    return NIL_VAL;
  }

  StringRef output = "";

  if (argc == 0 || !argv)
    return MAKE_OBJECT((MAKE_OBJ_STRING(output)));

  /// TODO : stringify a list

  if (IS_STRING(argv[0]))
    output = AS_STRING(argv[0])->str;
  else if (IS_BOOL(argv[0]))
    output = AS_BOOL(argv[0]) ? "true" : "false";
  else if (IS_DOUBLE(argv[0]))
    output = std::to_string(AS_DOUBLE(argv[0])).data();
  else if (IS_INTEGER(argv[0]))
    output = std::to_string(AS_INTEGER(argv[0])).data();

  return MAKE_OBJECT(MAKE_OBJ_STRING(output));
}

Value nativeBool(int argc, Value *argv) {
  if (argc != 1 || !argv) {
    diagnostic::emit("bool() is called with no arguments");
    diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
    return NIL_VAL;
  }

  return IS_TRUTHY(argv[0]) ? MAKE_BOOL(true) : MAKE_BOOL(false);
}

Value nativeList(int argc, Value *argv) {
  Value ret = MAKE_OBJECT(MAKE_OBJ_LIST());
  ObjList *list_obj = AS_LIST(ret);

  for (int i = 0; i < argc;)
    list_obj->elements.push(argv[i++]);

  return ret;
}

Value nativeSplit(int argc, Value *argv) { return NIL_VAL; }

Value nativeJoin(int argc, Value *argv) { return NIL_VAL; }

Value nativeSubstr(int argc, Value *argv) { return NIL_VAL; }

Value nativeContains(int argc, Value *argv) { return NIL_VAL; }

Value nativeTrim(int argc, Value *argv) { return NIL_VAL; }

Value nativeFloor(int argc, Value *argv) {
  if (argc != 1 || !argv) {
    diagnostic::emit("floor() expects 1 argument, got : " + std::to_string(argc));
    diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
    return NIL_VAL;
  }

  if (!IS_INTEGER(argv[0])) {
    diagnostic::emit("floor() is called with a non numeric value argument");
    // diagnostic::runtimeError(ErrorCode::);
    /// diagnostic::runtimeError(ErrorCode::);
    return NIL_VAL;
  }

  if (IS_INTEGER(argv[0]))
    return argv[0];

  return MAKE_REAL(std::floorf(AS_DOUBLE(argv[0])));
}

Value nativeCeil(int argc, Value *argv) {
  if (argc != 1 || !argv) {
    diagnostic::emit("ceil() expects 1 argument, got : " + std::to_string(argc));
    return NIL_VAL;
  }

  if (!IS_INTEGER(argv[0])) {
    diagnostic::emit("ceil() is called with a non numeric value argument");
    return NIL_VAL;
  }

  if (IS_INTEGER(argv[0]))
    return argv[0];

  return MAKE_REAL(std::ceilf(AS_DOUBLE(argv[0])));
}

Value nativeRound(int argc, Value *argv) { return NIL_VAL; }

Value nativeAbs(int argc, Value *argv) {
  if (argc != 1 || !argv) {
    diagnostic::emit("abs() expects 1 argument, got : " + std::to_string(argc));
    return NIL_VAL;
  }

  if (!IS_INTEGER(argv[0])) {
    diagnostic::emit("abs() is called with a non numeric value argument");
    return NIL_VAL;
  }

  if (IS_INTEGER(argv[0]))
    return MAKE_INTEGER(std::abs(AS_INTEGER(argv[0])));

  return MAKE_REAL(std::fabs(AS_DOUBLE(argv[0])));
}

Value nativeMin(int argc, Value *argv) {
  if (argc < 2 || !argv) {
    diagnostic::emit("ceil() expects 2 or more arguments, got : " + std::to_string(argc));
    return NIL_VAL;
  }

  if (!IS_INTEGER(argv[0])) {
    diagnostic::emit("ceil() is called with a non numeric value argument");
    return NIL_VAL;
  }

  Value ret = MAKE_REAL(AS_DOUBLE(argv[0]));
  bool all_ints = true;

  for (int i = 1; i < argc; ++i) {
    if (!IS_INTEGER(argv[i]))
      all_ints = false;

    ret = MAKE_REAL(std::fmin(AS_DOUBLE(ret), AS_DOUBLE(argv[i])));
  }

  if (all_ints)
    return MAKE_INTEGER(static_cast<int64_t>(AS_DOUBLE(ret)));

  return ret;
}

Value nativeMax(int argc, Value *argv) {
  if (argc < 2 || !argv) {
    diagnostic::emit("ceil() expects 2 or more arguments, got : " + std::to_string(argc));
    return NIL_VAL;
  }

  if (!IS_INTEGER(argv[0])) {
    diagnostic::emit("ceil() is called with a non numeric value argument");
    return NIL_VAL;
  }

  Value ret = MAKE_REAL(AS_DOUBLE(argv[0]));
  bool all_ints = true;

  for (int i = 1; i < argc; ++i) {
    if (!IS_INTEGER(argv[i]))
      all_ints = false;

    ret = MAKE_REAL(std::fmax(AS_DOUBLE(ret), AS_DOUBLE(argv[i])));
  }

  if (all_ints)
    return MAKE_INTEGER(static_cast<int64_t>(AS_DOUBLE(ret)));

  return ret;
}

Value nativePow(int argc, Value *argv) { return NIL_VAL; }

Value nativeSqrt(int argc, Value *argv) { return NIL_VAL; }

Value nativeAssert(int argc, Value *argv) { return NIL_VAL; }

Value nativeClock(int argc, Value *argv) { return NIL_VAL; }

Value nativeError(int argc, Value *argv) { return NIL_VAL; }

Value nativeTime(int argc, Value *argv) { return NIL_VAL; }

} // namespace mylang::runtime
