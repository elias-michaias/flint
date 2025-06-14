fuel(tank1).
fuel(tank2).

use_fuel(X) :- fuel(X).

?- use_fuel(tank1).
