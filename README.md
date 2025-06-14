# Linear Logic Compiler

A compiler for a minimal linear logic programming language that generates GC-less, ARC-less C code. This compiler enforces linear logic constraints at compile time, ensuring that resources are used exactly once, which naturally leads to memory-safe, efficient code without garbage collection or automatic reference counting.

## Features

- **Linear Logic Semantics**: Variables must be used exactly once
- **Memory Safety**: Compile-time prevention of memory leaks and double-free errors
- **GC-less Code Generation**: Generates efficient C code without garbage collection
- **Type Checking**: Static type checking with linearity constraints
- **Resource Management**: Explicit memory allocation and deallocation

## Language Features

### Basic Syntax

- **Let bindings**: `let x = value in body`
- **Lambda expressions**: `lam x. body`
- **Function application**: `func(arg)`
- **Conditionals**: `if cond then expr1 else expr2`
- **Binary operations**: `+`, `-`, `*`, `/`, `==`, `<`, `>`, `&&`, `||`

### Linear Logic Features

- **Linear variables**: Each variable must be used exactly once
- **Memory allocation**: `alloc(size)` - creates a linear pointer
- **Memory deallocation**: `free(ptr)` - consumes the linear pointer
- **Memory access**: `load(ptr)` and `store(ptr, value)`
- **Pairs**: `(a, b)` and pattern matching `match pair with (x, y) -> body`

### Function Definitions

```
function_name(parameter) = body
```

## Installation

1. Clone the repository
2. Build with Cargo:
   ```bash
   cargo build --release
   ```

## Usage

### Compile to C

```bash
cargo run -- compile input.ll -o output.c
```

### Compile and generate executable

```bash
cargo run -- compile input.ll -o output.c --executable
```

### Run directly

```bash
cargo run -- run input.ll
```

### Type check only

```bash
cargo run -- check input.ll
```

## Examples

### Simple Linear Computation

```
let x = 42 in x + 1
```

This program binds `x` to 42 and uses it exactly once in the expression `x + 1`.

### Linear Memory Management

```
let ptr = alloc(8) in
let value = 100 in
let _ = store(ptr, value) in
let result = load(ptr) in
let _ = free(ptr) in
result
```

This program:
1. Allocates 8 bytes of memory
2. Stores the value 100 in that memory
3. Loads the value back
4. Frees the memory
5. Returns the loaded value

### Function with Linear Logic

```
double(x) = x + x

let value = 21 in
double(value)
```

**Note**: This would actually fail type checking because `x` is used twice in `x + x`, violating linearity. In a real linear logic system, you'd need to explicitly duplicate the value or use a different approach.

## Type System

The compiler enforces linear logic constraints:

- **Linearity**: Each variable must be used exactly once
- **No implicit copying**: Values cannot be duplicated without explicit operations
- **Resource tracking**: Memory allocations must be paired with deallocations
- **Affine types**: Resources can be discarded (but not duplicated)

## Generated C Code

The compiler generates efficient C code with:

- **Manual memory management**: Explicit malloc/free calls
- **No reference counting**: Direct ownership transfer
- **No garbage collection**: Deterministic memory management
- **Stack-based allocation** for temporaries
- **Linear runtime** for resource management

## Architecture

- **Lexer** (`src/lexer.rs`): Tokenizes the source code
- **Parser** (`src/parser.rs`): Builds an Abstract Syntax Tree
- **Type Checker** (`src/typechecker.rs`): Enforces linear logic constraints
- **Code Generator** (`src/codegen.rs`): Generates C code
- **Runtime** (`c/`): C runtime support for linear operations

## Linear Logic Theory

This compiler is based on linear logic principles:

- **Linear implication** (A ⊸ B): Functions that consume their argument exactly once
- **Multiplicative conjunction** (A ⊗ B): Pairs where both components must be used
- **Additive conjunction** (A & B): Choice where one component is selected
- **Resource awareness**: Explicit tracking of resource usage

## Limitations

This is a minimal implementation with several simplifications:

- Limited type system (mainly integers)
- No polymorphism
- No modules or imports
- Simplified string handling
- Basic error messages

## Future Work

- More sophisticated type system
- Polymorphic types
- Better error messages
- Module system
- Standard library
- Optimization passes
- Garbage collection integration options

## Contributing

This is an educational project demonstrating linear logic compilation techniques. Contributions are welcome, especially:

- Bug fixes
- Better error messages
- Additional language features
- Documentation improvements
- Test cases

## License

MIT License - see LICENSE file for details.
