# Flint

Flint is a linear logic programming language that is statically typed and compiled to C code.
Currently, it is in the most infantile stages of development, and can only produce very basic logic programs using a highly sub-optimal unification engine.

The goal is to utilize a resource-consumption model of data processing that allows for a lean C implementation without any garbage collection or reference counting subprocesses. In theory, the advantage of this approach is minimizing any performance overhead while also maximizing interoperation and reach through the C ecosystem.

## Examples

```prolog
// shopping.fl
// Shopping example - A store selling bread and milk for $1 each
// Demonstrates linear logic resources being consumed during transactions

// Define basic types as subtypes of atom
item :: type of atom.
dollar :: type of atom.
customer :: type of atom.

// Define specific items as distinct types of item
bread :: item.
milk :: item.

// Define specific customers as atoms of customer type
alice :: customer.
bob :: customer.

// Define specific money as atoms of dollar type  
dollar1 :: dollar.
dollar2 :: dollar.
dollar3 :: dollar.

// Define predicates for the store inventory
has_item :: item -> type.
has_money :: customer -> dollar -> type.
wants_item :: customer -> item -> type.

// Define the transaction predicate
can_buy :: customer -> item -> dollar -> type.

// Store inventory - persistent resources (the store keeps restocking)
persistent inv1 :: has_item(bread).
persistent inv2 :: has_item(milk).

// Customer money - linear resources (money gets consumed when spent)
m1 :: has_money(alice, dollar1).
m2 :: has_money(alice, dollar2).  // Alice has $2
m3 :: has_money(bob, dollar3).    // Bob has $1

// Customer preferences
w1 :: wants_item(alice, bread).
w2 :: wants_item(alice, milk).
w3 :: wants_item(bob, bread).

// Transaction rule: A customer can buy an item if they have the money and the store has the item
transaction :: customer -> item -> dollar -> type.

transaction_rule :: 
    has_money($customer, $money) & has_item($item) & wants_item($customer, $item)
    => transaction($customer, $item, $money).

// Test queries
// Alice should be able to buy bread (she has money, wants bread, store has bread)
?- transaction(alice, bread, dollar1).

// Alice should also be able to buy milk with a dollar
?- transaction(alice, milk, dollar2).

// Alice can also buy both
?- transaction(alice, bread, dollar1) & transaction(alice, milk, dollar2).

// What can alice buy?
?- transaction(alice, $item, dollar1).
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
| **amoeba** | Cassowary constraint solver for linear programming | [starwing/amoeba](https://github.com/starwing/amoeba) | Header-only C library |

### Building the Flint Compiler

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd flint-new
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
Use the `--debug` flag on any command for more details.