# NanoScript — Project Context for Claude Code

## What this project is

NanoScript is a custom compiled language built on LLVM. The compiler is written in C++ and lives in `src/`. It compiles `.nano` source files to either native ARM64 binaries or wasm32-wasi `.wasm` files.

## Building the compiler

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Running the compiler

```bash
# Flags: --config=debug|development|shipping   --wasm (optional)
build/nanoscript <source.nano> [--config=...] [--wasm] [output]

# Examples
build/nanoscript hello.nano                          # debug native binary
build/nanoscript hello.nano --config=debug --wasm    # debug wasm
build/nanoscript hello.nano --config=development     # O2 native with DWARF
build/nanoscript hello.nano --config=shipping        # O3+LTO native, no debug
wasmtime hello.wasm
```

---

## NanoScript Language Reference

### Type system

There is exactly **one type: `int64`** (signed 64-bit integer). There are no other types — no floats, no strings, no booleans, no arrays, no structs. Every variable, literal, and expression is int64.

### Comments

```
// single-line comment only — no block comments
```

### Variables

Variables are declared implicitly on first assignment. No `var`, `let`, or type annotation is needed or allowed. A variable exists for the entire program once assigned anywhere in the file.

```
x = 10;
name = 42;
my_var_2 = x + 1;
```

- Names: `[a-zA-Z_][a-zA-Z0-9_]*`
- Every statement ends with `;`
- Using a variable before assigning it is a compile error

### Literals

Integer literals only. No negative literal syntax — use `0 - n` for negatives.

```
x = 0;
x = 9223372036854775807;   // max int64
x = 0 - 1;                 // -1
```

### Arithmetic operators

All operators work on int64 and return int64. Division is integer (truncates toward zero).

```
a = x + y;
a = x - y;
a = x * y;
a = x / y;    // integer division
```

Operator precedence (highest to lowest): `* /` → `+ -` → comparisons.
Parentheses override precedence.

```
result = (x + y) * 2;
```

### Comparison operators

Comparisons return int64: `1` for true, `0` for false.

```
a = x == y;
a = x != y;
a = x < y;
a = x > y;
a = x <= y;
a = x >= y;
```

### Output

`out` prints a single int64 value to stdout followed by a newline. It is not a function — no parentheses.

```
out x;
out x + y;
out (x * 2) + 1;
```

### Conditionals

`if` with mandatory braces. No `else`. No `else if`. Condition is any expression — zero is false, non-zero is true.

```
if (x > 0) {
    out x;
}

if (x == y) {
    x = x + 1;
    out x;
}
```

The body can contain any number of statements including nested `if` blocks.

### Complete grammar (EBNF)

```
program     = statement* EOF
statement   = assignment | if_stmt | out_stmt
assignment  = IDENT "=" expr ";"
if_stmt     = "if" "(" expr ")" "{" statement* "}"
out_stmt    = "out" expr ";"
expr        = comparison
comparison  = add_sub ( ("==" | "!=" | "<" | ">" | "<=" | ">=") add_sub )*
add_sub     = mul_div ( ("+" | "-") mul_div )*
mul_div     = primary ( ("*" | "/") primary )*
primary     = INT_LITERAL | IDENT | "(" expr ")"
```

### What does NOT exist (do not suggest these)

- No functions or procedures
- No loops (`while`, `for`, `loop`)
- No `else` or `else if`
- No imports or modules
- No strings or characters
- No floating-point numbers
- No arrays, slices, or collections
- No pointers or references
- No type annotations
- No `return` statement
- No `break` or `continue`
- No boolean type (comparisons return int64 0 or 1)
- No negative literals (write `0 - n`)
- No compound assignment (`+=`, `-=`, etc.)
- No increment/decrement (`++`, `--`)
- No bitwise operators
- No logical operators (`&&`, `||`, `!`)

---

## Compiler source layout

```
src/
  lexer.hpp / lexer.cpp      — tokeniser
  ast.hpp                    — AST node types (NodeKind enum dispatch, no RTTI)
  parser.hpp / parser.cpp    — recursive descent parser
  codegen.hpp / codegen.cpp  — LLVM IR + DWARF codegen
  main.cpp                   — CLI entry point
```

Key codegen facts:
- All variables are stack allocas; mem2reg promotes them to SSA registers
- DWARF debug info uses `DIBuilder`; present for debug and development configs
- Wasm target emits a `__main_void` alias required by wasm32-wasi crt1
- Shipping config runs `buildLTODefaultPipeline(O3)` for whole-program optimisation
