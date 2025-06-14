parent(tom, bob).
parent(bob, liz).
parent(pat, tom).

ancestor(X, Y) :- parent(X, Y).
ancestor(X, Y) :- parent(X, Z), ancestor(Z, Y).

?- ancestor(pat, liz).
