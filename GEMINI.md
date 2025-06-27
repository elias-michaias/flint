# Project Overview: Flint

Flint is a cutting-edge functional logic programming language with a strong emphasis on linearity (linear types). It is designed to compile efficiently to C, leveraging the performance benefits of compiled code while providing a high-level, expressive programming model. The entire compiler for Flint is implemented in Rust, ensuring type safety, memory safety, and high performance.

## Key Characteristics:

*   **Language Paradigm:** Functional Logic Programming - Combines features from functional programming (e.g., pure functions, immutability) and logic programming (e.g., unification, backtracking, logical variables).
*   **Type System:** Linear Types - A strict type system that ensures resources (memory, file handles, etc.) are used exactly once. This prevents common programming errors like double-freeing or forgetting to close resources, and enables efficient memory management without garbage collection.
*   **Compilation Target:** C - Flint programs are compiled into C source code, which can then be compiled by a standard C compiler into native executables. This provides excellent performance and interoperability with existing C libraries.
*   **Compiler Implementation Language:** Rust - The Flint compiler itself is written in Rust, a systems programming language known for its safety, speed, and concurrency. This choice contributes to a robust and reliable compiler.

## Project Structure:

The Flint project is organized into several key directories, each serving a distinct purpose in the language's development and compilation pipeline.

*   `flint/`: This is the main executable for the Flint compiler. When the Rust project is built, the compiled binary will typically reside here (or in `target/debug` or `target/release`).
*   `Cargo.toml`: The manifest file for the Rust project. It defines project metadata, dependencies, features, and how the project should be built.
*   `Cargo.lock`: A generated file that records the exact versions of all dependencies used in the project, ensuring reproducible builds.
*   `README.md`: The main documentation file for the project, providing a high-level overview, build instructions, and usage examples.
*   `.git/`: The Git version control repository directory, containing all revision history and configuration.
*   `.gitignore`: Specifies intentionally untracked files that Git should ignore (e.g., build artifacts, temporary files, editor backups).
*   `target/`: This directory is created by Cargo (Rust's build system and package manager) and contains all compiled artifacts, including the Flint compiler executable, intermediate build files, and test binaries.

### `src/` (Rust Compiler Source Code):

This directory contains the entire source code for the Flint compiler, written in Rust. Each module handles a specific phase or aspect of the compilation process:

*   `src/ast.rs`: Defines the Abstract Syntax Tree (AST) for the Flint language. This module is responsible for representing the parsed structure of Flint programs in a tree-like data structure.
*   `src/codegen.rs`: Handles the code generation phase. This module takes the Intermediate Representation (IR) and translates it into C source code. It's where the linear types and functional logic constructs are mapped to C equivalents.
*   `src/diagnostic.rs`: Manages error reporting and warnings during compilation. It provides utilities for emitting clear and informative messages to the user about syntax errors, type errors, and other issues.
*   `src/ir.rs`: Defines the Intermediate Representation (IR) of Flint programs. The AST is transformed into this IR, which is a more optimized and compiler-friendly representation before code generation.
*   `src/lexer.rs`: Implements the lexical analysis (tokenization) phase of the compiler. It reads the raw Flint source code and breaks it down into a stream of tokens (e.g., keywords, identifiers, operators, literals).
*   `src/main.rs`: The entry point of the Flint compiler application. It orchestrates the entire compilation pipeline, calling functions from other modules to perform lexing, parsing, type-checking, IR generation, and code generation.
*   `src/package.rs`: Likely handles package management and module resolution for Flint programs, similar to how Rust's Cargo or Python's pip manages dependencies.
*   `src/parser.rs`: Implements the parsing phase. It takes the stream of tokens from the lexer and constructs the Abstract Syntax Tree (AST), ensuring the code adheres to the Flint language's grammar.
*   `src/typechecking.rs`: Performs static type analysis on the Flint program. This crucial module enforces the linear type system, ensuring that resources are used correctly and that the program is type-safe before code generation.

### `runtime/` (C Runtime Library):

This directory contains the C runtime library that Flint programs link against. This runtime provides the low-level primitives and support functions necessary for executing compiled Flint code, especially for features related to functional logic programming and linear types.

*   `runtime/async.c`: Likely contains implementations for asynchronous operations or concurrency primitives, supporting the functional logic paradigm.
*   `runtime/constraint.c`: Implements the constraint satisfaction mechanisms essential for logic programming. This module handles the propagation and solving of constraints.
*   `runtime/environment.c`: Manages the runtime environment, including variable bindings, scope, and context for executing Flint code.
*   `runtime/linear.c`: Provides the core runtime support for linear types, including mechanisms for tracking resource ownership and ensuring single-use semantics.
*   `runtime/list.c`: Implements list manipulation functions, a common data structure in functional programming.
*   `runtime/Makefile`: The build script for the C runtime library. It defines how the various C source files are compiled and linked into a static or dynamic library.
*   `runtime/matching.c`: Implements pattern matching logic, a fundamental feature in functional and logic programming for destructuring data and dispatching based on structure.
*   `runtime/narrowing.c`: Likely related to narrowing in logic programming, a technique for finding solutions to equations by specializing terms.
*   `runtime/runtime.c`: Contains general runtime utility functions and the main entry point for the compiled Flint program's execution.
*   `runtime/runtime.h`: The main header file for the runtime library, declaring functions and data structures that compiled Flint code will use.
*   `runtime/types.h`: Defines the C representations of Flint's data types.
*   `runtime/unification.c`: Implements the core unification algorithm, a cornerstone of logic programming for solving equations between terms.
*   `runtime/.deps/`: Directory for build dependencies generated by the Makefile.
*   `runtime/interop/`:
    *   `runtime/interop/c.c`: Likely handles interoperability between Flint's runtime and raw C code, allowing Flint programs to call C functions and vice-versa.
*   `runtime/lib/`: Contains external libraries that the Flint runtime depends on.
    *   `runtime/lib/amoeba/`: Potentially a custom or third-party library for specific runtime functionalities.
    *   `runtime/lib/libdill/`: Contains the `libdill` library, a structured concurrency library for C. This suggests Flint's runtime leverages `libdill` for its asynchronous and concurrent features.
        *   `runtime/lib/libdill-2.14/`: Source code for `libdill` version 2.14.
        *   `runtime/lib/libdill-install/`: Installation directory for `libdill`.
*   `runtime/object/`: Likely contains object files (`.o`) generated during the compilation of the C runtime.
*   `runtime/out/`: Output directory for runtime build artifacts.
    *   `runtime/out/libdill_objects/`: Object files specifically for `libdill`.
*   `runtime/test/`:
    *   `runtime/test/test_runtime.c`: Unit tests for the C runtime library, ensuring the correctness of its low-level functions.

### `examples/`:

This directory holds example Flint programs, demonstrating the language's features and usage.

*   `examples/python_test.fl`: An example Flint program, likely showcasing interoperability or specific features relevant to Python (perhaps through FFI or a binding).
*   `examples/unification_test.fl`: An example Flint program specifically designed to test and demonstrate the unification capabilities of the language.
*   `examples/.flint/`: This directory likely contains compiled artifacts, intermediate files, or libraries generated when compiling the example Flint programs. It might serve as a cache or a staging area for example-specific builds.

## Compilation Process:

The compilation of a Flint program to an executable involves several stages:

1.  **Lexing:** The `src/lexer.rs` module reads the `.fl` source file and converts it into a stream of tokens.
2.  **Parsing:** The `src/parser.rs` module takes the token stream and constructs an Abstract Syntax Tree (AST) (`src/ast.rs`), verifying syntax.
3.  **Type Checking:** The `src/typechecking.rs` module analyzes the AST to ensure type correctness, especially enforcing the linear type rules.
4.  **Intermediate Representation (IR) Generation:** The AST is transformed into a more optimized Intermediate Representation (`src/ir.rs`).
5.  **Code Generation:** The `src/codegen.rs` module translates the IR into C source code.
6.  **C Compilation:** The generated C code, along with the Flint C runtime library (`runtime/`), is compiled by a standard C compiler (e.g., GCC, Clang) into a native executable.

## Testing Strategy:

The project employs a multi-layered testing strategy to ensure the correctness of both the compiler and the runtime:

*   **Rust Unit/Integration Tests:** The Rust compiler modules (`src/`) will have associated unit and integration tests (typically in `src/module_name/tests/` or `tests/` directory in the root, though not explicitly shown in the provided structure). These tests verify the correctness of lexing, parsing, type-checking, IR generation, and code generation.
*   **C Runtime Tests:** The `runtime/test/test_runtime.c` file contains unit tests for the C runtime library, ensuring that the low-level primitives for linear types, unification, constraints, and concurrency work as expected.
*   **Example-based Testing:** The `examples/` directory serves as a form of integration testing, where actual Flint programs are compiled and executed to verify the end-to-end correctness of the compiler and runtime.

## Build Process:

*   **Rust Compiler:** The Rust compiler is built using Cargo. Running `cargo build` will compile the `flint` executable.
*   **C Runtime:** The C runtime library is built using `make` within the `runtime/` directory, as indicated by the `runtime/Makefile`. This Makefile handles the compilation of C source files and linking with external libraries like `libdill`.
