# Fairuz integer runtime contract

Date: 2026-09-25  
Status: design contract from the integer implementation discussion; not an audit of the repository.

## 1. Purpose and scope

Fairuz exposes one integer abstraction whose supported operations behave consistently across small and large values. Storage is an implementation detail: an integer can occupy the small representation or a garbage-collected arbitrary-precision object.

Integer arithmetic must preserve exact mathematical results, subject to the operation's language semantics and available resources. Exceeding the small representation triggers promotion, not silent wrapping, floating-point conversion, or C++ undefined behavior.

This document specifies creation, representation, promotion, operation dispatch, and optimization boundaries. It does not settle division rounding, negative shift-count behavior, signed right-shift semantics, or bitwise semantics for negative integers. Those language decisions must be specified consistently across both representations before implementing the affected operations.

“Must” identifies a correctness requirement. “Should” identifies an implementation recommendation. Proposed helper names describe responsibilities rather than existing repository APIs.

## 2. Representation and invariants

### Small integers

Define `SMALL_MIN` and `SMALL_MAX` from the actual `Value` encoding. Do not assume that a 64-bit host implies 64 available signed payload bits: tags may reduce the range.

- Every stored small integer must lie within `[SMALL_MIN, SMALL_MAX]`.
- Extracting a small integer into the arithmetic carrier must preserve its value.
- The examples below assume the carrier is signed 64-bit `i64` and that the small range is a subset of its range.
- A successful machine-overflow check does not, by itself, prove that a result fits a narrower small payload.

### Big integers

The discussed representation uses a separate sign and an array of full-width `u32` magnitude limbs, least significant first:

```text
magnitude = limbs[0] + limbs[1] * 2^32 + limbs[2] * 2^64 + ...
```

Each limb is in `[0, 2^32 - 1]`. The base is `2^32`, not `32`.

This document uses `negative == true` to mean a negative value. The meaning of the existing `sign` field has not been confirmed; adapt the names and mapping to the implementation rather than silently changing that field's meaning.

Published integer objects must satisfy these invariants:

- Nonzero magnitudes have no unused zero limbs at the most significant end.
- Zero has no negative sign and follows one documented canonical representation.
- Limb counts and allocation capacities use the array's size type without narrowing.
- Capacity calculations and byte-size calculations must not wrap.
- Arithmetic does not mutate operand objects through borrowed limb references.
- Temporary buffers may be unnormalized while computing; results are normalized before publication.

## 3. Creation contract

Every path that introduces an integer must establish a valid representation. This includes literal loading, interpreter/native entry points, conversions, and expression results.

Establishing validity does not require repeating a dynamic range check when it is already known.

| Creation path | Required behavior |
| --- | --- |
| Integer literal | Classify during parsing/loading or use a checked runtime constructor. Parsing must not overflow a fixed-width accumulator. |
| Known small constant | Construct directly under a documented range guarantee. |
| Copy of an existing integer value | Preserve the established representation; no new promotion test is needed merely to copy it. |
| Incoming native integer | Check payload representability unless guaranteed by the caller's contract. |
| Conversion from another type | Apply the conversion's semantic rules and establish range without an invalid intermediate conversion. |
| Arithmetic result | Use an operation-specific checked path or a proven-safe unchecked path. |
| Limb result | Normalize sign and magnitude, then apply the chosen demotion policy. |

A low-level function such as `Value::from_int` should have an explicit contract: either it checks representation capacity or its caller must prove that the value fits. Do not let different callers assume different guarantees.

Literal constants may be classified once and reused. The contract does not require reparsing or rechecking a literal on every evaluation.

## 4. Uniform operation API and internal dispatch

All integers expose the same language-level addition, subtraction, multiplication, shift, comparison, and other supported integer operations. The implementation selects a path based on operand representations and operation semantics.

| Operands | Addition/subtraction path |
| --- | --- |
| Small + small | Checked native arithmetic; return a small value if it fits, otherwise construct the exact big result. |
| Small + big | Present the small operand as a sign and magnitude, then run signed magnitude arithmetic. |
| Big + small | Symmetric dispatch, preserving operand order for subtraction. |
| Big + big | Run signed magnitude arithmetic over limb arrays. |

Mixed operations must never call `as_big_int()` on a small value. A small operand can be represented by a temporary magnitude view with at most two `u32` limbs for an `i64` carrier; allocating a temporary garbage-collected integer is not inherently necessary.

Views must retain valid backing storage for their entire use. Any allocation that can trigger collection must obey the collector's rooting and pointer-stability rules. Inputs must remain reachable, and borrowed pointers must not be assumed stable across a moving collection.

## 5. Checked small-integer fast path

Use compiler overflow builtins where supported, with a defined fallback for other toolchains. Conceptually:

```cpp
// Illustrative helper names; not a drop-in repository implementation.
i64 result;
if (!__builtin_add_overflow(a, b, &result)) {
    if (fits_small(result))
        return Value::from_int(result);

    return make_integer_from_i64(result, gc);
}

return add_from_magnitudes(a, b, gc);
```

The first branch distinguishes machine representability from small-payload representability. If the payload covers all of `i64`, `fits_small` is redundant and should compile away.

On reported machine overflow, do not treat the output variable as the exact mathematical sum. The fallback must obtain the full result from the original operands, using limb arithmetic or another suitably wide, defined computation.

The equivalent operation-specific builtins can serve subtraction and multiplication. Do not evaluate an ordinary overflowing signed C++ expression first and attempt to detect overflow afterward.

### Performance contract

- Keep the common small/small path short and allocation-free when the result fits.
- Keep promotion and limb arithmetic off that common path where practical.
- Overflow builtins express checked arithmetic to the compiler. They are not branch-prediction directives.
- Hardware predicts branches independently. An uncommon overflow path can be predictable without making its check free.
- No fixed slowdown percentage is assumed. Measure representative interpreter workloads and arithmetic-heavy loops.

On suitable targets, checked arithmetic can use the arithmetic instruction's overflow flags followed by a conditional branch. Exact code generation depends on compiler, target, and optimization settings; it is not part of the language contract.

## 6. Magnitude arithmetic and signed dispatch

### Addition of magnitudes

For base `2^32`, the core loop is:

```cpp
u64 carry = 0;
for (size_t i = 0; i < n; ++i) {
    u64 const a = i < lhs.size() ? lhs[i] : u64{0};
    u64 const b = i < rhs.size() ? rhs[i] : u64{0};
    u64 const total = a + b + carry;
    result.push(static_cast<u32>(total));
    carry = total >> 32;
}
if (carry != 0)
    result.push(static_cast<u32>(carry));
```

Here `n` is the larger input length and capacity permits up to `n + 1` limbs, with allocation arithmetic checked before use. Operands become `u64` before addition. The maximum intermediate is `2^33 - 1`, which fits in `u64`; the carry is zero or one. Missing limbs are read as zero without padding or mutating inputs.

### Signed addition

1. If the signs match, add the magnitudes and retain that sign.
2. If the signs differ, compare magnitudes.
3. If magnitudes match, return canonical zero.
4. Otherwise subtract the smaller magnitude from the larger and retain the sign of the operand with the larger magnitude.

A magnitude-subtraction helper requires `larger >= smaller`, propagates borrow, and removes high zero limbs from the result. Calling ordinary signed subtraction on the original operands is not a substitute for this helper.

Signed subtraction follows addition with the right operand's mathematical sign reversed. Implement that reversal in sign/magnitude metadata when necessary: native negation of the minimum signed carrier value is not safe.

### Converting the minimum signed carrier value

Do not compute signed `-value` or a signed absolute value when `value` may be `INT64_MIN`. A safe `i64`-to-magnitude conversion is:

```cpp
bool const negative = value < 0;
u64 const bits = static_cast<u64>(value);
u64 const magnitude = negative ? u64{0} - bits : bits;
u32 const low = static_cast<u32>(magnitude);
u32 const high = static_cast<u32>(magnitude >> 32);
```

The unsigned conversion and subtraction are defined. Omit unnecessary high zero limbs when forming the normalized view.

## 7. Operation-specific range obligations

There is no single generic promotion test that safely implements every operation.

| Operation | Obligation |
| --- | --- |
| Addition/subtraction | Check machine overflow and small-payload range, or use a valid proof that both checks are unnecessary. |
| Multiplication | Check the product before packing; allocate enough limbs for the exact fallback. |
| Unary negation | Handle the minimum negative value without native signed overflow; check the resulting payload range. |
| Left shift | Validate the count and compute the resulting size safely before any native shift. Promote when required. |
| Right shift | Apply the language's signed rounding rule; handle large counts without executing an invalid native shift. |
| Comparison | Compare mathematical values across representations; the boolean result needs no integer promotion check. |
| Division/remainder | Handle zero divisors and the signed-carrier minimum divided by `-1` before native division/remainder; apply the selected language rounding rule. |
| Bitwise operations | Implement the selected signed semantics consistently across both representations. |

For native shifts, a count greater than or equal to the carrier width is invalid in C++. Other validity rules also depend on signedness and language version. Neither a later result check nor an addition-overflow builtin makes an invalid shift safe.

## 8. Result construction, normalization, and demotion

Centralize publication of limb results in a helper with responsibilities equivalent to:

```text
finish_integer(magnitude, negative, gc)
```

It must normalize the magnitude, canonicalize zero, enforce storage invariants, and produce a valid integer value. Allocation or resource failure must use the runtime's defined failure mechanism; it must not silently wrap, truncate, return a successful `nil`, or approximate the result.

**Demotion remains an explicit design decision.** This contract recommends returning a small integer whenever a completed result fits the small range. Retaining a big representation for a small magnitude is also semantically valid if all operations support it consistently. Select and document one policy rather than making unrelated arithmetic functions decide independently.

If demotion is enabled, fitting a negative magnitude requires care: a magnitude of `2^63` represents `INT64_MIN` with a negative sign, although that magnitude cannot itself be represented as a positive `i64`.

## 9. Future range-based optimization

The initial implementation keeps required runtime checks. A later optimizer may remove them only when a sound proof or a runtime guard establishes the relevant preconditions.

For a small addition fast path, the proof must establish:

1. Operand types and representations are compatible with the specialized path.
2. The native arithmetic cannot overflow its carrier.
3. The mathematical result fits the small payload.

Other operations need their own preconditions, such as valid shift counts and division guards. Loop analysis must account for every iteration and intermediate value, not only the final result.

The execution engine must be able to act on proofs. A specialized AST node, bytecode instruction, or generated-code path can omit checks; attaching a proof while still calling an always-checked generic helper will not eliminate that overhead.

Internal unchecked operations must have explicit preconditions and must not be used by ordinary unproven callers. Runtime-guarded specialization must fall back before an unsafe operation and preserve evaluation order and observable side effects. It must not replay already executed expression side effects when recovering.

The observation that a workload has never overflowed is not a proof that a check can be removed. Profiling may guide specialization, but speculative assumptions need guards.

## 10. Acceptance criteria

The implementation should verify these boundaries with exact expected results or a trusted arbitrary-precision oracle:

- Small results at, below, and above both payload boundaries.
- Signed-carrier overflow and underflow in small/small operations.
- Both mixed operand orders and all sign combinations.
- Carry propagation across several `0xFFFFFFFF` limbs.
- Borrow propagation across zero limbs and cancellation to zero.
- Minimum signed-carrier conversion and negation.
- Shift counts around the native width and supported resource limits.
- Operand immutability, including adding a value to itself.
- Equivalent behavior between checked and proven-safe specialized paths.
- Normalization and the selected demotion policy.

Performance measurements should separately capture ordinary small arithmetic, interpreter dispatch overhead, and promotion-heavy cases. Compare before and after optimization without weakening integer semantics.

## 11. Integration decisions still to resolve

| Decision | Required resolution |
| --- | --- |
| Small payload width | Derive `SMALL_MIN` and `SMALL_MAX` from the actual `Value` layout. |
| Existing sign flag | Confirm whether `true` means positive or negative and map helpers consistently. |
| Low-level constructor | Document whether `Value::from_int` checks range or requires proven input. |
| Demotion | Choose eager demotion or a documented policy for retaining big values. |
| Zero storage | Choose the canonical representation and enforce it at publication. |
| Signed division, shifts, and bitwise operations | Specify language semantics before finalizing their implementations. |
| GC and failures | Connect temporary rooting, pointer lifetime, and resource failures to the existing runtime conventions. |

These decisions do not change the central contract: one integer API, exact arithmetic, compact common-case storage, operation-specific promotion, and checks removed only under valid guarantees.


## 12. Implemented integration decisions

- The current NaN-boxed `Value` has a signed 48-bit payload: `SMALL_MIN = -2^47` and `SMALL_MAX = 2^47 - 1`.
- `ObjBigInt::sign == true` remains positive. Limbs are owned `std::vector<u32>` storage (least significant first), with `size_t` counts. Canonical magnitude zero is an empty vector with positive sign; ordinary published zero eagerly demotes to small zero.
- All completed arithmetic results eagerly demote when they fit the small payload. The one-argument `Value::from_int` is an internal packer requiring proven payload representability, asserted in debug builds. Native inputs use its checked GC overload. `as_int()` checks signed 64-bit representability for native APIs; integer arithmetic never narrows through that API.
- `/` preserves existing language behavior: divisible integer operands produce an exact integer; a nonintegral quotient produces a float. `%` uses truncation toward zero, with the remainder having the dividend's sign. Zero divisors are errors.
- Negative shifts are errors. Arbitrarily large nonnegative right shifts saturate to zero or negative one. Right shift is arithmetic (floor division by a power of two); this replaces the old negative-input logical shift that returned a float. Bitwise operations use infinite two's-complement semantics. Left shift remains exact and promotes.
- Integer exponentiation with a nonnegative integer exponent is exact. Negative exponents retain floating-point semantics. Finite float-to-integer conversion truncates toward zero and can promote beyond 64 bits; nonfinite input is an error. Floor, ceil, and round use the same checked publication path.
- Integer comparisons are exact, including comparison with floats and dictionary-key equality. Native APIs with fixed-width parameters reject values they cannot represent instead of truncating them.
- Collection is nonmoving and occurs at VM dispatch boundaries, never inside integer helper allocation. Borrowed operand views remain valid through each operation, and results are stored in a VM root before the next collection. Published limb capacity is included in GC memory accounting. Limbs use owned heap storage so collection releases them independently of the parse arena.
- The implementation has a resource ceiling of 2^19 limbs (2^24 bits / 2 MiB magnitude storage) per integer buffer. Capacity sums are checked before allocation, and requests exceeding the ceiling fail through the allocation diagnostic. Operations may require a carry/scratch limb in addition to the final normalized magnitude. Native allocation failures follow the runtime failure path; they never return a successful nil or an approximate integer.
- Literal magnitudes are parsed without fixed-width overflow, stored in the chunk, and reused by load instructions. All ordinary arithmetic opcodes retain checks; there are no unchecked integer specializations.
