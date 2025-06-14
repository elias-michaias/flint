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
