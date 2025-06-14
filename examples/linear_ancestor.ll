parent(tom, bob).
parent(bob, liz).
parent(bob, ann).
parent(pat, tom).

ancestor(X, Y) :- parent(X, Y).
ancestor(X, Y) :- parent(X, Z), ancestor(Z, Y).

?- ancestor(pat, liz).
