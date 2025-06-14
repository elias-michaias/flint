# Flint

Flint is an linear logic programming language that is statically typed and compiled to C code.
Currently, it is in the most infantile stages of development, and can only produce very basic logic programs using a highly sub-optimal unification implementation.
```prolog
// ancestor.fn

// Define type
person :: type.

// Term type declarations
tom, bob, liz, pat :: person.

// Predicate type declarations
parent :: person -> person -> type.
ancestor :: person -> person -> type.

// Facts with typed terms
parent(tom, bob).
parent(bob, liz).
parent(pat, tom).

// Rules
ancestor(X, Y) :- parent(X, Y).
ancestor(X, Y) :- parent(X, Z), ancestor(Z, Y).

// Query
?- ancestor(tom, liz).
?- ancestor(pat, liz).
```

## Usage
Run files directly:
`cargo run -- run flint_file.fn`
Build files:
`cargo run -- build flint_file.fn`
Check files:
`cargo run -- check flint_file.fn`