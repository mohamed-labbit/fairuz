# Fairuz Isolated Optimization Modules

This document describes the isolated optimization prototypes added alongside the existing compiler pipeline. These modules are intentionally opt-in: they do not replace `ssa_loop.cc`, do not modify `compiler.cc`, and do not affect execution unless a caller explicitly invokes their public APIs.

The modules are:

- `fairuz/ssa_loop_2.hpp` / `fairuz/ssa_loop_2.cc`
- `fairuz/ssa_constructs_2.hpp` / `fairuz/ssa_constructs_2.cc`
- `fairuz/bytecode_optim_2.hpp` / `fairuz/bytecode_optim_2.cc`
- `fairuz/tier_optim_2.hpp` / `fairuz/tier_optim_2.cc`

## Design Goals

The optimization work is split by compiler layer:

1. `ssa_loop_2`: loop-specific symbolic range analysis and bounds-check elimination.
2. `ssa_constructs_2`: AST construct-level analysis across expressions, assignments, branches, functions, classes, and loops.
3. `bytecode_optim_2`: bytecode-level peephole, reachability, branch folding, compaction, and jump repair.
4. `tier_optim_2`: higher-tier planning for inlining, escape analysis, specialization, and VM/JIT-style tiering.

The current active Fairuz pipeline is still:

```text
Parser / analyzer -> AST -> compiler -> bytecode -> VM
```

The isolated optimizer architecture models a future pipeline:

```text
AST
  -> construct analysis
  -> loop analysis
  -> bytecode generation
  -> bytecode optimizer
  -> tier/specialization planner
  -> VM
```

## `ssa_loop_2`: Loop Range and Bounds Analysis

Public API:

```cpp
fairuz::ssa_loop_2::optimize_while_loop(AST::Fa_WhileStmt*);
fairuz::ssa_loop_2::optimize_for_loop(AST::Fa_ForStmt*);
```

### Main Optimizations

`ssa_loop_2` focuses on loop facts that can prove index operations safe.

Implemented analyses:

- Pseudo-CFG construction for loops.
- Pseudo-SSA definition collection.
- Symbolic affine bounds.
- Guard extraction from loop conditions.
- Fixed-point range refinement.
- Induction variable detection.
- Scalar-evolution-style add recurrence detection.
- Trip-count estimation.
- List length stability analysis.
- Loop-invariant expression detection.
- Strength-reduction candidate detection.
- Read and write bounds-check candidates.
- Safe read index marking via `Fa_IndexExpr::make_safe()`.

### Symbolic Bounds

The loop optimizer represents bounds as linear expressions:

```text
constant + c1*x + c2*len(a)
```

Examples:

```text
0
i
i + 1
len(a)
len(a) - 1
```

This lets the optimizer prove cases such as:

```fairuz
i := 0
while i < len(a):
    a[i]
    i += 1
```

The index range is:

```text
i in [0, len(a) - 1]
```

So `a[i]` is safe if `a` has stable length in the loop.

### Guard Facts

Loop conditions are converted into facts.

Examples:

```fairuz
i < 10
```

becomes:

```text
i <= 9
```

```fairuz
i >= 0 and i < len(a)
```

becomes:

```text
i >= 0
i <= len(a) - 1
```

Supported guard operators:

- `<`
- `<=`
- `>`
- `>=`
- `==`
- conjunction via logical `and`

Disjunction is deliberately not used for proof unless future path splitting is added. A condition like `A or B` cannot be safely intersected into one fact set.

### Induction Variables

The optimizer recognizes recurrences:

```fairuz
i := i + c
i := c + i
i := i - c
```

including augmented assignment after parser lowering.

The internal model is:

```text
{start, +, step}<loop>
```

Examples:

```fairuz
i := 0
while i < n:
    i += 1
```

is:

```text
{0, +, 1}<loop>
```

Negative steps are also modeled:

```fairuz
i := n - 1
while i >= 0:
    i -= 1
```

is:

```text
{n - 1, +, -1}<loop>
```

### Trip Counts

For simple induction/guard pairs, the optimizer derives trip-count facts.

Positive step:

```text
i starts at s
i <= u
i += step
```

Approximate exact trip count:

```text
ceil((u - s + 1) / step)
```

Negative step:

```text
i starts at s
i >= l
i -= step
```

Approximate exact trip count:

```text
ceil((s - l + 1) / abs(step))
```

Symbolic trip upper bounds are retained even when the exact integer value is unavailable.

### List Length Stability

Bounds-check elimination requires stable list length.

Allowed:

```fairuz
a[i] := value
```

This mutates an element but does not change length.

Rejected:

```fairuz
a := other
append(a, x)
اضف(a, x)
pop(a)
احذف(a)
```

These can change list identity or length, so the optimizer refuses to prove indexes into `a` safe.

### Bounds-Check Proof

For an index:

```fairuz
a[idx]
```

the proof obligation is:

```text
idx.lower >= 0
idx.upper <= len(a) - 1
```

For literal lists:

```fairuz
[1, 2, 3][i]
```

the list length is known immediately.

For named lists:

```fairuz
a[i]
```

the optimizer resolves `a` through `LoopHeader` and requires stable length.

### Current Mutating Behavior

`ssa_loop_2` only mutates AST nodes for proven safe read indexes:

```cpp
index_expr->make_safe();
```

Write-side safe candidates are tracked in stats but not lowered, because the active compiler has no `UNSAFE_LIST_SET` opcode yet.

## `ssa_constructs_2`: AST Construct Analysis

Public API:

```cpp
fairuz::ssa_constructs_2::optimize_program(Fa_Array<AST::Fa_Stmt*> const&);
fairuz::ssa_constructs_2::optimize_stmt(AST::Fa_Stmt*);
```

This module analyzes all currently supported AST constructs at a broad level.

### Covered Constructs

- Literals
- Names
- Unary expressions
- Binary expressions
- Calls
- Lists
- Dicts
- Indexes
- Assignment expressions
- Expression statements
- Assignment statements
- Blocks
- `if`
- `while`
- `for`
- Functions
- Returns
- Break / continue
- Classes

### Pseudo-SSA

Assignments create versioned pseudo-SSA definitions:

```text
x_1 = 1
x_2 = x_1 + 2
```

This enables:

- copy-propagation candidates
- constant-propagation candidates
- dead-store candidates
- local value facts

It is not a full SSA implementation: there are no real phi nodes yet. Branch merges use conservative value merging.

### Constant Folding

The analyzer evaluates constant expressions such as:

```fairuz
1 + 2
10 < 20
not true
```

and increments `constant_fold_candidates`.

This module reports opportunities; it does not rewrite the AST.

### Copy Propagation

When a name resolves to a constant or a simple copy fact:

```fairuz
y := x
z := y
```

the use of `y` is counted as a copy-propagation candidate.

### Common Subexpression Detection

Expressions are assigned structural keys, for example:

```text
bin:ADD(n:x,i:1)
```

Repeated keys are counted as common-subexpression candidates.

This is a local GVN-style prototype.

### Branch Analysis

For:

```fairuz
if true:
    ...
else:
    ...
```

the module identifies a branch simplification candidate.

For comparison guards:

```fairuz
if i < 10:
```

it records guard facts such as:

```text
i <= 9
```

### Reachability

After:

```fairuz
return value
break
continue
```

subsequent statements in the same traversal are considered unreachable candidates.

### Function Summaries

For each function, the module tracks:

- name
- parameter count
- return presence
- tail-recursion candidate
- effect summary

Tail-recursion detection looks for:

```fairuz
return f(...)
```

inside function `f`.

### Class Summaries

For each class, the module tracks:

- class name
- member count
- method count

Methods are analyzed recursively as functions/statements.

### Effect Facts

Calls are conservatively classified.

Pure builtins:

```text
len
حجم
str
int
```

Mutating builtins:

```text
append
اضف
pop
احذف
```

Unknown calls are treated conservatively.

## `bytecode_optim_2`: Bytecode Optimizer

Public API:

```cpp
fairuz::bytecode_optim_2::optimize_chunk(runtime::Fa_Chunk* chunk, bool apply_rewrites = false);
```

This module works after bytecode generation.

It has two modes:

```cpp
optimize_chunk(chunk, false); // analyze only
optimize_chunk(chunk, true);  // apply safe local rewrites
```

### Bytecode Basic Blocks

The optimizer discovers basic-block leaders:

1. instruction `0`
2. jump targets
3. instruction after a jump

The number of leaders is reported as `basic_blocks`.

### Reachability

The optimizer performs graph traversal over bytecode instructions.

Edges:

- fallthrough edge for ordinary instructions
- target edge for jumps
- target and fallthrough for conditional jumps

Unvisited instructions are unreachable candidates.

In rewrite mode, unreachable instructions are removed during compaction.

### Constant Propagation Over Registers

The optimizer tracks simple register constants:

```text
r0 = 1
r1 = 2
r2 = r0 + r1
```

The register map stores:

```cpp
reg -> known constant value
```

It understands:

- `LOAD_NIL`
- `LOAD_TRUE`
- `LOAD_FALSE`
- `LOAD_INT`
- `LOAD_CONST`
- `MOVE`
- selected integer binary operations

Unknown side-effecting operations clear affected register facts.

### Integer Constant Folding

When both operands of an arithmetic/comparison instruction are known integers, the optimizer folds:

```text
OP_ADD r2, r0, r1
```

into:

```text
LOAD_INT r2, folded_value
```

when the folded integer fits the existing `LOAD_INT` encoding.

Supported operations:

- add
- subtract
- multiply
- divide, if divisor is nonzero
- modulo, if divisor is nonzero
- equality
- inequality
- less-than
- less-than-or-equal

### Redundant Move Removal

This pattern:

```text
MOVE r, r
```

is removed.

In rewrite mode it first becomes `NOP`, then compaction removes it.

### Jump Simplification

This pattern:

```text
JUMP +0
```

or equivalent conditional jump-to-next is removed.

### Conditional Branch Folding

If a conditional branch depends on a known constant register:

```text
LOAD_TRUE r0
JUMP_IF_TRUE r0, target
```

then rewrite mode converts it to:

```text
JUMP target
```

If the branch is known not taken, it becomes:

```text
NOP
```

The later compaction pass removes the `NOP`.

### Jump Threading Candidates

If a jump targets another unconditional jump:

```text
JUMP L1
L1:
JUMP L2
```

the optimizer reports a jump-threading candidate.

The current rewrite path reports this but does not yet retarget through chains before compaction.

### Return Specialization

This pattern:

```text
RETURN r, 1
```

can be rewritten as:

```text
RETURN1 r
```

when rewrite mode is enabled.

### Global Cache Candidates

The optimizer reports candidates for:

- `LOAD_GLOBAL`
- `STORE_GLOBAL`

These could later be rewritten into cached global opcodes after cache-slot allocation and invalidation rules are integrated.

### Constant Pool Duplicate Detection

The optimizer scans constants and reports duplicate boxed values.

It does not rewrite the constant pool yet because that requires remapping all `LOAD_CONST`, `LOAD_GLOBAL`, `STORE_GLOBAL`, and closure/function references safely.

### Bytecode Compaction

In rewrite mode, after local rewrites:

1. reachability is recomputed
2. unreachable instructions and `NOP`s are removed
3. old instruction indexes are mapped to new instruction indexes
4. surviving jumps are retargeted
5. `code` is replaced with compacted code
6. `locations` is compacted in parallel
7. line tables are rebuilt

Jump retargeting uses:

```text
new_offset = new_target - new_pc - 1
```

### Source Location Repair

After compaction, `locations` is rebuilt to match the new bytecode layout.

Then `lines` is rebuilt from `locations`:

```text
line entry starts whenever source line changes
```

This keeps disassembly and diagnostics aligned with the compacted instruction stream.

## `tier_optim_2`: Tiering and Specialization Planner

Public API:

```cpp
fairuz::tier_optim_2::analyze_program(Fa_Array<AST::Fa_Stmt*> const&);
fairuz::tier_optim_2::analyze_chunk(runtime::Fa_Chunk const*);
```

This module does not rewrite code. It identifies future tiering and specialization opportunities.

### AST-Level Planning

The AST analyzer reports:

- small-function inline candidates
- tail-recursive function candidates
- allocation sites
- non-escaping allocation candidates
- scalar-replacement candidates
- type-feedback sites
- native-call specialization candidates

### Small Function Inlining

A function is considered an inline candidate when its body has at most a small number of statements.

Current threshold:

```text
<= 8 statements
```

This is a heuristic only. Real inlining needs:

- body cloning
- parameter substitution
- local remapping
- recursion controls
- debug/source location preservation

### Tail Recursion

The planner detects:

```fairuz
fn f(...):
    ...
    return f(...)
```

as a tail-recursive function candidate.

A future transform can lower this to:

```text
assign parameters
jump to function entry
```

or to a canonical loop.

### Escape and Allocation Planning

List and dict literals are counted as allocation sites.

Potential future optimizations:

- stack allocation
- scalar replacement
- allocation sinking
- allocation elimination

The current escape check is shallow and conservative. A real implementation needs def-use chains and call effect information.

### Native Call Specialization

Calls to known builtins are reported as specialization candidates:

- `len`
- `حجم`
- `append`
- `اضف`

Examples of future lowering:

```text
CALL len(list)
```

to:

```text
LIST_LEN
```

when the argument type is proven or profiled as a list.

### Bytecode-Level Tiering

The bytecode analyzer reports hot-site categories:

- call sites
- global cache sites
- arithmetic sites
- index sites
- type-feedback sites
- JIT/tier-up candidates

Current tier-up heuristics:

- large chunks, currently more than 64 instructions
- inline-cache slots with more than 1000 hits

Future runtime integration could use these facts to trigger:

- bytecode specialization
- inline cache patching
- template JIT stubs
- full native compilation for hot loops/functions

## Integration Boundaries

All modules are currently isolated. They compile as standalone translation units and are not referenced by the active compiler pipeline.

Current active behavior:

```text
No optimizer module runs unless explicitly called.
```

Recommended integration order:

1. Integrate `bytecode_optim_2::optimize_chunk(chunk, true)` behind a compiler flag.
2. Add tests for bytecode compaction, jump retargeting, and line-table repair.
3. Add a safe store opcode before enabling write-side bounds-check elimination.
4. Feed `ssa_loop_2` safe-index facts into compiler lowering.
5. Use `ssa_constructs_2` only for analysis until AST rewrites have robust ownership and replacement APIs.
6. Use `tier_optim_2` after VM inline-cache hit data is meaningful.

## Current Limitations

The modules are intentionally conservative and incomplete in several ways:

- No real CFG object shared by all passes.
- No real SSA phi nodes.
- No full dominator tree.
- No full alias analysis.
- No full effect system.
- No function body cloning.
- No AST replacement framework.
- No global constant-pool remapping.
- No runtime tiering hook.
- No safe list-store opcode.

These are infrastructure gaps rather than algorithmic blockers.

## Summary

The implemented optimizer family now covers:

- loop range analysis
- scalar-evolution-style induction facts
- bounds-check proof
- AST constant/copy/CSE/dead-store candidates
- branch and reachability analysis
- function/class summaries
- bytecode constant folding
- bytecode branch folding
- bytecode reachability and compaction
- jump retargeting
- line-table repair
- tiering and specialization planning

The strongest currently executable rewrite layer is `bytecode_optim_2` with `apply_rewrites = true`. The AST and loop modules mostly produce facts and candidates, except for safe read indexes in `ssa_loop_2`, which can mark `Fa_IndexExpr` nodes safe when explicitly invoked.
