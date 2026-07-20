# AST-Level vs Bytecode-Level Optimizations: Analysis for Fairuz

## Executive Summary

**For Fairuz, a hybrid approach is optimal:**
- **AST-level:** Architectural, semantic, high-level transformations (constant folding, dead code elimination, algebraic simplification)
- **Bytecode-level:** Micro-optimizations, peephole, instruction selection, and register allocation (if you have registers)

---

## 1. AST-LEVEL OPTIMIZATIONS

### Characteristics

**When they run:** Before bytecode generation (post semantic analysis)

**What they see:** Full AST with type information, scope, and semantic properties

**What they transform:** Language constructs → simpler language constructs

**Scope:** Entire program visible; global analysis possible

### Advantages of AST-Level

#### 1.1 Semantic Understanding
```
AST has full context:
- Type information (int vs float, signed vs unsigned)
- Scope information (local vs global, free vs bound)
- Side effect analysis (pure vs impure functions)
- Dead code detection
```

**Example:** You know `x * 0` is always an int, not a float. You can fold to `0` safely.
At bytecode level, you might not know the type anymore.

#### 1.2 Architectural Transformations
```
High-level rewrites that would be costly at bytecode level:

- Loop strength reduction (induction variable simplification)
  for i = 0 to n:
      arr[i * 4] = ...
  
  AST-level: Transform to increment-by-4 loop
  Bytecode-level: Would need to analyze branch targets, compute loop structure
```

#### 1.3 CSE (Common Subexpression Elimination)
```
AST level: Easy to detect identical subtrees
  x = a + b
  y = a + b    ← Same subtree structure, identical operands

Bytecode level: Harder (operand tracking is more complex)
  LOAD a
  LOAD b
  ADD
  STORE x
  LOAD a
  LOAD b
  ADD
  STORE y    ← Must track data flow to recognize CSE
```

#### 1.4 Better Register/Stack Allocation Decisions
```
With AST context, you know:
- Variable lifetime (when it's last used)
- Expression nesting depth
- Whether a value is spilled or not

At bytecode level, you've already decided stack layout.
```

#### 1.5 Function Inlining & Specialization
```
AST-level: Easy to inline small functions, generate specialized versions

Bytecode-level: Harder; bytecode is already generated
```

#### 1.6 Algebraic Simplifications
```
(a + b) - a → b         (AST-level: pattern match and rewrite)
(x * 2) / 2 → x         (AST-level: recognize and simplify)
!!x → x                 (AST-level: simplify double negation)

At bytecode level: Much harder to recognize structure
```

#### 1.7 Type-Directed Optimizations
```
float x = (float)(int_value + 1)
  
AST-level: See the cast, optimize integer addition
Bytecode-level: Integer conversion already happened
```

### Disadvantages of AST-Level

#### 1.1 Code Generation Assumptions
```
If you optimize aggressively at AST level, you assume
your bytecode generator is smart enough to preserve those optimizations.

Risk: Optimize away something the codegen could have optimized differently
```

**Example:**
```
AST: Transform (x * 2) to (x + x)
Bytecode codegen: Sees ADD and generates:
  LOAD x
  LOAD x
  ADD
  
vs. what you wanted:
  LOAD x
  SHIFT_LEFT 1
```

#### 1.2 Context Loss
```
AST-level optimizations depend on having type/scope information.

If that information isn't available or accurate, optimization fails silently
or produces wrong results.
```

#### 1.3 Performance Estimation
```
Hard to estimate actual bytecode size/speed impact without seeing bytecode.

You might spend effort optimizing something that bytecode generation
makes moot anyway.
```

---

## 2. BYTECODE-LEVEL OPTIMIZATIONS

### Characteristics

**When they run:** After bytecode generation

**What they see:** Bytecode instructions, register/stack layout

**What they transform:** Instruction sequences → shorter/cheaper sequences

**Scope:** Can't see high-level structure; must work with lowered form

### Advantages of Bytecode-Level

#### 2.1 Direct Instruction Manipulation
```
You see exactly what will execute. No ambiguity.

LOAD x
LOAD 0
COMPARE_EQ
POP_JUMP_IF_FALSE

Recognizable pattern → replace with TEST_ZERO + POP_JUMP_IF_FALSE
```

#### 2.2 Peephole Optimization
```
Classic pattern matching on instruction sequences:

Pattern:
  LOAD const
  STORE var
  LOAD var
  ...

Replace with:
  LOAD const
  ...
  (reuse LOAD const value; don't store)

Easy at bytecode level; hard at AST level.
```

#### 2.3 Dead Code Elimination (at bytecode level)
```
Unreachable bytecode is easy to detect:
  LOAD 1
  POP_JUMP_IF_TRUE label
  <unreachable code>
  label: ...

Simple data flow analysis on bytecode.
```

#### 2.4 Branch Target Optimization
```
If a jump target is unused, remove it.
If a branch always taken, replace with unconditional jump.

Easy to analyze at bytecode level; hard at AST level where jumps are implicit.
```

#### 2.5 Constant Propagation
```
Bytecode makes values explicit:
  LOAD_CONST 5
  STORE x
  LOAD x
  ADD y
  
Obvious that we can replace LOAD x with LOAD_CONST 5.
```

#### 2.6 Code Size Reduction
```
Merge small instructions:
  LOAD_CONST 0
  STORE x
  
vs. 
  STORE_CONST 0 x

Recognizable at bytecode level; requires codegen knowledge at AST level.
```

#### 2.7 Verifiable Correctness
```
You can write unit tests for bytecode transformations:
```
input_bytecode = [
  LOAD x,
  LOAD 0,
  COMPARE_EQ,
  BRANCH
]
expected_output = [
  LOAD x,
  TEST_ZERO,
  BRANCH
]

Easy to verify; AST transformation testing is harder (must parse → optimize → codegen).
```

### Disadvantages of Bytecode-Level

#### 2.1 Information Loss
```
AST has type information.
Bytecode doesn't (unless you emit type metadata).

LOAD x
LOAD 0
COMPARE_EQ

Is this integer comparison or float comparison?
Already lost this information.
```

#### 2.2 Context-Dependent Optimizations
```
Can't reason about variable scope, lifetime, or whether a function is pure.

Example: Can we reorder these instructions?
  CALL function
  LOAD x
  ADD
  
At AST level: Check if function has side effects; if pure, reorder.
At bytecode level: Must be conservative; don't reorder.
```

#### 2.3 Limited to Local Patterns
```
Bytecode-level optimizations are typically local (peephole):
  Match 3-4 instruction patterns, replace with 2-3

Global optimizations (loop optimization, function inlining) are much harder.
```

#### 2.4 Redundant Work
```
If AST codegen is smart, bytecode optimizations might be redundant:
  AST: (x * 2)
  Codegen: LOAD x; SHIFT_LEFT 1   (smart codegen)
  Bytecode optimizer: Nothing to do
  
But if codegen is naive:
  Codegen: LOAD x; LOAD 2; MUL
  Bytecode optimizer: Recognize pattern, optimize to SHIFT_LEFT
```

#### 2.5 Harder to Test
```
Testing requires:
1. Write source code
2. Compile to AST
3. Codegen to bytecode
4. Optimize bytecode
5. Execute/verify
6. Check results

vs. AST level:
1. Write source code
2. Compile to AST
3. Optimize AST
4. Codegen to bytecode
5. Execute

Longer pipeline = harder to debug.
```

---

## 3. HYBRID APPROACH (Recommended for Fairuz)

### Strategy

**Tier 1: AST-Level (Semantic optimizations)**
- Constant folding
- Dead code elimination (unreachable branches)
- Algebraic simplifications
- Loop strength reduction
- Function call with const args → inline
- Type-directed transformations
- CSE (common subexpression elimination)
- Copy propagation
- Range analysis for bounds checking

**Tier 2: Bytecode-Level (Instruction optimizations)**
- Peephole optimization (pattern matching)
- Comparison strength reduction
- Instruction selection refinement
- Register/stack allocation optimization
- Unreachable bytecode elimination
- Branch target simplification
- Constant propagation (in bytecode form)

### Why Hybrid Works Best

```
┌─────────────────┐
│   Source Code   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   AST (typed)   │
└────────┬────────┘
         │
    [AST Optimizer] ◄── High-level transformations
         │              - Constant folding
         │              - Loop strength reduction
         │              - CSE
         │              - Dead code elimination
         │
         ▼
┌──────────────────────┐
│ Optimized AST        │
└────────┬─────────────┘
         │
    [Codegen]
         │
         ▼
┌──────────────────────┐
│ Bytecode (unoptimized)│
└────────┬─────────────┘
         │
  [Bytecode Optimizer] ◄── Low-level peephole optimizations
         │                 - Comparison strength reduction
         │                 - Branch optimization
         │                 - Instruction merging
         │
         ▼
┌──────────────────────┐
│ Bytecode (optimized) │
└────────┬─────────────┘
         │
      [VM]
```

### Example: Loop Optimization

#### Input Source
```cpp
for (int i = 0; i < n; i++) {
    arr[i * 4 + offset] = compute(i);
}
```

**AST-Level Optimization:**
```
Recognize loop induction variable pattern:
- i increments by 1
- Use of i * 4 is linear
- Transform to increment-by-4 loop

Optimized AST:
for (int i = 0; i < n; i++) {
    j = offset + (i * 4)       // Computed once outside loop
    arr[j] = compute(i)
    // Next iteration: j += 4
}
```

**Bytecode Generation (smart codegen):**
```
LOAD_CONST offset
STORE j

label_loop_start:
LOAD i
LOAD n
COMPARE_LT
POP_JUMP_IF_FALSE label_loop_end

LOAD j
CALL compute(i)
STORE_INDEX arr
LOAD j
LOAD_CONST 4
ADD
STORE j

LOAD i
LOAD_CONST 1
ADD
STORE i
JUMP label_loop_start
label_loop_end:
```

**Bytecode-Level Optimization (peephole):**
```
Recognize pattern:
  LOAD j
  LOAD_CONST 4
  ADD
  STORE j

This is just j += 4; replace with single instruction if available:
  ADD_CONST_STORE j, 4
```

---

## 4. CONCRETE RECOMMENDATION FOR FAIRUZ

Given your Fairuz interpreter's design:

### You have a bytecode VM with:
- NaN-boxing (type info available at runtime, not at bytecode level)
- Computed-goto dispatch (instruction selection matters)
- Custom `StringRef` and `Array<T>` (type-aware structures)
- Inline caching (benefits from knowing function purity at compile time)

### Optimization Strategy:

**Definitely do AST-level:**
1. ✅ Constant folding (arithmetic, string, array literals)
2. ✅ Dead code elimination (unreachable branches via control flow analysis)
3. ✅ Algebraic simplification (x * 0 → 0, x + 0 → x, etc.)
4. ✅ Loop strength reduction (induction variables, array indexing)
5. ✅ CSE (common subexpression elimination)
6. ✅ Copy propagation
7. ✅ Function inlining (small pure functions)
8. ✅ Range analysis for bounds checking

**Consider bytecode-level:**
1. ⚠️ Comparison strength reduction (depends on bytecode ops you emit)
2. ⚠️ Peephole optimization (instruction sequence patterns)
3. ⚠️ Dead code elimination (unreachable bytecode)
4. ⚠️ Branch target simplification

**Why this split for Fairuz:**

- **AST-level is where the biggest wins are.** Constant folding, dead code, CSE eliminate entire large expressions before bytecode generation.
- **Bytecode-level is for fine-tuning.** Your interpreter is sophisticated; it can handle well-generated bytecode. The 10-20% gain from peephole optimization isn't worth the complexity if AST-level is solid.
- **Your NaN-boxing VM benefits from fewer instructions.** Smaller bytecode = better cache locality. AST optimizations that reduce code generation (constant folding, loop reduction) matter more than instruction selection tweaks.

---

## 5. IMPLEMENTATION ORDER

### Phase 1: AST-Level (Highest Impact)
```
Week 1-2:
  - Constant folding visitor
  - Dead code elimination
  
Week 3:
  - Algebraic simplification
  - Loop strength reduction
  
Week 4:
  - CSE + copy propagation
  - Range analysis
```

**Expected speedup:** 20-40% on typical programs (constant folding alone often 10-20%)

### Phase 2: Smart Codegen (No new pass, improve existing)
```
Week 5:
  - Improve bytecode generator to recognize optimized AST
  - Example: (x * 4) from loop should emit SHIFT instead of MUL
  - Don't regenerate, just smarter instruction selection
```

**Expected speedup:** 5-10%

### Phase 3: Bytecode-Level (If needed, optional)
```
Week 6-7:
  - Peephole optimizer if profiling shows it's needed
  - Start with comparison strength reduction (easy to implement)
  - Measure before doing more
```

**Expected speedup:** 5-15% (only if codegen isn't already good)

---

## 6. DECISION TREE

**Should comparison strength reduction be AST-level or bytecode-level?**

```
AST-level:
  ├─ Pro: Recognize (x == 0) pattern before codegen
  │       Can emit TEST_ZERO opcode directly
  │       Reduces bytecode size before generation
  │
  ├─ Con: Need to maintain a normalized comparison IR
  │       Must map AST patterns → new AST patterns
  │
  └─ Verdict: Only if you have custom comparison opcodes
             and want to save bytecode size

Bytecode-level:
  ├─ Pro: See actual LOAD x, LOAD 0, COMPARE_EQ sequence
  │       Straightforward peephole matching
  │       Easy to test and verify
  │
  ├─ Con: Depends on codegen producing canonical form
  │       What if codegen is inconsistent?
  │
  └─ Verdict: Easier and safer for Fairuz

Recommendation for Fairuz: BYTECODE-LEVEL
  - Your bytecode is the "interface" to the VM
  - Peephole patterns are predictable
  - You can measure improvement directly
  - Easier to implement incrementally
```

**Why?** Because you already have a custom bytecode VM. The instruction set is yours to control. Bytecode-level peephole is the natural place for micro-optimizations.

---

## 7. DETAILED EXAMPLE: Strength Reduction Location

### Original Source
```cpp
if (x == 0) { ... }
```

### AST-Level Approach

Visitor during AST optimization:
```cpp
class ComparisonOptimizer {
    ASTNode* visit(BinaryOp* cmp) {
        if (cmp->op == EQ && isZeroConstant(cmp->right)) {
            // Transform: x == 0 → TEST_ZERO(x)
            return new UnaryOp(TEST_ZERO, cmp->left);
        }
    }
};
```

Output AST:
```
UnaryOp(TEST_ZERO, Identifier("x"))
```

Bytecode generated:
```
LOAD x
TEST_ZERO           ◄── Directly from optimized AST
POP_JUMP_IF_FALSE
```

**Cost:** 
- Need to define TEST_ZERO node type in AST
- Need codegen support for TEST_ZERO → TEST_ZERO bytecode
- Extra AST visitor pass

**Benefit:**
- Cleaner bytecode before execution
- Smaller bytecode file

---

### Bytecode-Level Approach

Codegen (unchanged, naive):
```cpp
visit(BinaryOp* cmp) {
    emit(LOAD, cmp->left);
    emit(LOAD, cmp->right);
    emit(COMPARE_EQ);
}
```

Bytecode generated:
```
LOAD x
LOAD 0
COMPARE_EQ
POP_JUMP_IF_FALSE
```

Peephole optimizer:
```cpp
class BytecodeOptimizer {
    void optimize() {
        for (int i = 0; i < bytecode.size() - 2; i++) {
            if (bytecode[i] == LOAD_CONST_0 &&
                bytecode[i+1] == COMPARE_EQ) {
                // Pattern: LOAD_CONST_0 followed by COMPARE_EQ
                bytecode[i] = TEST_ZERO;
                bytecode[i+1] = NO_OP;  // Remove next instruction
            }
        }
    }
};
```

Output bytecode:
```
LOAD x
TEST_ZERO
POP_JUMP_IF_FALSE
```

**Cost:**
- Peephole pass after codegen
- Bytecode modification (small overhead)

**Benefit:**
- No AST changes needed
- Codegen stays simple/naive
- Easy to test (bytecode is the source of truth)
- Easy to add more patterns incrementally

---

## 8. BENCHMARKING & PROFILING

**How to decide:**

1. **Profile your current interpreter:**
   ```
   - Run your benchmark suite
   - Identify hot code paths
   - Measure time spent in:
     * Comparisons
     * Arithmetic
     * Memory access
   ```

2. **Implement AST-level first:**
   ```
   - Add constant folding
   - Measure speedup
   - If 15%+ → you found the wins, stop
   - If <10% → look for other bottlenecks
   ```

3. **If AST-level plateaus, then bytecode-level:**
   ```
   - Profile bytecode execution
   - Identify repeated instruction patterns
   - Implement peephole for top-3 patterns
   - Measure each pattern's impact
   ```

---

## 9. FINAL RECOMMENDATION FOR FAIRUZ

### Do This (AST-Level)
```
Priority 1:
  ✅ Constant folding (biggest bang for buck)
  ✅ Dead code elimination
  ✅ Loop strength reduction
  ✅ CSE
  
Priority 2:
  ✅ Range analysis for bounds checking
  ✅ Copy propagation
```

### Do This Only If Profiling Shows It Helps (Bytecode-Level)
```
Priority 3:
  ⚠️ Peephole optimizer
  ⚠️ Comparison strength reduction
  ⚠️ Branch target simplification
```

### Don't Do This
```
❌ Both AST-level AND bytecode-level for the same optimization
   (e.g., constant folding at both levels)
   
❌ Complex inter-procedural analysis at bytecode level
   (too expensive; do at AST level instead)
```

### Why This for Fairuz Specifically

1. **You have a clean AST** (from your parser) → AST optimization is natural
2. **You have a custom bytecode VM** → Bytecode operations are under your control
3. **Your VM is sophisticated** (NaN-boxing, inline caching) → It can handle decently-generated bytecode
4. **Premature optimization kills** → Start with AST, measure, add bytecode optimizations only if needed

---

## Summary Table

| Aspect | AST-Level | Bytecode-Level |
|--------|-----------|-----------------|
| **Semantic understanding** | ✅ Full (types, scope, purity) | ❌ Lost |
| **Scope** | ✅ Global (whole program) | ❌ Local (patterns) |
| **Code size reduction** | ✅ Significant | ⚠️ Modest |
| **Execution speed** | ✅ Large gains | ⚠️ Small gains |
| **Ease of implementation** | ⚠️ Needs AST changes | ✅ Isolated pass |
| **Testing** | ⚠️ Hard (full pipeline) | ✅ Easy (bytecode unit tests) |
| **Information loss** | ❌ None | ✅ Significant |
| **Redundancy risk** | ✅ Low | ⚠️ Could be moot |

---

## Conclusion

**For Fairuz: Hybrid AST + Bytecode-level, but prioritize AST.**

1. **Do AST-level optimizations first** — constant folding, dead code, loop optimization
2. **Improve codegen** — make it smart enough to recognize optimized AST patterns
3. **Add bytecode-level peephole only if profiling requires it** — comparison strength reduction, branch simplification

This gives you 80% of the benefit with 20% of the complexity.