# Parser fixtures missing C++ test references

This checklist records fixture files whose filenames do not occur in the C++ source/header files under `tests/`. The scan includes the `test_tokens/` subdirectory. It is a reference inventory, not a claim that the corresponding syntax has no inline tests elsewhere.

- Total `.fa` files scanned: **180**.
- Missing C++ filename references: **120**.
- New fixtures awaiting tests: **110**.
- Previously existing fixtures awaiting tests: **10**.
- The two `.inc` datasets are already referenced by interpreter/regression tests and are not included below.

No C++ tests were added or changed for these fixtures. Every unchecked row below still needs a C++ test reference.

## How to turn these fixtures into tests

- For parser fixtures, use `Parser(&file, BodyParsing::Eager)` and `parse_program()`. Lazy parsing can defer the malformed function body in `invalid_function_body.fa`.
- Set/reset the diagnostic source and engine for each case. For valid input, assert no diagnostics, then inspect the AST rather than relying only on successful parsing.
- The statement counts below are intended top-level AST counts. Nested body statements are not counted there; the latest fixture revision has not been run.
- New literal and expression fixtures put the construct on the right-hand side of `:=`. Inspect the expression statement's `AssignExpr`, then its `value`, when asserting the literal or expression AST. Deliberately malformed assignment targets remain invalid by design.
- An error may produce a partial AST. Assert its diagnostic code/location, not merely an empty AST.
- Five lexical-error fixtures raise `diagnostic::DiagnosticAbort` during tokenization/parsing. Catch or assert that exception before checking the diagnostic.
- Import fixtures test syntax only: they do not require those modules to resolve. Other fixtures also intentionally contain unbound identifiers. Do not execute them as programs to determine parser validity.
- Parenthesized sequences currently produce list-like AST nodes, not a distinct tuple type.

## New valid fixtures — 84 missing references

These fixtures are intended to parse without diagnostics. For each row, add the file reference and AST assertions for the named construct. The assignment-form revision has not been tested.

| C++ test added | Fixture | Top-level statements | Construct / assertions to add |
|---|---|---:|---|
| [ ] | [arabic_numeric_literals.fa](arabic_numeric_literals.fa) | 2 | Arabic digits in integer and floating-point literals |
| [ ] | [assert_english_message.fa](assert_english_message.fa) | 1 | English assert keyword with a message |
| [ ] | [assert_english.fa](assert_english.fa) | 1 | English assert keyword without a message |
| [ ] | [assert_message.fa](assert_message.fa) | 1 | Arabic assertion with an optional message expression |
| [ ] | [assert_statement.fa](assert_statement.fa) | 1 | Arabic assertion statement without a message |
| [ ] | [augmented_assign_arabic_percent.fa](augmented_assign_arabic_percent.fa) | 1 | Arabic percent augmented assignment |
| [ ] | [augmented_assign_bitand.fa](augmented_assign_bitand.fa) | 1 | Bitwise AND augmented assignment |
| [ ] | [augmented_assign_bitor.fa](augmented_assign_bitor.fa) | 1 | Bitwise OR augmented assignment |
| [ ] | [augmented_assign_bitxor.fa](augmented_assign_bitxor.fa) | 1 | Bitwise XOR augmented assignment |
| [ ] | [augmented_assign_index.fa](augmented_assign_index.fa) | 1 | Augmented assignment to an indexed target |
| [ ] | [augmented_assign_lshift.fa](augmented_assign_lshift.fa) | 1 | Left-shift augmented assignment |
| [ ] | [augmented_assign_member.fa](augmented_assign_member.fa) | 1 | Augmented assignment to a member target |
| [ ] | [augmented_assign_rshift.fa](augmented_assign_rshift.fa) | 1 | Right-shift augmented assignment |
| [ ] | [binary_literal.fa](binary_literal.fa) | 1 | Binary integer literal |
| [ ] | [bitwise_expression.fa](bitwise_expression.fa) | 3 | Binary bitwise AND, OR and XOR |
| [ ] | [bitwise_not.fa](bitwise_not.fa) | 2 | Bitwise negation |
| [ ] | [call_multiline.fa](call_multiline.fa) | 1 | Multiline call arguments |
| [ ] | [call_trailing_comma.fa](call_trailing_comma.fa) | 1 | Trailing comma in call arguments |
| [ ] | [class_constructor_this.fa](class_constructor_this.fa) | 1 | Constructor and explicit this-field assignment/read |
| [ ] | [class_definition.fa](class_definition.fa) | 1 | Indented class with a method |
| [ ] | [class_field_shorthand_augmented.fa](class_field_shorthand_augmented.fa) | 1 | Leading-dot augmented assignment inside a method |
| [ ] | [class_field_shorthand.fa](class_field_shorthand.fa) | 1 | Leading-dot field assignment inside a method |
| [ ] | [class_inheritance.fa](class_inheritance.fa) | 2 | Class with a parent identifier |
| [ ] | [class_multiple_methods.fa](class_multiple_methods.fa) | 1 | Multiple sibling methods with inline and indented bodies |
| [ ] | [class_operator_arabic_percent.fa](class_operator_arabic_percent.fa) | 1 | Arabic percent spelling in an operator-named method |
| [ ] | [class_operator_methods.fa](class_operator_methods.fa) | 1 | Operator-named methods for all five supported arithmetic operators |
| [ ] | [comments.fa](comments.fa) | 2 | Full-line, trailing and indented comments |
| [ ] | [comparison_greater_equal.fa](comparison_greater_equal.fa) | 1 | Greater-than-or-equal comparison |
| [ ] | [comparison_less_equal.fa](comparison_less_equal.fa) | 1 | Less-than-or-equal comparison |
| [ ] | [continuation_indentation.fa](continuation_indentation.fa) | 2 | Uneven indentation inside delimiters must not introduce block boundaries |
| [ ] | [dictionary_computed_keys.fa](dictionary_computed_keys.fa) | 1 | Expression-valued dictionary keys |
| [ ] | [dictionary_empty.fa](dictionary_empty.fa) | 1 | Empty dictionary literal |
| [ ] | [dictionary_literal.fa](dictionary_literal.fa) | 1 | Dictionary literal with multiple entries |
| [ ] | [dictionary_multiline.fa](dictionary_multiline.fa) | 1 | Multiline dictionary entries |
| [ ] | [dictionary_nested.fa](dictionary_nested.fa) | 1 | Nested dictionaries and collection values |
| [ ] | [dictionary_trailing_comma.fa](dictionary_trailing_comma.fa) | 1 | Trailing comma in a dictionary |
| [ ] | [float_literal.fa](float_literal.fa) | 1 | Floating-point literal in a parser fixture |
| [ ] | [function_multiple_parameters.fa](function_multiple_parameters.fa) | 1 | Function with multiple parameters |
| [ ] | [function_nested.fa](function_nested.fa) | 1 | Nested function definitions |
| [ ] | [function_no_parameters.fa](function_no_parameters.fa) | 1 | Function definition with an empty parameter list |
| [ ] | [hex_literal.fa](hex_literal.fa) | 1 | Hexadecimal integer literal |
| [ ] | [if_else_if.fa](if_else_if.fa) | 1 | Else-if chain followed by else |
| [ ] | [if_else.fa](if_else.fa) | 1 | Indented if/else branches |
| [ ] | [import_dotted_module.fa](import_dotted_module.fa) | 2 | Dotted module paths with default and explicit aliases |
| [ ] | [import_from_alias.fa](import_from_alias.fa) | 1 | Aliased member import |
| [ ] | [import_from_member.fa](import_from_member.fa) | 1 | Single member import |
| [ ] | [import_from_multiline.fa](import_from_multiline.fa) | 1 | Parenthesized multiline imports with a trailing comma |
| [ ] | [import_from_multiple.fa](import_from_multiple.fa) | 1 | Multiple imported names with individual aliases |
| [ ] | [import_module_alias.fa](import_module_alias.fa) | 1 | Whole-module import with an alias |
| [ ] | [import_module.fa](import_module.fa) | 1 | Whole-module import without an alias |
| [ ] | [index_assignment.fa](index_assignment.fa) | 1 | Index-write assignment target |
| [ ] | [index_expression.fa](index_expression.fa) | 1 | Expression-valued index |
| [ ] | [index_nested.fa](index_nested.fa) | 1 | Repeated indexing on assignment targets and values |
| [ ] | [index_read.fa](index_read.fa) | 1 | Index-read expression |
| [ ] | [inline_class.fa](inline_class.fa) | 2 | Inline class and method body |
| [ ] | [inline_else_binding.fa](inline_else_binding.fa) | 2 | Inline nested if must leave aligned else to the outer if |
| [ ] | [inline_for.fa](inline_for.fa) | 2 | Inline for body and following statement |
| [ ] | [inline_function.fa](inline_function.fa) | 2 | Inline function body and following statement |
| [ ] | [inline_if.fa](inline_if.fa) | 2 | Inline if/else bodies and following statement |
| [ ] | [inline_while.fa](inline_while.fa) | 2 | Inline while body and following statement |
| [ ] | [list_multiline.fa](list_multiline.fa) | 1 | Multiline list literal |
| [ ] | [logical_not.fa](logical_not.fa) | 2 | Logical negation and its precedence with logical operators |
| [ ] | [member_assignment.fa](member_assignment.fa) | 1 | Member-write assignment target |
| [ ] | [member_read.fa](member_read.fa) | 1 | Member-read expression |
| [ ] | [method_call.fa](method_call.fa) | 1 | Member access followed by a call |
| [ ] | [modulo_expression.fa](modulo_expression.fa) | 2 | ASCII and Arabic modulo operators |
| [ ] | [nested_if_else.fa](nested_if_else.fa) | 1 | Nested if/else ownership |
| [ ] | [numeric_separators.fa](numeric_separators.fa) | 2 | Digit separators in integers and fractional literals |
| [ ] | [octal_literal.fa](octal_literal.fa) | 1 | Octal integer literal |
| [ ] | [operator_precedence.fa](operator_precedence.fa) | 3 | Mixed arithmetic, shift, bitwise, comparison and logical precedence |
| [ ] | [parameters_multiline.fa](parameters_multiline.fa) | 1 | Multiline function parameters |
| [ ] | [parameters_trailing_comma.fa](parameters_trailing_comma.fa) | 1 | Trailing comma in function parameters |
| [ ] | [parenthesized_sequence.fa](parenthesized_sequence.fa) | 1 | Nonempty parenthesized sequence produces a list-like AST |
| [ ] | [parenthesized_singleton.fa](parenthesized_singleton.fa) | 1 | Singleton parenthesized sequence with a trailing comma |
| [ ] | [postfix_chain.fa](postfix_chain.fa) | 2 | Mixed call, index, member and repeated-call chaining |
| [ ] | [power_expression.fa](power_expression.fa) | 1 | Exponentiation expression |
| [ ] | [power_right_associative.fa](power_right_associative.fa) | 1 | Right-associative exponentiation |
| [ ] | [power_unary_precedence.fa](power_unary_precedence.fa) | 3 | Power precedence with negation and negative exponents |
| [ ] | [return_bare.fa](return_bare.fa) | 1 | Bare return in a function body |
| [ ] | [return_value.fa](return_value.fa) | 1 | Return with an expression |
| [ ] | [shift_expression.fa](shift_expression.fa) | 2 | Left and right shifts |
| [ ] | [single_quoted_string.fa](single_quoted_string.fa) | 1 | Single-quoted UTF-8 string literal |
| [ ] | [string_escapes.fa](string_escapes.fa) | 2 | Supported string escapes including Unicode and escaped quotes |
| [ ] | [unary_plus.fa](unary_plus.fa) | 2 | Unary plus on a literal and an expression |

## New deliberate error fixtures — 25 missing references

Diagnostic names below are expectations based on the earlier fixture versions, before the assignment-form revision. They have not been rechecked after that revision. Additional recovery diagnostics may follow. All files are intended to be rejected; five are lexical abort cases.

| C++ test added | Fixture | Expected first diagnostic | Throws DiagnosticAbort | What to verify |
|---|---|---|---|---|
| [ ] | [invalid_assignment_in_expression.fa](invalid_assignment_in_expression.fa) | `ASSIGNMENT_IN_EXPRESSION` | No | Assignment cannot appear as a call argument |
| [ ] | [invalid_assignment_target.fa](invalid_assignment_target.fa) | `INVALID_ASSIGN_TARGET` | No | A literal cannot be an assignment target |
| [ ] | [invalid_binary_digit.fa](invalid_binary_digit.fa) | `INVALID_BINARY_DIGIT` | Yes | Invalid digit in a binary literal |
| [ ] | [invalid_class_missing_colon.fa](invalid_class_missing_colon.fa) | `EXPECTED_COLON_CLASS` | No | Class header missing its colon |
| [ ] | [invalid_dictionary_closing_brace.fa](invalid_dictionary_closing_brace.fa) | `UNCLOSED_DELIMITER` | No | Dictionary missing its closing brace |
| [ ] | [invalid_dictionary_colon.fa](invalid_dictionary_colon.fa) | `EXPECTED_COLON_DICT` | No | Dictionary entry missing its colon |
| [ ] | [invalid_function_body.fa](invalid_function_body.fa) | `UNEXPECTED_TOKEN` | No | Invalid syntax inside a function requires eager parsing |
| [ ] | [invalid_function_missing_colon.fa](invalid_function_missing_colon.fa) | `EXPECTED_COLON_FN` | No | Function header missing its colon |
| [ ] | [invalid_function_name.fa](invalid_function_name.fa) | `EXPECTED_FN_NAME` | No | Missing function name |
| [ ] | [invalid_hex_literal.fa](invalid_hex_literal.fa) | `INVALID_BASE_LITERAL` | Yes | Hexadecimal prefix without digits |
| [ ] | [invalid_import_alias.fa](invalid_import_alias.fa) | `EXPECTED_ALIAS_NAME` | No | Missing alias after باسم |
| [ ] | [invalid_import_member.fa](invalid_import_member.fa) | `EXPECTED_IMPORT_NAME` | No | Missing imported member name |
| [ ] | [invalid_import_module.fa](invalid_import_module.fa) | `EXPECTED_MODULE_NAME` | No | Missing module name |
| [ ] | [invalid_import_parenthesis.fa](invalid_import_parenthesis.fa) | `UNCLOSED_DELIMITER` | No | Parenthesized imports missing their closing delimiter |
| [ ] | [invalid_index_expression.fa](invalid_index_expression.fa) | `UNEXPECTED_TOKEN` | No | Subscript missing its index expression |
| [ ] | [invalid_inline_extra_statement.fa](invalid_inline_extra_statement.fa) | `UNEXPECTED_TOKEN` | No | Inline suite contains an extra same-line statement |
| [ ] | [invalid_member_name.fa](invalid_member_name.fa) | `EXPECTED_MEMBER_NAME` | No | Member access missing an identifier after the dot |
| [ ] | [invalid_mismatched_delimiter.fa](invalid_mismatched_delimiter.fa) | `MISMATCHED_DELIMITER` | No | Mismatched closing delimiter |
| [ ] | [invalid_missing_block.fa](invalid_missing_block.fa) | `EXPECTED_INDENT` | No | Compound header missing an indented body |
| [ ] | [invalid_octal_digit.fa](invalid_octal_digit.fa) | `INVALID_OCTAL_DIGIT` | Yes | Invalid digit in an octal literal |
| [ ] | [invalid_parameter_name.fa](invalid_parameter_name.fa) | `EXPECTED_PARAM_NAME` | No | Non-identifier function parameter |
| [ ] | [invalid_parent_class.fa](invalid_parent_class.fa) | `EXPECTED_CLASS_NAME` | No | Parent class must be an identifier |
| [ ] | [invalid_string_escape.fa](invalid_string_escape.fa) | `INVALID_ESCAPE_SEQUENCE` | Yes | Unsupported string escape |
| [ ] | [invalid_unterminated_string.fa](invalid_unterminated_string.fa) | `UNTERMINATED_STRING` | Yes | String literal missing its closing quote |
| [ ] | [recovery_multiple_errors.fa](recovery_multiple_errors.fa) | `UNEXPECTED_TOKEN` | No | Independent syntax errors with valid statements between and after them Also assert two diagnostics and retained assignments وسط and نهاية, in that order. |

## Known unsupported syntax — 1 missing reference

- [ ] [scientific_literal.fa](scientific_literal.fa) assigns `1e3`, `2.5e+2`, and `4.2e-3` to identifiers. Earlier probing of the standalone expressions found scientific notation unsupported, with `UNEXPECTED_TOKEN` diagnostics. The assignment-form revision has not been run; its exact diagnostic count is unverified. Add an explicit rejection test for current behavior, or implement the feature before expecting successful numeric ASTs. Do not silently treat this as a valid fixture.

## Previously existing fixtures — 10 missing references

These files were already present and were not edited. Their current eager-parsing behavior was checked separately.

| C++ test added | Fixture | Current diagnostics | Top-level statements | Follow-up |
|---|---|---:|---:|---|
| [ ] | [chained_comparison.fa](chained_comparison.fa) | 0 | 1 | Assert the current nested binary-comparison AST; comparison chaining has no separate AST node. |
| [ ] | [empty_input.fa](empty_input.fa) | 0 | 0 | Empty program: no statements and no errors. |
| [ ] | [invalid_syntax.fa](invalid_syntax.fa) | 1 | 0 | Unary operators without an operand: expect UNEXPECTED_EOF. |
| [ ] | [large_file.fa](large_file.fa) | 0 | 5000 | Expect 5,000 expression statements containing equality comparisons. The file uses =, not :=. |
| [ ] | [missing_operand.fa](missing_operand.fa) | 1 | 0 | Missing right operand: expect UNEXPECTED_EOF. |
| [ ] | [test_file.fa](test_file.fa) | 2 | 1 | Contains obsolete النتيجة spelling: eager parsing currently reports two UNEXPECTED_TOKEN diagnostics. Modernize separately or make its error expectation explicit. |
| [ ] | [unary_minus.fa](unary_minus.fa) | 0 | 1 | Expect a unary-negation AST around integer 5. |
| [ ] | [unexpected_eof.fa](unexpected_eof.fa) | 1 | 0 | Incomplete binary expression: expect UNEXPECTED_EOF. |
| [ ] | [whitespace_only.fa](whitespace_only.fa) | 0 | 0 | Whitespace-only program: no statements and no errors. |

`test_indentation.fa` was retired as an empty duplicate of `empty_input.fa`.

## Validation boundary

No tests, compiler runs, or parser probes were run for the latest assignment-form revision, at the user's request. Earlier probing applied to the previous fixture contents, not to this revision. The statement counts and diagnostic expectations above are guidance for writing the missing C++ tests, not a claim that those tests pass. The reference counts describe the earlier filename inventory; the 120 C++ test references and their AST assertions remain work for the checklist above.
