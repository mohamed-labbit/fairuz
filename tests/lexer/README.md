# Lexer test fixtures

Fixtures under `test_cases/test_tokens/` contain short Arabic snippets that exercise individual token kinds. They are intentionally small so the expected tokens can be asserted directly in the tests.

Notable fixtures:
- `recognizes_plus.txt`, `recognizes_integer.txt`, `recognizes_identifier.txt`, `recognizes_keyword.txt`, `recognizes_string_literal.txt`: Baseline coverage for common token categories.
- `recognizes_none_keyword.txt`, `recognizes_boolean_keywords.txt`: Confirm the Arabic keywords for `none`, `true`, and `false` (`عدم`, `صحيح`, `خطا`) produce the correct keyword tokens.
- `recognizes_expression.txt` and `recognizes_stmt_00`-`04`: Validate full-line lexing with operators, control flow, assignment, and indentation handling.
