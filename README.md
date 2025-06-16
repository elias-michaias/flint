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