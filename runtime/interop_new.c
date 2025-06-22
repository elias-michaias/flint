#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// C INTEROPERABILITY LAYER
// =============================================================================
//
// This module provides seamless interop between Flint and C:
// - C functions are treated as purely deterministic computations
// - No unification or logic programming semantics apply to C calls
// - Direct C function calls (since Flint compiles to C)
// - Type conversion between Flint values and C types
// - Compile-time registration of C functions
//
// Key principles:
// 1. C functions are deterministic - they compute results, don't unify
// 2. All Flint values are converted to appropriate C types before calling
// 3. C return values are converted back to Flint values
// 4. No backtracking or choice points within C functions
// 5. Linear resources are properly handled across the boundary
// 6. C functions are called directly (no FFI needed since we compile to C)

// =============================================================================
// C FUNCTION REGISTRY
// =============================================================================

// C type information for marshaling
typedef enum {
    C_TYPE_VOID,
    C_TYPE_INT,
    C_TYPE_LONG,
    C_TYPE_DOUBLE,
    C_TYPE_STRING,      // char*
    C_TYPE_POINTER      // void*
} CType;

// C function signature descriptor
typedef struct CFunctionSignature {
    char* name;
    CType return_type;
    CType* param_types;
    size_t param_count;
    bool consumes_args;     // Whether this function consumes its arguments (for linear types)
} CFunctionSignature;

// Function pointer types for different signatures
typedef int (*CFuncInt0)(void);
typedef int (*CFuncInt1)(int);
typedef int (*CFuncInt2)(int, int);
typedef int (*CFuncInt3)(int, int, int);
typedef double (*CFuncDouble1)(double);
typedef double (*CFuncDouble2)(double, double);
typedef char* (*CFuncString1)(char*);
typedef char* (*CFuncString2)(char*, char*);

// Registered C function
typedef struct CFunction {
    CFunctionSignature sig;
    void* func_ptr;            // Direct C function pointer
    struct CFunction* next;    // Linked list
} CFunction;

// Global registry of C functions
static CFunction* registered_functions = NULL;

// =============================================================================
// TYPE CONVERSION UTILITIES
// =============================================================================

// Convert Flint Value to C type
static bool flint_to_c_value(Value* flint_val, CType c_type, void* c_result) {
    if (!flint_val || !c_result) return false;
    
    // Dereference logical variables first
    flint_val = flint_deref(flint_val);
    
    switch (c_type) {
        case C_TYPE_INT: {
            if (flint_val->type != VAL_INTEGER) return false;
            *(int*)c_result = (int)flint_val->data.integer;
            return true;
        }
        
        case C_TYPE_LONG: {
            if (flint_val->type != VAL_INTEGER) return false;
            *(long*)c_result = (long)flint_val->data.integer;
            return true;
        }
        
        case C_TYPE_DOUBLE: {
            if (flint_val->type == VAL_FLOAT) {
                *(double*)c_result = flint_val->data.float_val;
                return true;
            } else if (flint_val->type == VAL_INTEGER) {
                *(double*)c_result = (double)flint_val->data.integer;
                return true;
            }
            return false;
        }
        
        case C_TYPE_STRING: {
            if (flint_val->type == VAL_STRING) {
                *(char**)c_result = flint_val->data.string;
                return true;
            } else if (flint_val->type == VAL_ATOM) {
                *(char**)c_result = flint_val->data.atom;
                return true;
            }
            return false;
        }
        
        case C_TYPE_POINTER: {
            // For now, just pass the Value pointer itself
            *(void**)c_result = flint_val;
            return true;
        }
        
        case C_TYPE_VOID:
            return false; // Can't convert TO void
    }
    
    return false;
}

// Convert C return value to Flint Value
static Value* c_to_flint_value(CType c_type, void* c_value) {
    if (!c_value && c_type != C_TYPE_VOID) return NULL;
    
    switch (c_type) {
        case C_TYPE_VOID:
            return flint_create_atom("unit"); // Unit value for void returns
            
        case C_TYPE_INT:
            return flint_create_integer(*(int*)c_value);
            
        case C_TYPE_LONG:
            return flint_create_integer(*(long*)c_value);
            
        case C_TYPE_DOUBLE:
            return flint_create_float(*(double*)c_value);
            
        case C_TYPE_STRING: {
            char* str = *(char**)c_value;
            return str ? flint_create_string(str) : flint_create_atom("null");
        }
        
        case C_TYPE_POINTER: {
            // Return an opaque pointer value (could be extended with type info)
            Value* ptr_val = flint_create_atom("c_pointer");
            return ptr_val;
        }
    }
    
    return NULL;
}

// =============================================================================
// C FUNCTION REGISTRATION
// =============================================================================

// Register a C function with direct function pointer
bool flint_register_c_function(const char* name, void* func_ptr, 
                               CType return_type, CType* param_types, size_t param_count,
                               bool consumes_args) {
    if (!name || !func_ptr) return false;
    
    // Create new function entry
    CFunction* cfunc = (CFunction*)malloc(sizeof(CFunction));
    if (!cfunc) return false;
    
    // Initialize signature
    cfunc->sig.name = strdup(name);
    cfunc->sig.return_type = return_type;
    cfunc->sig.param_count = param_count;
    cfunc->sig.consumes_args = consumes_args;
    
    if (param_count > 0) {
        cfunc->sig.param_types = (CType*)malloc(sizeof(CType) * param_count);
        memcpy(cfunc->sig.param_types, param_types, sizeof(CType) * param_count);
    } else {
        cfunc->sig.param_types = NULL;
    }
    
    // Set function pointer
    cfunc->func_ptr = func_ptr;
    
    // Add to registry
    cfunc->next = registered_functions;
    registered_functions = cfunc;
    
    return true;
}

// Find a registered C function
static CFunction* find_c_function(const char* name) {
    CFunction* current = registered_functions;
    while (current) {
        if (strcmp(current->sig.name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// =============================================================================
// C FUNCTION CALLING (Direct dispatch based on signature)
// =============================================================================

// Call a registered C function from Flint
Value* flint_call_c_function(const char* name, Value** args, size_t arg_count, Environment* env) {
    if (!name) return NULL;
    
    // Find the function
    CFunction* cfunc = find_c_function(name);
    if (!cfunc) {
        fprintf(stderr, "C function '%s' not found\n", name);
        return NULL;
    }
    
    // Check argument count
    if (arg_count != cfunc->sig.param_count) {
        fprintf(stderr, "C function '%s' expects %zu arguments, got %zu\n", 
                name, cfunc->sig.param_count, arg_count);
        return NULL;
    }
    
    // Handle linear resource consumption
    if (cfunc->sig.consumes_args) {
        for (size_t i = 0; i < arg_count; i++) {
            flint_consume_value(args[i], LINEAR_OP_FUNCTION_CALL);
        }
    }
    
    // Direct function call based on signature
    // This is much simpler than libffi since we know the types at compile time
    Value* result = NULL;
    
    if (cfunc->sig.return_type == C_TYPE_INT) {
        if (cfunc->sig.param_count == 0) {
            CFuncInt0 func = (CFuncInt0)cfunc->func_ptr;
            int ret = func();
            result = flint_create_integer(ret);
        } else if (cfunc->sig.param_count == 1 && cfunc->sig.param_types[0] == C_TYPE_INT) {
            CFuncInt1 func = (CFuncInt1)cfunc->func_ptr;
            int arg0;
            if (!flint_to_c_value(args[0], C_TYPE_INT, &arg0)) {
                fprintf(stderr, "Failed to convert argument 0 for C function '%s'\n", name);
                return NULL;
            }
            int ret = func(arg0);
            result = flint_create_integer(ret);
        } else if (cfunc->sig.param_count == 2 && 
                   cfunc->sig.param_types[0] == C_TYPE_INT && 
                   cfunc->sig.param_types[1] == C_TYPE_INT) {
            CFuncInt2 func = (CFuncInt2)cfunc->func_ptr;
            int arg0, arg1;
            if (!flint_to_c_value(args[0], C_TYPE_INT, &arg0) ||
                !flint_to_c_value(args[1], C_TYPE_INT, &arg1)) {
                fprintf(stderr, "Failed to convert arguments for C function '%s'\n", name);
                return NULL;
            }
            int ret = func(arg0, arg1);
            result = flint_create_integer(ret);
        }
    } else if (cfunc->sig.return_type == C_TYPE_DOUBLE) {
        if (cfunc->sig.param_count == 1 && cfunc->sig.param_types[0] == C_TYPE_DOUBLE) {
            CFuncDouble1 func = (CFuncDouble1)cfunc->func_ptr;
            double arg0;
            if (!flint_to_c_value(args[0], C_TYPE_DOUBLE, &arg0)) {
                fprintf(stderr, "Failed to convert argument 0 for C function '%s'\n", name);
                return NULL;
            }
            double ret = func(arg0);
            result = flint_create_float(ret);
        }
    } else if (cfunc->sig.return_type == C_TYPE_STRING) {
        if (cfunc->sig.param_count == 1 && cfunc->sig.param_types[0] == C_TYPE_STRING) {
            CFuncString1 func = (CFuncString1)cfunc->func_ptr;
            char* arg0;
            if (!flint_to_c_value(args[0], C_TYPE_STRING, &arg0)) {
                fprintf(stderr, "Failed to convert argument 0 for C function '%s'\n", name);
                return NULL;
            }
            char* ret = func(arg0);
            result = ret ? flint_create_string(ret) : flint_create_atom("null");
        }
    }
    
    if (!result) {
        fprintf(stderr, "Unsupported function signature for C function '%s'\n", name);
        return NULL;
    }
    
    // Suppress unused parameter warning
    (void)env;
    
    return result;
}

// =============================================================================
// CONVENIENCE FUNCTIONS FOR COMMON C TYPES
// =============================================================================

// Register simple C functions with common signatures
bool flint_register_c_int_function(const char* name, int (*func)(int)) {
    CType params[] = {C_TYPE_INT};
    return flint_register_c_function(name, (void*)func, C_TYPE_INT, params, 1, false);
}

bool flint_register_c_string_function(const char* name, char* (*func)(char*)) {
    CType params[] = {C_TYPE_STRING};
    return flint_register_c_function(name, (void*)func, C_TYPE_STRING, params, 1, false);
}

bool flint_register_c_math_function(const char* name, double (*func)(double)) {
    CType params[] = {C_TYPE_DOUBLE};
    return flint_register_c_function(name, (void*)func, C_TYPE_DOUBLE, params, 1, false);
}

bool flint_register_c_binary_int_function(const char* name, int (*func)(int, int)) {
    CType params[] = {C_TYPE_INT, C_TYPE_INT};
    return flint_register_c_function(name, (void*)func, C_TYPE_INT, params, 2, false);
}

// =============================================================================
// INTEGRATION WITH FLINT NARROWING SYSTEM
// =============================================================================

// Create a Flint function wrapper for a C function
Value* flint_create_c_function_wrapper(const char* c_func_name) {
    CFunction* cfunc = find_c_function(c_func_name);
    if (!cfunc) return NULL;
    
    // Create a Flint function that calls the C function
    Value* wrapper = flint_create_function(c_func_name, cfunc->sig.param_count, NULL);
    
    // Store the C function reference in the function's implementation pointer
    wrapper->data.function.impl = (void*)cfunc;
    
    return wrapper;
}

// Integration with the narrowing system (called from narrowing.c)
Value* flint_narrow_c_function(const char* name, Value** args, size_t arg_count, Environment* env) {
    return flint_call_c_function(name, args, arg_count, env);
}

// =============================================================================
// CLEANUP
// =============================================================================

void flint_cleanup_c_interop(void) {
    CFunction* current = registered_functions;
    while (current) {
        CFunction* next = current->next;
        
        free(current->sig.name);
        free(current->sig.param_types);
        free(current);
        current = next;
    }
    registered_functions = NULL;
}

// =============================================================================
// EXAMPLE C FUNCTIONS FOR TESTING
// =============================================================================

// Simple test functions that can be registered
static int c_add(int a, int b) {
    return a + b;
}

static int c_factorial(int n) {
    if (n <= 1) return 1;
    return n * c_factorial(n - 1);
}

static char* c_reverse_string(char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    
    for (size_t i = 0; i < len; i++) {
        result[i] = str[len - 1 - i];
    }
    result[len] = '\0';
    
    return result;
}

// Initialize built-in C functions
void flint_init_builtin_c_functions(void) {
    // Register some basic math functions
    flint_register_c_binary_int_function("c_add", c_add);
    flint_register_c_int_function("c_factorial", c_factorial);
    flint_register_c_string_function("c_reverse_string", c_reverse_string);
    
    // Register standard math library functions
    flint_register_c_math_function("c_sin", sin);
    flint_register_c_math_function("c_cos", cos);
    flint_register_c_math_function("c_sqrt", sqrt);
    flint_register_c_math_function("c_exp", exp);
    flint_register_c_math_function("c_log", log);
}
