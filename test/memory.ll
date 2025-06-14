let ptr = alloc(8) in
let value = 100 in
let _ = store(ptr, value) in
let result = load(ptr) in
let _ = free(ptr) in
result
