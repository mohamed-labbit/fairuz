# Structural interpreter and standard-library regressions

`discovered_failures.inc` contains 73 process-isolated regression cases for existing Fairuz behavior. The ordinary GoogleTest suite runs each source with a five-second timeout. These are correctness checks, not a backlog of deliberately failing feature requests.

## Reproduce

```sh
cmake --build build --target fairuz fairuz_tests -j4
./build/fairuz_tests --gtest_filter='DiscoveredFailures/*'
```

## Retained coverage

- Numeric literals, exponentiation, and NaN equality.
- NUL preservation in Unicode construction, concatenation, JSON, and binary file reads.
- Character-based string slicing, consistent with indexing and substring operations; invalid UTF-8 byte strings retain byte slicing.
- Calls through existing module values and callable fields, including import-name shadowing. Named methods retain priority when a field has the same name.
- Nil, boolean, and reference identity equality.
- Deep copying without merging distinct equal containers, while preserving shared children and cycles. The standard library uses an internal identity primitive for its existing memoization algorithm.
- Fraction reduction and sign normalization without colliding with implicit field assignments in constructors.
- JSON number grammar and CSV field validation and preservation.
- Approximate assertions rejecting unordered NaN operands.

## Removed expectations

The September 2026 review removed 29 cases at the user's request to exclude opinionated semantics and unimplemented features:

- **15 integer-to-float rounding cases** (`FloatRoundOnce*`, `FloatComparisonAfterConversion*`) prescribed Python's exact nearest-even conversion results for arbitrary-size integers. The current integer contract guarantees exact integer operations and comparisons, but does not specify this conversion rounding policy.
- **12 integer-division rounding cases** (`DivisionRoundOnce*`) required a particular correctly rounded binary64 quotient. The contract does not prescribe that rounding mode. Several also compared a rounded floating result with a decimal integer that was not exactly representable, imposing an additional mixed-comparison expectation.
- **2 test-runner cases** (`StdlibAssertionCatchesRuntimeError`, `StdlibTestSuiteExecutes`) depended on the absent `__اختبار_التقط__` and `__اختبار_نفذ__` native hooks. Implementing a catch boundary and test runner would be new runtime functionality.

Existing integer arithmetic/oracle tests remain enabled. Tests for ordinary assertions and the current standard-library modules remain enabled. Four additional regressions cover primitive/reference equality and preservation of shared children and cycles during deep copy.
