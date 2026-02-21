# NanoScript — Copilot Instructions

This repository contains the NanoScript compiler (C++) and `.nano` source files.

## NanoScript language rules

NanoScript has **one type: int64**. Every variable, literal, and expression is a signed 64-bit integer.

### Syntax at a glance

```
// comment
x = 10;                  // assignment — implicit declaration on first use
y = x + 1;               // arithmetic: + - * /  (integer division)
out y;                   // print int64 to stdout, no parentheses
if (x > 0) { out x; }   // conditional — no else, braces required
```

### All valid operators

| Category | Operators |
|---|---|
| Arithmetic | `+`  `-`  `*`  `/` |
| Comparison | `==`  `!=`  `<`  `>`  `<=`  `>=` |
| Assignment | `=` (simple only) |

Comparisons return int64: `1` = true, `0` = false.

### Keywords

`if`  `out` — these are the only keywords.

### Rules

- Every statement ends with `;`
- Variable names: `[a-zA-Z_][a-zA-Z0-9_]*`
- No type annotations — just assign
- Parentheses group expressions: `result = (x + y) * 2;`
- `if` condition: any expression — zero is false, non-zero is true
- No negative literals — write `0 - n`

### What does NOT exist — never suggest these

```
// NO:
while (x > 0) { }        // no loops
if (x) { } else { }      // no else
fn add(a, b) { }         // no functions
import math               // no imports
x += 1;  x++;            // no compound assignment or increment
x && y   x || y  !x      // no logical operators
x & y    x | y   x ^ y   // no bitwise operators
f32 x = 1.5              // no floats
s = "hello"              // no strings
arr = [1, 2, 3]          // no arrays
return x                  // no return
```

### Example program

```nano
// hello.nano
x = 10;
y = 32;
z = x + y;
out z;           // 42

if (z > 40) {
    out 1;       // 1
}

counter = 1;
if (counter == 1) {
    counter = counter + 1;
    out counter; // 2
}
```

## Building and running

```bash
cmake -B build && cmake --build build

# native binary (default: debug config)
build/nanoscript hello.nano
./hello

# wasm (run with wasmtime)
build/nanoscript hello.nano --wasm
wasmtime hello.wasm

# configs: debug (O0+DWARF), development (O2+DWARF), shipping (O3+LTO)
build/nanoscript hello.nano --config=shipping --wasm
```

## Compiler source (C++)

- `src/lexer.*`   — tokeniser
- `src/ast.hpp`   — AST nodes, NodeKind enum
- `src/parser.*`  — recursive descent parser
- `src/codegen.*` — LLVM IR + DWARF generation
- `src/main.cpp`  — CLI
