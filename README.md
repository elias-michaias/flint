# Flint

Flint is a linear logic programming language that is statically typed and compiled to C code.
Currently, it is in the most infantile stages of development, and can only produce very basic logic programs using a highly sub-optimal unification engine.

The goal is to utilize a resource-consumption model of data processing that allows for a lean C implementation without any garbage collection or reference counting subprocesses. In theory, the advantage of this approach is minimizing any performance overhead while also maximizing interoperation and reach through the C ecosystem.

## Examples

```prolog
type food (
    fruit (apple | orange) 
    | meat (pork | poultry (
        chicken | turkey)
    )
).
type satisfied.
type happy.

apple1 :: apple.
chicken1 :: chicken.
turkey1 :: turkey.

eat_rule :: food => satisfied.
mood_rule :: satisfied => happy.

?- eat_rule & mood_rule.
// true (3 solutions found).
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