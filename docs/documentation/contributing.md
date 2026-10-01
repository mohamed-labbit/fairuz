# Maintaining this documentation

Update docs with the language implementation, not from analogy to another language. Inspect lexer tokens, parser branches, compiler handling, VM operations, builtin registration, stdlib source, and tests. Label behavior as **Language behavior**, **Current implementation**, or **Unresolved** when its status matters. Keep exact syntax and preserve Arabic source bytes; do not add invisible direction characters to executable examples.

After changing `stdlib/*.ف`, run `python3 docs/documentation/generate-module-reference.py` to refresh the declaration inventory, then edit semantic descriptions for changed symbols. The generator only extracts declarations and a few body clues; it cannot verify behavior. Update [coverage](coverage.md) and [unresolved](unresolved.md) with evidence.

```sh
python3 -m pip install mkdocs pygments
python3 -m mkdocs build --strict
python3 -m mkdocs serve
python3 docs/documentation/validate.py --lang en
python3 docs/documentation/validate.py --lang ar
python3 -m mkdocs build --strict -f mkdocs_ar.yml
```

The MkDocs build writes to `site/`; it is local only. `serve` exposes a local preview. Inspect the index, grammar, a module page, and a mixed Arabic/Latin code block in a browser, then copy the code back into a UTF-8 file and run it. Run the C++ and standard-library suites with `ctest --test-dir build --output-on-failure` when changes affect behavior.

When changing syntax, update both language references and their `language/grammar.ebnf` copies, the root grammar, and the README. Keep inline-body examples in `validate.py` so documented syntax and output are checked in both languages.
