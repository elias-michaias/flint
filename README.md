# Flint

Flint is a functional logic programming language that is statically typed and compiled to C code. It has a strong focus on robust, controlled C interop.
It enforces linearity at the language level with opt-in exponentiation.
Currently, it is in the most infantile stages of development, and can only produce very basic programs using a highly sub-optimal unification algorithm.

The goal is to utilize a resource-consumption model of data processing that allows for a lean C implementation without any garbage collection or reference counting subprocesses. In theory, the advantage of this approach is minimizing any performance overhead while also maximizing interoperation and reach through the C ecosystem.

## Examples

```
// automatically resolves C imports
// becomes submodule scoped under C effect
import C "stdio.h" as stdio

// automatically resolves pypi dependencies
// does type conversion and runs using Python C API
import Python "pypi::numpy::2.3.1" as numpy

add :: (i32, i32) -> i32
add :: ($x, $y) => $x + $y

add :: (i32, i32) -> i32
add :: ($x, $y) => $x + $y

// C, Python = algebraic effects
main :: () -> () using C, Python
main :: () => { 
    let add($z, 7) = 11
    let add($a, $z) = 15
    let $res = Python.numpy.power(~$a, ~$z)
    C.stdio.printf("Result: a = %d, z = %d, a^z = %d\n", $a, $z, $res)
    // Result: a = 11, z = 4, a^z = 14641
}
```

## Build Instructions

### Prerequisites

- **Rust**: Install from [rustup.rs](https://rustup.rs/)
- **C Compiler**: GCC or Clang (for runtime compilation)
- **Make**: For building the runtime
- **curl**: For automatic dependency fetching

### Dependencies

The Flint runtime automatically downloads and builds the following dependencies:

| Dependency | Purpose | URL | Type |
|------------|---------|-----|------|
| **libdill** | Structured concurrency and async operations | [libdill.org](http://libdill.org/libdill-2.14.tar.gz) | C library (built) |
| **amoeba** | Cassowary constraint solver | [starwing/amoeba](https://github.com/starwing/amoeba) | Header-only C library |

### Building the Flint Compiler

1. Clone the repository:
   ```bash
   git clone https://github.com/elias-michaias/flint
   cd flint
   ```

2. Build the Rust compiler:
   ```bash
   cargo build --release
   ```

### Building the Runtime

The Flint runtime is written in C and provides:
- Unification and constraint solving with the amoeba constraint solver
- Linear resource management  
- Async operations with structured concurrency (libdill)
- C interoperability
- Pattern matching and narrowing

1. Navigate to the runtime directory:
   ```bash
   cd runtime
   ```

2. **Check dependency status** (optional):
   ```bash
   make list-deps
   ```

3. **Build the runtime library** (dependencies are automatically fetched):
   ```bash
   make
   ```
   
   The first build will automatically:
   - Download and build libdill for structured concurrency
   - Download amoeba header files for constraint solving
   - Create status tracking files in `.deps/`

4. **Run the test suite** to verify everything works:
   ```bash
   make test
   ```

5. **Clean build artifacts**:
   ```bash
   make clean              # Clean compiled objects and binaries
   make clean-deps         # Clean downloaded dependencies  
   make clean-all          # Clean everything
   ```

### Dependency Management

The runtime build system automatically handles all dependencies:

- **Automatic fetching**: Dependencies are downloaded on first build
- **Smart caching**: Dependencies are only re-downloaded if missing
- **Status tracking**: Use `make list-deps` to see dependency status
- **Clean rebuilds**: Use `make clean-deps` to force fresh dependency downloads

You don't need to manually install any dependencies - the Makefile handles everything automatically.

### Development

For development, you can use:
- `make debug` - Build with debug symbols and no optimization
- `make profile` - Build with profiling support
- `make deps` - Explicitly fetch/build dependencies without building runtime

## Usage
Run files directly:
```bash
cargo run -- run flint_file.fl
```
Build files:
```bash
cargo run -- build flint_file.fl
```
Check files:
```bash
cargo run -- check flint_file.fl
```
`se the `--debug` flag on any command for more details.