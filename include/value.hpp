#ifndef VALUE_HPP
#define VALUE_HPP

#include "opcode.hpp"

namespace mylang::runtime {

using Value = uint64_t;

static constexpr Value NANBOX_QNAN = UINT64_C(0x7FF8000000000000);
static constexpr Value NANBOX_SIGN_BIT = UINT64_C(0x8000000000000000);
static constexpr Value TAG_INT = UINT64_C(0x7FF9000000000000);
static constexpr Value TAG_OBJ = UINT64_C(0xFFF8000000000000);
static constexpr Value PAYLOAD_MASK = UINT64_C(0x0000FFFFFFFFFFFF);
static constexpr Value NIL_VAL = UINT64_C(0x7FF8000000000001);
static constexpr Value FALSE_VAL = UINT64_C(0x7FF8000000000002);
static constexpr Value TRUE_VAL = UINT64_C(0x7FF8000000000003);
static constexpr uint64_t INT_TAG16 = UINT64_C(0x7FF9);
static constexpr uint64_t OBJ_TAG16 = UINT64_C(0xFFF8);

enum class ObjType : uint8_t { STRING, LIST, FUNCTION, CLOSURE, NATIVE, UPVALUE };

struct ObjHeader {
  ObjType type{ObjType::UPVALUE};
  bool isMarked{false};
  ObjHeader *next{nullptr};

  explicit ObjHeader(ObjType t) noexcept : type(t) {}
};

struct ObjString : ObjHeader {
  StringRef str;
  uint64_t hash;

  explicit ObjString(StringRef s) : ObjHeader(ObjType::STRING), str(s), hash(static_cast<uint64_t>(std::hash<StringRef>{}(s))) {}
};

struct ObjList : ObjHeader {
  Array<Value> elements;

  ObjList() : ObjHeader(ObjType::LIST) {}

  void reserve(uint32_t cap) { elements.reserve(cap); }
  uint32_t size() const { return elements.size(); }
};

struct ObjFunction : ObjHeader {
  unsigned int arity{0};
  unsigned int upvalueCount{0};
  Chunk *chunk{nullptr};
  ObjString *name{nullptr};

  explicit ObjFunction(Chunk *ch = nullptr) : ObjHeader(ObjType::FUNCTION), chunk(ch) {}
};

struct ObjUpvalue : ObjHeader {
  Value *location{nullptr};
  Value closed;
  ObjUpvalue *nextOpen{nullptr};

  explicit ObjUpvalue(Value *slot) noexcept : ObjHeader(ObjType::UPVALUE), location(slot) {}
};

struct ObjClosure : ObjHeader {
  ObjFunction *function{nullptr};
  Array<ObjUpvalue *> upValues;

  explicit ObjClosure(ObjFunction *fn) : ObjHeader(ObjType::CLOSURE), function(fn), upValues(fn->upvalueCount, nullptr) {}
};

using NativeFn = Value (*)(int argc, Value *argv);

struct ObjNative : ObjHeader {
  NativeFn fn;
  ObjString *name{nullptr};
  int arity;

  ObjNative(NativeFn f, ObjString *n, int a) : ObjHeader(ObjType::NATIVE), fn(f), name(n), arity(a) {}
};

#define MAKE_OBJ_STRING(s) getAllocator().allocateObject<ObjString>(s)
#define MAKE_OBJ_LIST() getAllocator().allocateObject<ObjList>()
#define MAKE_OBJ_FUNCTION(ch) getAllocator().allocateObject<ObjFunction>(ch)
#define MAKE_OBJ_UPVALUE(slot) getAllocator().allocateObject<ObjUpvalue>(slot)
#define MAKE_OBJ_CLOSURE(fn) getAllocator().allocateObject<ObjClosure>(fn)
#define MAKE_OBJ_NATIVE(f, n, a) getAllocator().allocateObject<ObjNative>(f, n, a)

#define MAKE_REAL(d)                                                                                                                                 \
  ({                                                                                                                                                 \
    auto _tmp = (d);                                                                                                                                 \
    Value bits;                                                                                                                                      \
    __builtin_memcpy(&bits, &_tmp, sizeof(bits));                                                                                                    \
    bits;                                                                                                                                            \
  })
#define MAKE_OBJECT(p) TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK)
#define MAKE_BOOL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define MAKE_INTEGER(v) (static_cast<Value>(v) & PAYLOAD_MASK) | TAG_INT

#define IS_NIL(v) ((v) == NIL_VAL)
#define IS_BOOL(v) (((v) | 1) == TRUE_VAL)
#define IS_INTEGER(v) (((v) >> 48) == INT_TAG16)
#define IS_OBJECT(v) (((v) >> 48) == OBJ_TAG16)
#define IS_DOUBLE(v)                                                                                                                                 \
  ({                                                                                                                                                 \
    Value _v = (v);                                                                                                                                  \
    uint64_t _top = _v >> 48;                                                                                                                        \
    (_top != INT_TAG16 && _top != OBJ_TAG16 && !IS_BOOL(_v) && !IS_NIL(_v));                                                                         \
  })
#define IS_NUMBER(v)                                                                                                                                 \
  ({                                                                                                                                                 \
    Value _v = (v);                                                                                                                                  \
    (IS_INTEGER(_v) || IS_DOUBLE(_v));                                                                                                               \
  })

#define IS_STRING(v) (IS_OBJECT(v) && reinterpret_cast<ObjHeader *>((v) & PAYLOAD_MASK)->type == ObjType::STRING)
#define IS_LIST(v) (IS_OBJECT(v) && reinterpret_cast<ObjHeader *>((v) & PAYLOAD_MASK)->type == ObjType::LIST)
#define IS_FUNCTION(v) (IS_OBJECT(v) && reinterpret_cast<ObjHeader *>((v) & PAYLOAD_MASK)->type == ObjType::FUNCTION)
#define IS_CLOSURE(v) (IS_OBJECT(v) && reinterpret_cast<ObjHeader *>((v) & PAYLOAD_MASK)->type == ObjType::CLOSURE)
#define IS_NATIVE(v) (IS_OBJECT(v) && reinterpret_cast<ObjHeader *>((v) & PAYLOAD_MASK)->type == ObjType::NATIVE)
#define IS_TRUTHY(v)                                                                                                                                 \
  ({                                                                                                                                                 \
    Value _v = (v);                                                                                                                                  \
    uint64_t _tag = _v >> 48;                                                                                                                        \
    bool _res;                                                                                                                                       \
                                                                                                                                                     \
    if (__builtin_expect(IS_NIL(_v), 0))                                                                                                             \
      _res = false;                                                                                                                                  \
    else if (__builtin_expect((_v | 1) == TRUE_VAL, 0))                                                                                              \
      _res = (_v & 1);                                                                                                                               \
    else if (__builtin_expect(_tag == INT_TAG16, 1))                                                                                                 \
      _res = ((_v & PAYLOAD_MASK) != 0);                                                                                                             \
    else if (__builtin_expect(_tag == OBJ_TAG16, 0))                                                                                                 \
      _res = true;                                                                                                                                   \
    else                                                                                                                                             \
      _res = __builtin_expect((_v << 1) != 0, 1);                                                                                                    \
                                                                                                                                                     \
    _res;                                                                                                                                            \
  })

#define AS_BOOL(v) ((v) & 1)
#define AS_INTEGER(v)                                                                                                                                \
  ({                                                                                                                                                 \
    int64_t _payload = (int64_t)((v) & PAYLOAD_MASK);                                                                                                \
    if (_payload & (INT64_C(1) << 47))                                                                                                               \
      _payload |= ~PAYLOAD_MASK;                                                                                                                     \
    _payload;                                                                                                                                        \
  })

#define AS_DOUBLE(v)                                                                                                                                 \
  ({                                                                                                                                                 \
    Value _v = (v);                                                                                                                                  \
    double _d;                                                                                                                                       \
    __builtin_memcpy(&_d, &_v, sizeof(_d));                                                                                                          \
    _d;                                                                                                                                              \
  })

#define AS_DOUBLE_ANY(v)                                                                                                                             \
  ({                                                                                                                                                 \
    Value _v = (v);                                                                                                                                  \
    double _d;                                                                                                                                       \
    if (IS_INTEGER(_v))                                                                                                                              \
      _d = (double)AS_INTEGER(_v);                                                                                                                   \
    else                                                                                                                                             \
      __builtin_memcpy(&_d, &_v, sizeof(_d));                                                                                                        \
    _d;                                                                                                                                              \
  })

#define AS_OBJECT(v) reinterpret_cast<ObjHeader *>(static_cast<uintptr_t>((v) & PAYLOAD_MASK))
#define AS_STRING(v) static_cast<ObjString *>(AS_OBJECT(v))
#define AS_LIST(v) static_cast<ObjList *>(AS_OBJECT(v))
#define AS_FUNCTION(v) static_cast<ObjFunction *>(AS_OBJECT(v))
#define AS_CLOSURE(v) static_cast<ObjClosure *>(AS_OBJECT(v))
#define AS_NATIVE(v) static_cast<ObjNative *>(AS_OBJECT(v))

enum class TypeTag : uint8_t {
  NONE = 0,
  NIL = 1 << 0,
  BOOL = 1 << 1,
  INT = 1 << 2,
  DOUBLE = 1 << 3,
  STRING = 1 << 4,
  LIST = 1 << 5,
  CLOSURE = 1 << 6,
  NATIVE = 1 << 7
};

[[nodiscard]] inline bool hasTag(TypeTag mask, TypeTag t) noexcept { return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(t)) != 0; }

[[nodiscard]] inline TypeTag operator|(TypeTag a, TypeTag b) noexcept {
  return static_cast<TypeTag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline TypeTag &operator|=(TypeTag &a, TypeTag b) noexcept { return a = a | b; }

[[nodiscard]] inline TypeTag valueTypeTag(Value v) noexcept {
  if (IS_NIL(v))
    return TypeTag::NIL;
  if (IS_BOOL(v))
    return TypeTag::BOOL;
  if (IS_INTEGER(v))
    return TypeTag::INT;

  if (IS_OBJECT(v)) {
    switch (AS_OBJECT(v)->type) {
    case ObjType::STRING:
      return TypeTag::STRING;
    case ObjType::LIST:
      return TypeTag::LIST;
    case ObjType::CLOSURE:
      return TypeTag::CLOSURE;
    case ObjType::NATIVE:
      return TypeTag::NATIVE;
    default:
      return TypeTag::NONE;
    }
  }

  return TypeTag::DOUBLE;
}

} // namespace mylang::runtime

#endif // VALUE_HPP
