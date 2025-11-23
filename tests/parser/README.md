# Parser test cases

This directory holds the parser-focused fixtures used across unit tests. Each file under `test_cases/` is a minimal snippet designed to exercise a single parsing branch:

- `number_literal.txt` / `string_literal.txt` / `boolean_literal_true.txt` / `boolean_literal_false.txt`: Ensure primitive literal tokens are parsed into the correct `LiteralExpr` variants.
- `identifier.txt`: Provides a bare identifier for tests that construct `NameExpr` instances.
- `unary_minus.txt`: Covers unary operator handling when parsing numeric literals.
- `none_literal.txt`: Exercises parsing of the Arabic `عدم` keyword into a `NONE` literal.
- `parenthesized_number.txt`: Verifies parenthesized expressions round-trip through `parsePrimary` without altering the contained literal.
