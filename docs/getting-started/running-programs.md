# Running programs

`fairuz file.fa` executes a source file; `.ف` files also work as entrypoints. Exactly one input file is accepted. Options can precede or follow the path. There is no interactive mode or program-argument forwarding.

| Option | Behavior |
|---|---|
| `--check` | Parse and compile all bodies, including unused functions, without executing the program or imports. |
| `--dump-ast` | Eagerly parse function bodies and print the complete tree. |
| `--dump-bytecode` | Print compiled bytecode. |
| `--time` | Print execution time to standard error. |
| `--diagnostics=json` | Emit structured diagnostics on standard error. |
| `--semantic-tokens` | Emit semantic tokens; `-` reads standard input in this mode. |
| `-h`, `--help`; `-V`, `--version` | Display help or version. |

`fairuz format file.fa` rewrites valid input in place. See [errors](../language/errors.md) and [module loading](../runtime/module-loading.md).
