# Flint

Flint is a linear logic programming language that is statically typed and compiled to C code.
Currently, it is in the most infantile stages of development, and can only produce very basic logic programs using a highly sub-optimal unification engine.

The goal is to utilize a resource-consumption model of data processing that allows for a lean C implementation without any garbage collection or reference counting subprocesses. In theory, the advantage of this approach is minimizing any performance overhead while also maximizing interoperation and reach through the C ecosystem.

## Examples

```prolog
// ancestor.fl

// Define type
person :: type.

// Term type declarations
tom, bob, liz, pat :: person.

// Predicate type declarations
parent :: person -> person -> type.
ancestor :: person -> person -> type.

// Facts with typed terms
!parent(tom, bob).
!parent(bob, liz).
!parent(pat, tom).

// Rules
ancestor(X, Y) :- !parent(X, Y).
ancestor(X, Y) :- parent(X, Z), !ancestor(Z, Y).

// Query
?- ancestor(tom, liz).
?- ancestor(pat, liz).
```

```prolog
// vending.fn
coin :: type.                
c1, c2 :: coin.

item :: type.
soda :: item.
chips :: item.

has :: item -> type.          
buy :: item -> type. 

// Consume resources
buy(soda) :- coin, has(soda).
buy(chips) :- coin, coin, has(chips).

?- buy(soda).
?- buy(chips).
?- buy(soda), buy(soda).

// Fails: not enough coins
?- buy(chips), buy(soda).
```

## Usage
Run files directly:
`cargo run -- run flint_file.fn`
Build files:
`cargo run -- build flint_file.fn`
Check files:
`cargo run -- check flint_file.fn`