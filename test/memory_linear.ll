// This version respects linear logic - each pointer is used exactly once
let ptr = alloc(8) in
let value = 100 in
store(ptr, value)
