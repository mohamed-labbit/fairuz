# Compiler refactoring design

Date: 2026-10-19  
Source baseline inspected: main checkout at commit `26335d70`, including its uncommitted compiler and typed-operand changes.

## 1. Purpose and reading order

This document turns the compiler-refactoring discussion into an incremental design. The immediate next step is extracting call-register bookkeeping. Later steps separate assignment targets, class planning, deferred compilation context, and statement control flow.

The source has already changed since the beginning of that discussion. Existing allocator and emitter work is a foundation to preserve, not a proposal to implement again. The status below describes a source inspection, not a passing build or test result.

No tests, builds, compiler executions, or parser probes were run to prepare this document. The verification section is a plan for a future authorized validation step.

Read sections 2–4 for the boundaries and terminology, section 5 for the detailed call design, and sections 6–11 for subsequent work. Sections 12–14 define migration order, completion criteria, and an eventual file layout.

All proposed C++ interfaces are illustrative. Names and error-propagation syntax can be adapted to the repository; the contracts are the design.

### 1.1 Current implementation status

| Area | Observed state | Remaining work |
|---|---|---|
| Register allocation | `CompilerState` owns a `RegisterAllocator`; duplicate cursors were removed | Preserve its contract; add range/checkpoint operations only when a concrete consumer needs them |
| Peak register count | Frame size uses `state.registers.max_reg()` | Keep counts distinct from register identities during typed-operand work |
| Expression scratch cleanup | `RegMark` and expression/call paths use allocator rewinds | Centralize call-specific arithmetic in `CallRegisters` |
| Bytecode emission | `ByteCodeEmitter` emits instructions, payloads, loads, and patches jumps | Tighten operand contracts and eventually remove its dependency on the whole compiler |
| Payload formats | Cache and name payloads have separate emission operations | Keep that distinction private to the emitter where possible |
| Error propagation | Statement compilation uses `ErrorOr<void, RuntimeError>`; bind/set/discard helpers exist | Consolidate overlapping macros and make declaration behavior unmistakable |
| Typed operands | `RegType`, `PCType`, `IndexType`, `InstrType`, and `SlotIndex` wrappers are being introduced | Finish wrapper semantics and distinguish unrelated operand domains where useful |
| Call preparation | Local lambdas handle arguments and receivers | Extract the register layout and cleanup into one helper |
| Assignment targets | `compile_assign_impl()` branches on target shape and repeats capture/read/write work | Introduce a prepared target |
| Class construction | One large routine plans layouts, registers methods, and emits descriptors | Separate those phases and retain function indices directly |
| Deferred function compilation | Captures a compiler-state snapshot and metadata tables | Replace the broad capture with a named definition context; retain transactional compilation |
| Statement flow | Mutable `is_dead` is saved/reset across branches and loops | Return an explicit statement-flow result |
| File organization | Expressions and statements contain several responsibilities each | Split files after the corresponding responsibilities have stable interfaces |

### 1.2 Compatibility requirements

These are implementation changes. Preserve:

- Existing language scoping: a local binding's register stops belonging to it when its lexical scope ends.
- Existing evaluation order and evaluation counts.
- Existing callable-field and module-member behavior.
- The current bytecode calling conventions during the first extraction.
- Special method slot ordering, inherited field ordering, and descriptor construction rules.
- Lazy body compilation and the lifetime of AST/source data needed by deferred compilation.
- Existing error codes and source attribution, unless a separate behavior change is explicitly chosen.
- `ExprResult` constants and relocatable destinations.
- Transactional deferred compilation: failed compilation must not replace a usable deferred chunk with partial bytecode.

A new register allocation scheme, register packing optimization, bytecode format, type system, or intermediate representation is not required to achieve this design.

## 2. Responsibilities and ownership

The intended division is:

| Component | Owns or decides | Must not decide |
|---|---|---|
| `Compiler` / function compiler | AST traversal, binding rules, expression evaluation order, call selection | Instruction bit packing |
| `CompilerState` | One function's locals, loop contexts, active class context, allocator | Runtime call-frame storage |
| `RegisterAllocator` | Available register position, checked reservation, peak usage | Variable names, method lookup, AST traversal |
| `RegMark` | A compiler scratch checkpoint and its existing local-preservation rule | Fairuz lexical-scope semantics |
| `ScopeGuard` | Lexical depth and removal of locals leaving that scope | Raw allocator cursor mutation |
| `CallRegisters` — proposed | Layout and compiler lifetime of one call's register range | AST compilation, selecting a method, executing a call |
| `ByteCodeEmitter` | Encodings, payload words, instruction destination patching, jump offsets | Symbol lookup or register lifetime policy |
| `PreparedTarget` — proposed | Captured operands describing one assignment location | Independently re-evaluating that target on read/write |
| Class layout plan — proposed | Field and method-slot relationships | Running class initialization |
| Deferred function context — proposed | Definition-time information needed to compile later | The original compiler's mutable cursor or stack pointers |
| VM | Execution, runtime argument/frame setup, returns, runtime dispatch | Compiler register-allocation decisions |

The allocator lives inside each function's `CompilerState`. Guards and call helpers borrow it. They do not each own a second allocator.

## 3. Register concepts, without overloaded terminology

### 3.1 Register identity, next position, and peak count

A register is one slot in the VM's function frame. It is not a physical CPU register.

Suppose these slots are occupied:

```text
r0  parameter
r1  local
r2  scratch value
r3  scratch value

next available position = 4
peak number of slots   = 4
```

Rewinding to position 2 makes `r2` and higher available for subsequent emitted instructions. The peak stays 4. No runtime erase or free instruction is emitted.

The peak number of slots determines the required frame size. It is the existing `max_reg` concept, sometimes called a high-water mark.

A position is not necessarily a valid register identity: after all 250 permitted slots are allocated, the next position is 250, although the highest permitted register is 249.

### 3.2 Language scopes and compiler scratch checkpoints are different

For:

```text
دالة مثال():
    س := 1 + 2
    اكتب(س)
```

The local slot for `س` remains live after the assignment because the function's language scope has not ended. Intermediate slots used to compute its initializer may be reused as soon as the assignment's emitted calculation is complete.

| Cleanup event | Effect |
|---|---|
| Finish expression/statement scratch | Release calculation scratch; preserve locals still in scope and the continuing result |
| Leave a Fairuz lexical scope | Remove that scope's local bindings and release their slots |
| Leave a function at runtime | The VM handles its runtime frame; this is separate from compiler bookkeeping |

`RegMark` is a C++ checkpoint, not a new language scope. Its scan for newly declared locals exists because statement compilation can declare a local before that checkpoint is destroyed.

The local-preservation scan can remain during the initial refactor. Moving it into a more explicit local-lifetime API is an optional later change, not a prerequisite for `CallRegisters`.

### 3.3 Allocator invariants

Preserve these rules:

1. Successful allocation returns a previously available register.
2. Failure leaves the cursor and peak unchanged.
3. Reservation checks the requested register, not whether the current cursor is full.
4. Reserving an already occupied valid destination succeeds without changing state.
5. Rewind only moves backward or stays in place.
6. Rewind never lowers the peak.
7. Higher-level guards must not rewind through live locals or continuing expression results.
8. Slot arithmetic and resource-limit checks happen before narrowing to an encoded operand.

The allocator's rewind assertion checks rule 5. It cannot by itself establish rule 7, because it does not know which local or expression owns a slot.

With the current limit of 250, an eight-bit count can represent the peak. Wider internal counters remain a useful future simplification for range arithmetic. They are not a requirement to rewrite otherwise checked single-register allocation.

A future contiguous reservation should check the entire request before changing state. An API that interprets an arbitrary forward rewind as reservation would violate these contracts.

### 3.4 Expression-result contract

Keep the existing `ExprResult` alternatives:

| Alternative | Meaning | Materialization |
|---|---|---|
| `REG` | The value is already in a register | Move if a different destination is required |
| `RELOC` | An emitted instruction's destination remains patchable | Patch that instruction's destination |
| Constant alternatives | The value need not occupy a register yet | Emit a load into the chosen destination |

A register result may borrow a live local or refer to compiler scratch. Do not treat every `REG` result as a separately owned allocation.

A useful conceptual distinction is:

```cpp
// Illustrative contracts, not required replacement signatures.
compile_value(expression);       // Result remains usable in the caller's scratch lifetime.
compile_into(expression, dst);   // Writes dst without releasing dst during scratch cleanup.
```

When a scratch result must outlive an inner checkpoint, allocate its destination before entering that checkpoint, or explicitly retain it. Do not return a register and then let an inner guard release it.

A `RELOC` result describes a destination to patch, not an independently reserved result slot. Discharge it into a protected destination before later computation needs to preserve that value. Patching a destination does not itself extend register lifetimes or make arbitrary reordering across side effects safe.

Register validity and value stability are separate: a borrowed local register can remain allocated while later code changes its contents. When evaluation order requires a snapshot, copy it to a protected destination before compiling the later side effect.

## 4. What the emitter extraction has achieved, and what remains

The extracted emitter now owns loads, binary operations, field access encodings, call instruction encodings, payload emission, and jump patching. Keep those achievements.

### 4.1 Distinct payload contracts

Trailing `NOP` words have different meanings:

| Consumer | Payload field | Contents |
|---|---|---|
| Binary inline-cache recording | A | Cache slot |
| `INVOKE` cache payload convention | A | Cache slot |
| Named get/set | Bx | Constant-table name index |
| `INVOKE_NAMED` | Bx | Constant-table name index |

The current VM consumes the `INVOKE` payload but does not use its cache index in the same way as binary recording. Preserve its format rather than inferring that the two runtime paths already use identical caching.

Keep `emit_cache_payload()` and `emit_name_payload()` distinct. Prefer making them private once only higher-level emitter operations need them.

### 4.2 Remaining emitter work

- Keep the named-field `0xFF` sentinel internal to named-field helpers.
- Use register types for destinations and position types for instruction positions.
- Distinguish a method-vtable slot from an inline-cache slot. Both may fit in a byte, but `invoke()`'s method-slot operand is not its trailing cache index.
- Keep argument counts distinct from register identities at the semantic API level, even if both encode into an eight-bit field.
- Centralize range checking and signed offset calculation for branches, including backedges.
- Eventually make the emitter depend on the active chunk or a narrow output context rather than the whole compiler.
- Move per-chunk string-constant bookkeeping into a chunk-associated constant pool/builder if the current `(string, Chunk*)` cache becomes unnecessary.

Changing the emitter's dependency requires care: `CompilerStateGuard` switches the active function state. Do not capture the top-level chunk once and accidentally emit a deferred or nested compilation into it. A per-state/per-function emitter or an explicitly supplied chunk is safer than a stale cached pointer. The current compiler callback approach can remain until that lifetime is deliberately redesigned.

### 4.3 Recent regressions as design constraints

The earlier extraction exposed three concrete failure modes:

1. Integer discharge fell through to the float case and overwrote the result. Each materialization case must terminate deliberately.
2. A generic payload helper encoded a cache index in Bx while the decoder expected A. Payload meaning must be part of the operation.
3. An inverted assertion rejected valid jump offsets. User-triggerable bytecode size limits need real error checks, not only assertions removed in release builds.

The inspected emitter contains fixes for these previously identified issues. This is a static observation, not validation of the current typed-operand work.


## 5. Next implementation: call-register preparation

### 5.1 Problem in the current routine

`compile_call_impl()` currently combines:

1. Selecting ordinary call, known-slot invocation, named invocation, or callable-field fallback.
2. Allocating the callee/receiver slot, auxiliary slot, and explicit arguments.
3. Evaluating expressions and materializing their results.
4. Rewinding after nested expressions and after the completed call.
5. Emitting normal calls, tail calls, and explicit returns where required.

The existing `compile_args` and `compile_object` lambdas are useful starting points. The repeated arithmetic around them remains a responsibility to extract.

The target is a small helper called `CallRegisters`. It is compiler bookkeeping for one call site, not a new VM frame implementation.

### 5.2 Inputs, outputs, and ownership

| Item | Contract |
|---|---|
| Allocator | Borrow the current function's `RegisterAllocator` |
| Base register | An already allocated slot belonging to this call's staging range |
| Layout | Ordinary call or invocation with an auxiliary/self slot |
| Location | Used by the compiler adapter when translating allocation errors |
| Output | Argument destinations, appropriate counts, and a retained result register |
| Lifetime | One synchronous traversal of a call AST, including nested call compilation |
| Forbidden dependencies | No AST traversal, method lookup, runtime execution, or compiler-global state |

A compiler adapter may attach source information to allocator errors. The register helper need not own a `SourcePtr`.

The helper must not capture the allocator for a different function or survive a switch of `CompilerState`. A nested expression call uses another helper borrowing the same function allocator; a separately compiled function uses its own state and allocator.

### 5.3 Ordinary-call layout

For `f(10, 20)`, with base `r4`:

```text
r4  callee f / eventual result
r5  explicit argument 0
r6  explicit argument 1
r7+ available scratch
```

The VM reads the ordinary callee from the instruction's A register. Arguments start at A + 1. On completion the result occupies A.

After emission, retaining the result means rewinding to position 5: `r4` stays reserved and later instructions may reuse `r5` onward.

This rewind is not a runtime deletion. The emitted call consumes its arguments before later emitted instructions reuse those slots.

### 5.4 Nested argument example, step by step

For `f(g(1), h(2))`, preserve left-to-right evaluation. The numbers below illustrate the current incremental staging approach; there may be surrounding occupied locals below `r4`.

| Compiler step | Protected outer slots | Nested staging / emitted work | Next position after cleanup |
|---|---|---|---:|
| Stage `f` | `r4 = f` | Evaluate callee and release its scratch | 5 |
| Reserve outer argument 0 | `r4`, destination `r5` | Nested `g` starts above it | 6 |
| Compile `g(1)` | `r4`, `r5` | `r6 = g`, `r7 = 1`, emit nested call | 7 |
| Finish outer argument 0 | `r4`, `r5 = result of g` | Materialize/copy `r6` into `r5`, release nested scratch | 6 |
| Reserve outer argument 1 | `r4`, `r5`, destination `r6` | Nested `h` starts above it | 7 |
| Compile `h(2)` | `r4`, `r5`, `r6` | `r7 = h`, `r8 = 2`, emit nested call | 8 |
| Finish outer argument 1 | `r4`, `r5`, `r6 = result of h` | Materialize/copy `r7` into `r6` | 7 |
| Emit outer call | `r4`, `r5`, `r6` | `IC_CALL` at base `r4` with two explicit arguments | 7 |
| Finish normal call | `r4 = result of f` | Release argument slots for later instructions | 5 |

The first argument's result must survive compilation of the second argument. That is the reason each outer argument gets a destination before its nested expression is compiled.

Do not reserve the entire outer range up front during the first extraction. That is a valid alternative design, but it changes scratch positions and peak usage. Preserve the current incremental strategy first.

### 5.5 Method and named-call layout

For an invocation whose receiver is at base `R`:

```text
R      receiver / eventual result
R + 1  auxiliary slot
R + 2  explicit argument 0
R + 3  explicit argument 1
...
```

The auxiliary slot is used as follows:

- Known-slot `INVOKE`: reserve it; runtime method setup places implicit self there.
- `INVOKE_NAMED`: current compiler code stages the member-name value there; method setup can overwrite it with self.
- Named lookup that resolves to a module export or a callable field: the VM shifts explicit arguments left by one and calls the value without implicit self.

The helper arranges the initial layout. It must not preempt runtime dispatch by adding self to every callable member.

The emitted invocation count currently includes one implicit-self slot. The ordinary call count includes only explicit arguments:

```text
ordinary encoded argc = explicit argument count
invoke encoded argc   = explicit argument count + 1
```

The compiler currently allocates a generic function register before selecting a method path, then may allocate a separate receiver register. Retain that existing allocation in the first extraction. Removing an unused staging register is a separate optimization with separate expected bytecode changes.

### 5.6 Proposed lifecycle and minimal API

A conceptual lifecycle is:

```text
prepare base
    -> compile callee/receiver
    -> finish callee/receiver scratch
    -> prepare auxiliary slot if required
    -> [allocate argument -> compile it -> finish argument] repeated
    -> emit selected call instruction
    -> finish normal result OR finish terminal call
```

One possible API is:

```cpp
enum class CallLayout {
    Ordinary,
    Invocation
};

// Pseudocode: error and count types are illustrative.
class CallRegisters {
public:
    // Adopt an allocated call staging base; do not allocate a second allocator.
    CallRegisters(RegisterAllocator&, RegType base, CallLayout);

    void finish_callee();
    ErrorOr<RegType> prepare_auxiliary();
    ErrorOr<RegType> next_argument();
    void finish_argument(RegType argument);

    size_t explicit_count() const;
    ErrorOr<ArgumentCount> encoded_count() const;

    RegType finish_with_result();
    void finish_terminal();

private:
    RegisterAllocator& registers;
    RegType base;
    CallLayout layout;
    // Track the initialized prefix / outstanding argument / lifecycle state.
};
```

`ArgumentCount` above is a proposed semantic type, not an existing interface to assume available.

For ordinary calls:

- `finish_callee()` retains base and drops only scratch above it.
- `next_argument()` allocates the next slot immediately after the completed prefix.
- `finish_argument()` requires that the value has already been materialized there; it releases only scratch above that slot.
- `finish_with_result()` retains base and releases the argument/auxiliary range.
- `finish_terminal()` releases the staging range because no later instruction on this path consumes a result.

For invocation layout, `prepare_auxiliary()` runs after receiver scratch is removed and before explicit arguments. Keep the auxiliary slot in the protected prefix.

Reject impossible operation ordering with internal assertions: starting another argument before completing the previous one, finishing a frame twice, or finishing an unprepared invocation. Those are compiler bugs, unlike source programs exceeding a register/count limit.

### 5.7 Example use from the compiler

Keep AST handling in `Compiler`:

```cpp
// Pseudocode only: use the project's actual propagation helpers.
auto base = TRY(allocate_call_base(loc));
CallRegisters frame(state.registers, base, CallLayout::Ordinary);

auto callee = TRY(compile_expr_impl(call->callee));
emitter.discharge(callee, base, loc);
frame.finish_callee();

for (auto* argument : call->args) {
    auto destination = TRY(frame.next_argument());
    auto value = TRY(compile_expr_impl(argument));
    emitter.discharge(value, destination, argument->get_location());
    frame.finish_argument(destination);
}

auto argc = TRY(frame.encoded_count());
emitter.ic_call(base, argc, loc);
return ExprResult::reg(frame.finish_with_result());
```

The helper never receives `argument` or invokes `compile_expr_impl()`.

Selection of a known method slot, named invocation, callable-field fallback, and constructor behavior stays in the compiler. The emitter continues to own the instruction and payload encoding.

### 5.8 Existing destination registers

A requested output destination is not automatically a safe call base.

Suppose locals occupy `r0`–`r5` and the caller requests a result in `r1`. Staging arguments at `r2` onward would overwrite live locals. Finishing by rewinding to `r2` would also release them incorrectly.

The safe rule is:

1. Stage the call in a fresh temporary range above the current protected prefix.
2. Emit the call.
3. Copy/materialize its result into the requested destination.
4. Release the temporary staging range while keeping the existing destination and other locals live.

An optimization may reuse a requested destination as base only when it proves the required adjacent slots are available. That is not part of the first extraction.

Audit the `dst` parameter separately from the current common `nullptr` call sites. Do not accidentally broaden its contract while wrapping the code.

### 5.9 Normal calls, tail calls, and method returns

A normal call has a continuing result. A terminal call does not.

Current behavior to preserve:

- Ordinary non-tail call: emit `IC_CALL`, retain base.
- Ordinary tail call: emit `CALL_TAIL`, release compiler staging, mark the statement path terminal.
- Method call in return position: current compiler emits `INVOKE` or `INVOKE_NAMED` followed by `RETURN`. Keep this pair; it is not the same opcode as `CALL_TAIL`.
- Constructor-return exceptions currently selected in `compile_return()` remain there during extraction.

For method-call-plus-return, emit the return while the result slot is still considered live, then complete the terminal lifecycle. Avoid inventing a fake continuing value from a freed terminal frame.

Do not give every call a destructor that blindly rewinds to the entry position. That would release successful results. If RAII rollback is added, use explicit completion state and ensure an outer guard cannot rewind a second time to a position above the already restored cursor.

### 5.10 Errors, limits, and cleanup

- Check explicit count plus the layout's implicit count before narrowing.
- Let register allocation reject frame sizes that cannot fit alongside the function's existing live slots.
- On failure, never return a register result as though staging succeeded.
- In the initial extraction, preserve existing whole-function compilation failure/unwind behavior. A helper does not make partial bytecode rollback automatic.
- If recovering from a failed subexpression inside the same compilation is introduced later, both register state and emitted bytecode/constant/cache changes need a transactional boundary. Rewinding registers alone is insufficient.
- Preserve each argument's source location when converting allocator errors into compile diagnostics.

### 5.11 Migration sequence and completion criteria

First ordinary calls, then known-slot invocations, then named invocations and callable-field fallback. Share argument staging with dictionary construction only after ordinary call staging is stable; preserve the dictionary builtin's existing lookup behavior.

The extraction is complete when:

- Call-slot arithmetic and call-specific rewinds live in the helper.
- Nested calls cannot release earlier outer arguments.
- The compiler still makes the same dispatch and evaluation-order decisions.
- Existing call conventions and normal/tail behavior remain intact.
- Failed allocations cannot be mistaken for register zero.
- No extra per-block allocator or new runtime frame object has been introduced.


## 6. Assignment targets: prepare once, then read or write

### 6.1 Problem

`compile_assign_impl()` handles local/global binding policy, index writes, named/static fields, augmented operations, target capture, and result lifetime. Ordinary and augmented assignments repeat receiver/index/materialization work.

The design should separate deciding what location is assigned from computing and storing its value.

### 6.2 Proposed representation

```cpp
// Conceptual tagged data, not an implementation-ready declaration.
PreparedTarget =
    Local  { register }
  | Global { name_index }
  | Field  { captured_receiver, field_slot_or_name }
  | Index  { captured_container, captured_index };
```

Keep static-field-slot access and dynamic named-field access distinguishable. An unknown layout is not a proof that a numeric field slot is safe.

The compiler prepares the target. Small load/store helpers operate on its captured description without evaluating its AST again.

### 6.3 Ordinary indexed assignment

For `container()[index()] := value()`:

1. Evaluate and capture `container()`.
2. Evaluate and capture `index()`.
3. Evaluate `value()`.
4. Store through the captured container and index.
5. Release target scratch after the write.

The captured receiver/index must survive compilation and runtime evaluation of the RHS. Do not leave them as borrowed local registers if RHS evaluation can rebind those locals.

### 6.4 Augmented indexed assignment

For `container()[index()] += rhs()`:

1. Prepare container and index once.
2. Read the old value before evaluating `rhs()`.
3. Evaluate RHS once.
4. Apply the selected binary operator.
5. Write through exactly the prepared target.

Preparing the target is not an excuse to read its old value again after RHS side effects. Preserve exceptions and side-effect order on every failure path.

For member assignment, the same sequence applies with a captured receiver and member identity.

### 6.5 Names, declarations, and scope

Resolve whether a name means an existing local, new local, implicit instance field, or global using current language/compiler rules. Keep that binding policy in a resolver/compiler adapter.

A prepared target does not get to create a new local merely because an emitter needs a destination. Local declaration timing and global-store behavior remain explicit.

Assignments are statements in the language, while the current AST/codegen still carries an assignment expression internally. Preserve that internal contract initially. Do not re-enable assignment in expression contexts or remove internal assignment results as an unrelated change.

### 6.6 Completion criteria

The assignment routine should read as:

```text
resolve/prepare target
    -> optionally read old value
    -> evaluate RHS
    -> optionally apply augmented operation
    -> store
```

Target-specific receiver/index evaluation exists in one place. Binary-op selection can share a small mapping with ordinary expressions, while preserving immediate-shift versus register-shift encodings.

## 7. Class planning, method registration, and descriptor emission

### 7.1 Problem

`compile_class_def()` currently does all of the following:

- Resolve the parent information available at compile time.
- Merge and deduplicate fields.
- Assign special, inherited, overridden, and new method slots.
- Detect duplicate method names.
- Register deferred method chunks and emit closures.
- Assemble vtable/function relationships.
- Build the runtime descriptor.
- Update compile-time class knowledge and bind the class value.

Those are separate phases even if they remain in one translation unit initially.

### 7.2 Proposed phases

| Phase | Input | Output |
|---|---|---|
| Plan layout | Class AST and known parent layout | Own/inherited fields, complete method slots, dynamic-layout flag |
| Register methods | Plan and method definitions | Deferred function identities and their direct function indices |
| Emit descriptor | Plan and registered identities | Runtime `ClassDescriptor` entry |
| Emit binding | Descriptor identity and class name | `NEW_CLASS` and name/local binding |
| Publish compile-time layout | Validated plan | Layout knowledge used by subsequent compilation |

A `ClassLayoutPlan` should distinguish own fields from inherited/all fields and known layout from runtime-dependent layout.

### 7.3 Preserve the full method table before compiling bodies

Sibling method calls must resolve through a complete slot table regardless of source order. Finish slot planning before recording each method's deferred context.

Preserve:

- Reserved special slots and accepted Arabic operator spellings.
- Inherited method overrides reusing their slots.
- New methods appending in the established order.
- Duplicate-name rejection.
- Dynamic handling when the parent is imported or otherwise lacks a compile-time layout.

Do not infer a fixed slot from a name whose runtime value may have changed.

### 7.4 Keep indices when registering methods

The current code registers chunks into `Chunk::functions`, builds a temporary vtable of chunk pointers, and scans `functions` to recover each index.

Return the index at registration:

```cpp
struct RegisteredFunction {
    FunctionIndex index; // Proposed semantic type.
    Chunk* chunk;
};
```

Use that index to fill the descriptor directly. Preserve the null/inherited-slot sentinel convention.

Whether method closure emission is necessary or redundant is a separate question. Keep its existing emissions and temporary-register effects in the first decomposition.

### 7.5 Compile-time layout and runtime descriptor are different contracts

The primary checkout has already consolidated earlier descriptor structures. Do not repeat that old mechanical merge.

However, two meanings remain:

- Compile-time knowledge: which slots are safe to use for this receiver?
- Runtime descriptor: what data must `NEW_CLASS` use to construct/extend the runtime class?

Those can share storage or helper functions, but their contracts must stay distinct. A missing field list caused by an unknown parent must not masquerade as a complete empty layout.

Completion means each phase can be inspected independently, with no pointer scan needed to rediscover a function identity.

## 8. Explicit deferred function context

### 8.1 Preserve the current successful mechanism

The current callback captures a `CompilerState` snapshot plus globals, module names, and class knowledge, reconstructs a compiler later, and compiles into a temporary chunk.

The transactional chunk replacement is valuable:

```text
original deferred chunk
    -> compile temporary chunk
    -> on failure: keep original deferred chunk
    -> on success: replace original with complete temporary chunk
```

Keep that sequence.

### 8.2 Replace broad state capture with named inputs

Proposed conceptual data:

```cpp
struct DeferredFunctionContext {
    // Definition-time names/class knowledge required by current semantics.
    DefinitionEnvironment environment;
    MethodContext method; // Optional: receiver identity, fields, method slots.
};
```

The exact split can remain simple structs. The important constraint is that a definition snapshot does not contain a live allocator cursor, compiler pointer, or `enclosing` pointer to a destroyed stack object.

Create a fresh function `CompilerState` when materializing the function. Allocate self and parameters there using its own allocator.

The AST/function node and source snapshot must remain available. Preserve the repository's arena/source lifetimes; do not replace them casually with raw references into a temporary file manager.

### 8.3 Definition-time versus invocation-time information

Preserve what the current implementation captures at definition time. Do not silently change which later declarations or rebindings a deferred body sees.

If a metadata table is removed, first identify its actual consumers. For example, a module-name table that is only stored and copied may be removable, but removing it is an audited cleanup, not a general reason to erase all definition metadata.

### 8.4 Failure and retry contracts

- Parse/materialize a deferred AST body only through the existing successful caching contract.
- Do not mutate the published chunk incrementally before compilation succeeds.
- Keep constants, code, caches, and destination-patching state associated with the temporary chunk during an attempt.
- A retry must not inherit stale register state or a string-cache entry belonging to a discarded attempt.
- Preserve original diagnostic source/locations.

Completion means the callback capture can be understood from the context's fields without reconstructing which parts of `CompilerState` were manually cleared.

## 9. Statement control flow as an explicit result

### 9.1 Problem

`is_dead` currently describes whether the current statement path can continue. Branch and loop compilation save and restore it manually.

Error success and fallthrough are separate facts:

```text
compiled successfully, can continue
compiled successfully, cannot continue
failed to compile
```

A successful return statement is not a compilation error, and a branch containing a return does not automatically terminate its sibling branch.

### 9.2 Minimal representation

Start with a small value:

```cpp
enum class StatementFlow {
    FallsThrough,
    TerminatesPath
};

using StatementResult = ErrorOr<StatementFlow, RuntimeError>;
```

This is proposed work. Success-only setup helpers can remain `ErrorOr<void, ...>`. Do not add meaningless booleans to all compiler methods.

### 9.3 Composition rules

| Construct | Flow rule |
|---|---|
| Ordinary statement | Falls through unless its own construct is terminal |
| Return | Terminates current path |
| Break / continue | Terminates current body path; records a jump for the enclosing loop |
| Sequential block | Stop emitting later statements after a terminal statement |
| If without else | Conservatively falls through |
| If with else | Terminal only if every arm terminates |
| Loop | Conservatively falls through unless a later, separately designed analysis proves otherwise |

A break terminates the current loop-body path, not the containing function. Loop compilation consumes that effect through its exit fixup and still leaves a continuation after the loop.

Do not add constant-condition reachability analysis in the first pass.

### 9.4 Preserve observable diagnostics

The existing compiler skips some dead statements. Keep that diagnostic behavior initially where possible. More precise flow composition can change which unreachable code is visited; record and review that separately instead of treating every resulting diagnostic difference as mechanical refactoring.

Completion means branch/loop routines no longer need paired manual `incoming_dead` save/reset operations to protect sibling paths.

## 10. Typed operands and error propagation

### 10.1 Finish the current wrapper migration before stacking another large change

Typed wrappers already exist in the working tree. They are not a future feature to introduce from zero.

Their contracts should be small and conventional:

- Default construction initializes the stored underlying value.
- Prefix increment returns the updated object by reference.
- Postfix increment returns the old value.
- Equality and ordering compare the intended domain.
- A conversion from a wide count/position to an encoded register is checked at the appropriate boundary.
- Arithmetic must not silently erase the tag and later be reinterpreted as an unrelated operand domain.

The current wrapper work must be reviewed against these requirements before relying on it. In particular, review the observed default-constructor initializer and postfix return implementation; introducing a wrapper does not automatically preserve the semantics of the built-in type.

### 10.2 Domains worth separating

| Concept | Why it differs |
|---|---|
| Register identity | A valid frame slot |
| Register position/count | May be one past the highest allocated register |
| Program counter | Position of an instruction word |
| Encoded instruction | Packed bit representation |
| Constant/name index | Entry in a chunk's constant table |
| Function index | Entry in a chunk's function table |
| Method slot | Position in a class vtable |
| Inline-cache slot | Entry in a chunk's cache table |
| Argument count | Number of values passed according to a call convention |

Not all require separate public wrapper classes immediately. Avoid using the same semantic name for two unrelated operands merely because they share a width. For example, `SlotIndex` currently appears in method and cache roles; describe method operands accurately and split the domain when that prevents a real mix-up.

High-level emitter methods should accept semantic operands. A generic raw ABC encoder can convert several different domains into byte fields internally without forcing every high-level argument to pretend to be a register.

### 10.3 Consolidate propagation macros

Keep three visible intentions:

```cpp
TRY_BIND(name, expression);     // Declares a new value.
TRY_SET(&existing, expression); // Assigns an existing value.
TRY_DISCARD(expression);       // Requires success; no value needed.
```

The actual macro names are negotiable. Their behavior is not.

Requirements:

1. Evaluate the expression exactly once.
2. Propagate its original error; do not report a second diagnostic.
3. Preserve appropriate move behavior.
4. Generate hygienic internal temporary names.
5. Document whether expansion declares names in the surrounding block.
6. Use declaration macros as standalone statements in braced scopes.
7. Keep operation-specific helpers thin or remove them if they hide more than they explain.
8. Consolidate duplicate propagation families after their error types are compatible.

The current `TRY_BIND` uses a fixed internal temporary name while other helpers generate unique names. Repeated binds in a block should not collide with each other or an unrelated local.

The previous import regression illustrates the key rule: an allocation request must never invisibly redeclare a register variable when the caller intended to update it.

## 11. Smaller cleanups after responsibility extraction

These are follow-up tasks, not reasons to postpone the call-register extraction indefinitely.

- Audit declaration-only helpers such as assignment/class-method entry points before removing them.
- Remove unreachable fallback returns and unused state only after confirming their consumers.
- Remove the never-enabled operand-swap path if it remains unused.
- Share operator classification between ordinary binary and augmented operations without confusing immediate and register shift encodings.
- Consolidate opcode metadata for encoding format, operand roles, payload kind, and payload length where this would improve both emission and disassembly.
- Improve disassembly so strings print their contents rather than object addresses and payload words identify their associated instruction. Keep printing separate from storage if that makes `Chunk` simpler.
- Keep debug assertions for internal misuse and normal diagnostics for source-triggerable limits. Do not replace the latter with assertions.
- Decide whether compiler-specific operand wrappers belong in a dedicated compiler-types header rather than the general macros header.

Each cleanup should have a concrete reason: remove duplicated policy, prevent a demonstrated operand mix-up, or make an existing invariant visible.


## 12. Migration plan and review boundaries

Use separate, reviewable changes. A milestone is complete only when its own contract is satisfied; a file rename or a new class declaration is not enough.

| Order | Change | Preserve | Completion evidence |
|---:|---|---|---|
| 0 | Stabilize current typed operands and emitter extraction | Opcode numbers, operands, payload formats, diagnostics | Wrapper semantics and emitter/decoder contracts reviewed; validation still pending until run |
| 1 | Extract ordinary-call register preparation | Existing staging order, scratch positions, argument order | Argument/result rewinds owned by `CallRegisters` |
| 2 | Adapt known and named invocations | Auxiliary slot, implicit-self count, runtime callable-member handling | Both invocation paths use the same register-lifecycle operations |
| 3 | Adapt terminal calls and requested destinations | Existing constructor exceptions and invoke-plus-return behavior | Normal result and terminal completion have separate contracts |
| 4 | Reuse argument staging where applicable | Dictionary builtin lookup and ordered key/value evaluation | No duplicated argument-slot bookkeeping for equivalent ordinary calls |
| 5 | Introduce prepared assignment targets | Target evaluation count and old-value-before-RHS behavior | Ordinary and augmented writes share target preparation |
| 6 | Split class layout planning and method registration | Special/inherited slot ordering and descriptor semantics | Direct function indices; no pointer rediscovery scan |
| 7 | Name the deferred definition context | Source/AST lifetime and transactional publication | No broad live compiler-state capture |
| 8 | Return statement-flow information | Language semantics and agreed diagnostic behavior | No manual sibling-path restoration through `is_dead` |
| 9 | Narrow emitter dependencies and constant-pool ownership | Correct active chunk across state switches/retries | Emitter no longer needs the whole compiler; per-chunk cache ownership is clear |
| 10 | Split files and remove audited dead machinery | Interfaces established above | Source organization reflects responsibilities rather than hiding coupling |

Orders 5–8 can be adjusted when one change needs a preceding interface. The important constraint is that unrelated representation, lifetime, and semantic changes do not get bundled into one opaque diff.

### 12.1 First concrete change to implement

For the next implementation task, limit the scope to order 1:

1. Add `CallRegisters` with ordinary-call layout.
2. Adopt the existing allocated ordinary-call base.
3. Move argument destination allocation and finish-argument rewind into it.
4. Move normal-call result retention into it.
5. Retain method paths, the generic upfront allocation, and opcode selection as they are.
6. Document the destination contract rather than silently reusing an arbitrary local as a base.
7. Review the emitted sequence for one flat call and one nested call.

That first version need not support every eventual API listed in section 5. Establish the lifetime contract on the smallest path first.

### 12.2 Review questions for every milestone

- Which single responsibility moved?
- Which previously duplicated decision now has one implementation?
- Did emitted bytecode change intentionally, or should it remain equivalent?
- Are counts checked before encoding?
- Can failure leave a partially published result or a stale allocator/cache state?
- Which previously valid binding/receiver could be overwritten by scratch?
- Do source locations still identify the offending expression?
- Is a behavior change being hidden inside something described as extraction?

Record intentional bytecode changes separately from defects. For example, removing an unused register can change the peak count without changing program output; that is still a reviewable optimization.

## 13. Verification plan — not executed

The user requested written design and previously required parser tests to be written without execution. This document does not authorize or claim test runs. Use this section when validation is later requested.

### 13.1 Review can begin with static checks

- Check source references after each interface move.
- Search for direct cursor mutation outside the allocator.
- Search for instruction construction/patching outside the emitter and chunk implementation.
- Inspect VM decoders alongside every payload helper.
- Read a changed error path from its origin through its caller rather than checking only the local return type.
- Confirm every newly introduced file is part of the intended source tree and commit.

These checks do not demonstrate runtime correctness.

### 13.2 Focused scenarios for future validation

| Area | Required cases | Property |
|---|---|---|
| Allocator | Allocate all valid slots, fail next allocation, reserve invalid slot, reserve occupied valid slot at capacity | Correct boundaries; no mutation on failure |
| Allocator | Nested checkpoint cleanup, lexical scope exit, peak after rewind | Correct lifetime and frame size |
| Typed operands | Default value, prefix/postfix behavior, comparisons, checked narrowing | Wrappers preserve their documented semantics |
| Ordinary calls | Zero/multiple arguments, computed callee, `f(g(1), h(2))` | Consecutive arguments and preserved earlier values |
| Calls with side effects | Callee/arguments log their evaluation order | Each expression once, correct ordering |
| Invocations | Known self method, chained receiver, imported module member, callable instance field | Existing dispatch and self handling |
| Return paths | Ordinary tail call, native tail call, constructor return, invoke-plus-return | Correct terminal/continuing behavior |
| Destinations | Result into an existing local with live neighboring locals | Staging cannot overwrite unrelated locals |
| Argument limits | Explicit count plus implicit self; large live-local prefix | Checked count and frame-capacity boundaries |
| Assignments | Receiver/index/RHS with side effects, RHS rebinding, augmented mutation | Capture once; old value before RHS |
| Classes | Sibling method forward calls, overrides, special methods, imported parent | Complete and correct slot layouts |
| Deferred bodies | Original compiler destroyed, failed attempt, retry, multiple modules | Definition context and transaction lifetime |
| Flow | Return in one/both branches, break/continue in nested branches/loops | Correct path composition |
| Payloads | Multiple binary operations using nonzero cache slots; named fields | A versus Bx remains correct |
| Jumps | Forward/backward jumps, break/continue, zero displacement, boundary/oversized offsets | Correct arithmetic and normal overflow diagnostic |
| Constants | Integer/float/bool/nil, large literals, NaN-box and fallback paths where supported | Exactly one intended materialization path |

A particularly important nested-call invariant is that earlier arguments remain stable even if a later argument compiles another call. A particularly important failure invariant is that no overflow returns a successful register zero.

### 13.3 What to compare

Use the existing compiler bytecode checks for operand/layout contracts and runtime tests for observable results and side effects. AST comparisons serve parser behavior; they cannot alone verify register staging.

When preserving exact staging, inspect relevant instruction sequences as well as output. Avoid brittle whole-program snapshots if a focused operand/payload assertion captures the invariant.

Once the focused checks pass, broader compiler/VM/CLI/stdlib validation can establish integration confidence. A planned checklist is not evidence that those checks passed.

### 13.4 Failure recovery during implementation

If a milestone fails, diagnose that milestone's bytecode and lifetime changes before layering on the next abstraction. Preserve unrelated user edits. Do not reset a dirty checkout to recreate a baseline.

Deferred-compilation retry correctness must include the temporary chunk's constants and cache state, not just its code buffer. Register cleanup does not repair a partially committed chunk.

## 14. Eventual file organization and source map

### 14.1 Possible destination structure

This is a target organization, not an instruction to create every file now:

```text
include/Compiler/
    compiler.hpp
    register_allocator.hpp       existing allocation owner
    call_registers.hpp           proposed call staging helper
    bytecode_emitter.hpp         existing encoding owner
    prepared_target.hpp          proposed captured assignment location
    class_layout.hpp             proposed class planning data
    deferred_function.hpp        proposed definition-time context
    chunk.hpp
    opcode.hpp

lib/Compiler/
    compiler.cc                  orchestration and binding adapters
    compile_expr.cc              values, operators, expression dispatch
    compile_calls.cc             call selection and AST traversal
    compile_targets.cc           target preparation and assignment lowering
    compile_control.cc           blocks, branches, loops, returns
    compile_functions.cc         definitions and deferred compilation
    compile_classes.cc           layout planning and descriptor production
    compile_imports.cc           imported binding emission
    bytecode_emitter.cc
    chunk.cc
    compile_macros.hpp           one consistent propagation family
```

A small helper can be header-only if it has no substantial implementation. A proposed data type can remain private until multiple consumers need it. Do not multiply public classes solely to match this tree.

Move functions when their responsibility is clear. Splitting the existing methods across more files while keeping their old cross-cutting bookkeeping would not complete this refactor.

### 14.2 Current source entry points

Paths are relative to this document so links remain usable after moving the checkout.

| Source | What to inspect |
|---|---|
| [compiler.hpp](../../include/Compiler/compiler.hpp) | `CompilerState`, `RegMark`, `ScopeGuard`, `ExprResult`, compiler interfaces |
| [register_allocator.hpp](../../include/Compiler/register_allocator.hpp) | Allocation, rewind, reservation, peak usage |
| [compile_expr.cc](../../lib/Compiler/compile_expr.cc) | `compile_call_impl`, assignment lowering, expression materialization |
| [compile_stmt.cc](../../lib/Compiler/compile_stmt.cc) | Classes, deferred functions, branching, loops, return handling |
| [compiler.cc](../../lib/Compiler/compiler.cc) | Name lookup, compile entry points, register-error adapters, string interning |
| [bytecode_emitter.hpp](../../include/Compiler/bytecode_emitter.hpp) | Semantic emission interface and operand types |
| [bytecode_emitter.cc](../../lib/Compiler/bytecode_emitter.cc) | Payload formats, discharge, jump calculation |
| [chunk.hpp](../../include/Compiler/chunk.hpp) | Published code, constants, functions, descriptors, deferred callback |
| [chunk.cc](../../lib/Compiler/chunk.cc) | Storage limits, jump patching, disassembly |
| [opcode.hpp](../../include/Compiler/opcode.hpp) | Field widths, limits, instruction formats |
| [compile_macros.hpp](../../lib/Compiler/compile_macros.hpp) | Bind/set/discard propagation and operation wrappers |
| [Macros.hpp](../../include/Macros.hpp) | Current strong-alias definitions |
| [execute.cc](../../lib/Interpreter/execute.cc) | `IC_CALL`, `CALL_TAIL`, `INVOKE`, `INVOKE_NAMED` decoding |
| [call.cc](../../lib/Interpreter/call.cc) | Runtime frame setup, constructors, implicit-self placement |
| [vm.cc](../../lib/Interpreter/vm.cc) | Binary cache payload decoding |
| [compiler tests](../../tests/test_compiler/test_compiler.cpp) | Existing instruction/operand assertions |

### 14.3 Overall completion criteria

The design has been implemented when:

1. Each function has one register allocator, and no other component mutates its raw counters.
2. Call staging is described through a call-register lifecycle rather than repeated arithmetic.
3. The emitter owns instruction/payload packing and jump patching.
4. Assignment locations are captured once and reused for read/write.
5. Class planning, method registration, and descriptor emission have explicit boundaries.
6. Deferred functions retain a named definition context and publish compiled chunks transactionally.
7. Statement control-flow outcomes are explicit instead of communicated by mutable sibling-path flags.
8. Operand types communicate meaning without silently converting unrelated domains.
9. Error propagation remains recognizable, hygienic, and source-aware.
10. Responsibility changes are distinguishable from optional optimizations and language changes.
11. Validation results are recorded only after the corresponding checks actually run.

The next actionable milestone remains ordinary-call register preparation. The later sections explain how to continue without redesigning the VM or changing Fairuz's scope semantics.
