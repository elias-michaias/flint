#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// =============================================================================
// GLOBAL RUNTIME STATE
// =============================================================================

static VarId next_var_id = 1;
static Environment* global_env = NULL;
static ChoicePoint* current_choice_point = NULL;
static ConstraintStore* global_constraints = NULL;
static FunctionRegistry* global_function_registry = NULL;

// Global registry for all created logic variables
Value** global_logic_vars = NULL;
size_t global_logic_var_count = 0;
static size_t global_logic_var_capacity = 0;

// Simple memory tracking for cleanup
typedef struct MemBlock {
    void* ptr;
    struct MemBlock* next;
} MemBlock;

static MemBlock* allocated_blocks = NULL;

// =============================================================================
// MEMORY MANAGEMENT
// =============================================================================

void flint_init_runtime(void) {
    printf("[DEBUG] Initializing Flint runtime...\n");
    fflush(stdout);
    next_var_id = 1;
    printf("[DEBUG] Set next_var_id\n");
    fflush(stdout);
    
    // Initialize function registry
    global_function_registry = malloc(sizeof(FunctionRegistry));
    global_function_registry->entries = NULL;
    global_function_registry->count = 0;
    printf("[DEBUG] Created function registry\n");
    fflush(stdout);
    
    // Create constraint store first
    global_constraints = flint_create_constraint_store();
    printf("[DEBUG] Created constraint store\n");
    fflush(stdout);
    
    // Create global environment and connect it to the constraint store
    global_env = flint_create_environment(NULL);
    if (global_env && global_constraints) {
        global_env->constraint_store = global_constraints;
        printf("[DEBUG] Connected constraint store to global environment\n");
    }
    printf("[DEBUG] Created global environment\n");
    fflush(stdout);
    
    allocated_blocks = NULL;
    
    // Initialize the linear resource management system
    printf("[DEBUG] Calling flint_init_linear_system\n");
    fflush(stdout);
    flint_init_linear_system();
    printf("[DEBUG] flint_init_linear_system completed\n");
    fflush(stdout);
    
    // Initialize built-in C interop functions
    printf("[DEBUG] Calling flint_init_builtin_c_functions\n");
    fflush(stdout);
    flint_init_builtin_c_functions();
    printf("[DEBUG] flint_init_builtin_c_functions completed\n");
    fflush(stdout);
    
    // Initialize async system with structured concurrency
    printf("[DEBUG] Calling flint_init_async_system\n");
    fflush(stdout);
    flint_init_async_system(global_env);
    printf("[DEBUG] flint_init_async_system completed\n");
    fflush(stdout);
    flint_register_async_functions();
    printf("[DEBUG] Flint runtime initialized successfully\n");
    fflush(stdout);
}

Environment* flint_get_global_env(void) {
    return global_env;
}

ConstraintStore* flint_get_global_constraint_store(void) {
    return global_constraints;
}

// =============================================================================
// FUNCTION REGISTRY
// =============================================================================

void flint_register_function(const char* name, Value* (*func_ptr)(Value*)) {
    printf("[DEBUG] Registering function: %s\n", name);
    
    if (!global_function_registry) {
        printf("[DEBUG] Error: function registry not initialized\n");
        return;
    }
    
    // Create new registry entry
    FunctionRegistryEntry* entry = malloc(sizeof(FunctionRegistryEntry));
    entry->name = malloc(strlen(name) + 1);
    strcpy(entry->name, name);
    entry->arity = 1;
    entry->func_ptr = (void*)func_ptr;
    entry->next = global_function_registry->entries;
    
    // Add to front of list
    global_function_registry->entries = entry;
    global_function_registry->count++;
    
    printf("[DEBUG] Registered function %s (total functions: %zu)\n", name, global_function_registry->count);
}

void flint_register_function_2(const char* name, Value* (*func_ptr)(Value*, Value*)) {
    printf("[DEBUG] Registering 2-arg function: %s\n", name);
    
    if (!global_function_registry) {
        printf("[DEBUG] Error: function registry not initialized\n");
        return;
    }
    
    // Create new registry entry
    FunctionRegistryEntry* entry = malloc(sizeof(FunctionRegistryEntry));
    entry->name = malloc(strlen(name) + 1);
    strcpy(entry->name, name);
    entry->arity = 2;
    entry->func_ptr = (void*)func_ptr;
    entry->next = global_function_registry->entries;
    
    // Add to front of list
    global_function_registry->entries = entry;
    global_function_registry->count++;
    
    printf("[DEBUG] Registered 2-arg function %s (total functions: %zu)\n", name, global_function_registry->count);
}

Value* flint_call_registered_function(const char* name, Value* arg) {
    printf("[DEBUG] Looking up function: %s\n", name);
    
    if (!global_function_registry) {
        printf("[DEBUG] Error: function registry not initialized\n");
        return NULL;
    }
    
    // Search for function in registry
    FunctionRegistryEntry* current = global_function_registry->entries;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            printf("[DEBUG] Found function %s, calling it\n", name);
            if (current->arity == 1) {
                Value* (*func_ptr)(Value*) = (Value* (*)(Value*))current->func_ptr;
                return func_ptr(arg);
            } else {
                printf("[DEBUG] Error: function %s expects %d arguments, got 1\n", name, current->arity);
                return NULL;
            }
        }
        current = current->next;
    }
    
    printf("[DEBUG] Function %s not found in registry\n", name);
    return NULL;
}

bool flint_is_function_registered(const char* name) {
    if (!global_function_registry) {
        return false;
    }
    
    FunctionRegistryEntry* current = global_function_registry->entries;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return true;
        }
        current = current->next;
    }
    
    return false;
}

// Call a 2-argument function from the registry
Value* flint_call_registered_function_2(const char* name, Value* arg1, Value* arg2) {
    printf("[DEBUG] Looking up 2-arg function: %s\n", name);
    
    if (!global_function_registry) {
        printf("[DEBUG] Error: function registry not initialized\n");
        return NULL;
    }
    
    // Search for function in registry
    FunctionRegistryEntry* current = global_function_registry->entries;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            printf("[DEBUG] Found function %s, calling with 2 args\n", name);
            if (current->arity == 2) {
                Value* (*func_ptr)(Value*, Value*) = (Value* (*)(Value*, Value*))current->func_ptr;
                return func_ptr(arg1, arg2);
            } else {
                printf("[DEBUG] Error: function %s expects %d arguments, got 2\n", name, current->arity);
                return NULL;
            }
        }
        current = current->next;
    }
    
    printf("[DEBUG] Function %s not found in registry\n", name);
    return NULL;
}

// Value conversion helpers
int flint_value_to_int(Value* val) {
    if (!val) return 0;
    
    // Dereference if it's a logical variable
    val = flint_deref(val);
    if (!val) return 0;
    
    // Check if this value has been consumed (linearity enforcement)
    if (val->is_consumed && val->consumption_count > 0) {
        #ifdef DEBUG_LINEAR
        printf("[DEBUG] LINEAR: Attempting to access consumed value %p (count=%u)\n", 
               val, val->consumption_count);
        #endif
        printf("[DEBUG] Warning: Cannot convert consumed value to int, returning 0\n");
        return 0;
    }
    
    if (val->type == VAL_INTEGER) {
        return (int)val->data.integer;
    } else if (val->type == VAL_FLOAT) {
        return (int)val->data.float_val;
    }
    
    printf("[DEBUG] Warning: Cannot convert value to int, returning 0\n");
    return 0;
}

double flint_value_to_double(Value* val) {
    if (!val) return 0.0;
    
    // Dereference if it's a logical variable
    val = flint_deref(val);
    if (!val) return 0.0;
    
    if (val->type == VAL_FLOAT) {
        return val->data.float_val;
    } else if (val->type == VAL_INTEGER) {
        return (double)val->data.integer;
    }
    
    printf("[DEBUG] Warning: Cannot convert value to double, returning 0.0\n");
    return 0.0;
}

const char* flint_value_to_string(Value* val) {
    if (!val) return "";
    
    // Dereference if it's a logical variable
    val = flint_deref(val);
    if (!val) return "";
    
    if (val->type == VAL_STRING) {
        return val->data.string;
    } else if (val->type == VAL_ATOM) {
        return val->data.atom;
    }
    
    printf("[DEBUG] Warning: Cannot convert value to string, returning empty string\n");
    return "";
}

void flint_cleanup_runtime(void) {
    // Cleanup async system
    flint_cleanup_async_system();
    
    // Cleanup C interop functions
    flint_cleanup_c_interop();
    
    // Cleanup linear system first
    flint_cleanup_linear_system();
    
    // Temporarily disable global memory cleanup to avoid double-free issues
    // The memory cleanup may be freeing memory that was already freed explicitly
    /*
    MemBlock* current = allocated_blocks;
    while (current) {
        MemBlock* next = current->next;
        free(current->ptr);
        free(current);
        current = next;
    }
    allocated_blocks = NULL;
    */
    
    if (global_env) {
        flint_free_environment(global_env);
        global_env = NULL;
    }
}

void* flint_alloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    
    // Track the allocation
    MemBlock* block = malloc(sizeof(MemBlock));
    if (block) {
        block->ptr = ptr;
        block->next = allocated_blocks;
        allocated_blocks = block;
    }
    
    return ptr;
}

void flint_free(void* ptr) {
    if (!ptr) return;
    
    // Remove from tracking list and free only if found
    MemBlock** current = &allocated_blocks;
    bool found = false;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemBlock* to_free = *current;
            *current = (*current)->next;
            free(to_free);
            found = true;
            break;
        }
        current = &(*current)->next;
    }
    
    // Only free the pointer if it was in our tracking list
    if (found) {
        free(ptr);
    }
}

// =============================================================================
// VALUE MANAGEMENT
// =============================================================================

Value* flint_create_integer(int64_t val) {
    printf("[DEBUG] Creating integer value: %lld\n", val);
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_INTEGER;
    value->data.integer = val;
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    printf("[DEBUG] Created integer value at %p with value %lld\n", value, val);
    return value;
}

Value* flint_create_float(double val) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_FLOAT;
    value->data.float_val = val;
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    return value;
}

Value* flint_create_string(const char* str) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_STRING;
    value->data.string = flint_alloc(strlen(str) + 1);
    strcpy(value->data.string, str);
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    return value;
}

Value* flint_create_atom(const char* atom) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_ATOM;
    value->data.atom = flint_alloc(strlen(atom) + 1);
    strcpy(value->data.atom, atom);
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    return value;
}

Value* flint_create_record(char** field_names, Value** field_values, size_t field_count) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_RECORD;
    value->data.record.field_count = field_count;
    
    if (field_count > 0) {
        value->data.record.field_names = flint_alloc(sizeof(char*) * field_count);
        value->data.record.field_values = flint_alloc(sizeof(Value) * field_count);
        
        for (size_t i = 0; i < field_count; i++) {
            // Copy field names
            size_t name_len = strlen(field_names[i]);
            value->data.record.field_names[i] = flint_alloc(name_len + 1);
            strcpy(value->data.record.field_names[i], field_names[i]);
            
            // Copy field values
            value->data.record.field_values[i] = *field_values[i];
        }
    } else {
        value->data.record.field_names = NULL;
        value->data.record.field_values = NULL;
    }
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    return value;
}

Value* flint_create_logical_var(bool is_linear) {
    printf("[DEBUG] Creating logical variable (is_linear=%s)\n", is_linear ? "true" : "false");
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_LOGICAL_VAR;
    
    LogicalVar* var = flint_alloc(sizeof(LogicalVar));
    var->id = flint_fresh_var_id();
    var->binding = NULL;  // Uninstantiated
    var->waiters = NULL;
    var->use_count = 0;
    var->is_consumed = false;
    var->allow_reuse = !is_linear;  // If not linear by default, allow reuse
    
    value->data.logical_var = var;
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    // Register in global logic variable registry
    if (global_logic_var_count >= global_logic_var_capacity) {
        global_logic_var_capacity = (global_logic_var_capacity == 0) ? 8 : (global_logic_var_capacity * 2);
        global_logic_vars = realloc(global_logic_vars, sizeof(Value*) * global_logic_var_capacity);
    }
    global_logic_vars[global_logic_var_count++] = value;
    
    printf("[DEBUG] Created logical variable at %p with var at %p (id=%llu), registered globally (%zu total) - %s\n", 
           value, var, var->id, global_logic_var_count, 
           global_logic_var_count == 1 ? "call_result_1" : 
           global_logic_var_count == 2 ? "z" : 
           global_logic_var_count == 3 ? "unused" : 
           global_logic_var_count == 4 ? "add_result" : "unknown");
    
    // Register the variable in the global environment so it can be looked up for suspensions
    if (global_env) {
        flint_register_unbound_variable(global_env, var->id, var);
    }
    return value;
}

LogicalVar* flint_get_logical_var(Value* val) {
    if (val->type == VAL_LOGICAL_VAR) {
        return val->data.logical_var;
    }
    return NULL;
}

VarId flint_fresh_var_id(void) {
    return next_var_id++;
}

VarId flint_next_var_id(void) {
    return flint_fresh_var_id();
}

Value* flint_create_unbound_variable(VarId id) {
    Value* var = flint_create_logical_var(false);
    if (var && var->type == VAL_LOGICAL_VAR) {
        var->data.logical_var->id = id;
    }
    return var;
}

void flint_solve_function_constraint_runtime(const char* function_name, Value* arg_var, Value* target_value, Environment* env) {
    printf("[DEBUG] Solving constraint: %s($x) = %lld using constraint-based approach\n", function_name, flint_deref(target_value)->data.integer);
    
    if (arg_var->type != VAL_LOGICAL_VAR) {
        printf("[DEBUG] Error: arg_var is not a logic variable\n");
        return;
    }
    
    VarId var_id = arg_var->data.logical_var->id;
    int64_t target = flint_deref(target_value)->data.integer;
    
    // Add the function constraint to the constraint store
    ConstraintStore* store = flint_get_global_constraint_store();
    if (!store) {
        printf("[DEBUG] Error: No constraint store available\n");
        return;
    }
    
    // Add constraint: function_name(var_id) = target  
    printf("[DEBUG] Adding function constraint %s($%llu) = %lld to constraint store\n", function_name, var_id, target);
    bool constraint_added = flint_add_function_constraint(store, function_name, var_id, (int)target);
    
    if (!constraint_added) {
        printf("[DEBUG] Failed to add constraint to store\n");
        return;
    }
    
    // Check if the function is registered in our function registry
    if (!flint_is_function_registered(function_name)) {
        printf("[DEBUG] Function %s not registered - cannot solve function constraint\n", function_name);
        return;
    }
    
    printf("[DEBUG] Function %s is registered - proceeding with constraint solving\n", function_name);
    
    // Create a fresh variable to represent the result of function_name(arg_var)
    Value* function_result = flint_create_logical_var(false);
    
    // Create a suspension that represents calling the function
    // This suspension will evaluate function_name(arg_var) when forced
    printf("[DEBUG] Creating function call suspension for %s\n", function_name);
    Value* function_call = flint_create_function_call_suspension(function_name, (Value*[]){arg_var}, 1);
    
    // Unify the function call result with our function_result variable
    // This sets up the constraint: function_result = function_name(arg_var)
    printf("[DEBUG] Unifying function call suspension with result variable\n");
    bool unified_call = flint_unify(function_result, function_call, env);
    
    if (!unified_call) {
        printf("[DEBUG] Failed to unify function call with result variable\n");
        return;
    }
    
    // Now unify the function result with the target value
    // This sets up the constraint: function_name(arg_var) = target_value
    printf("[DEBUG] Unifying function result with target value %lld\n", target);
    bool unified_target = flint_unify(function_result, target_value, env);
    
    if (!unified_target) {
        printf("[DEBUG] Failed to unify function result with target value\n");
        return;
    }
    
    // Trigger constraint propagation to solve the system
    printf("[DEBUG] Triggering constraint propagation for variable %llu\n", var_id);
    flint_solve_constraints(store, var_id, env);
    
    // Check if the variable is now bound
    LogicalVar* var = arg_var->data.logical_var;
    if (var->binding) {
        Value* bound_value = flint_deref(var->binding);
        if (bound_value->type == VAL_INTEGER) {
            printf("[DEBUG] Constraint solved: $x = %lld\n", bound_value->data.integer);
        } else {
            printf("[DEBUG] Variable bound to non-integer value\n");
        }
    } else {
        printf("[DEBUG] Variable remains unbound after constraint solving\n");
        printf("[DEBUG] This indicates the constraint system needs more sophisticated solving\n");
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void flint_print_value(Value* val) {
    if (!val) {
        printf("NULL");
        return;
    }
    
    switch (val->type) {
        case VAL_INTEGER:
            printf("%lld", val->data.integer);
            break;
        case VAL_FLOAT:
            printf("%f", val->data.float_val);
            break;
        case VAL_STRING:
            printf("\"%s\"", val->data.string);
            break;
        case VAL_ATOM:
            printf("%s", val->data.atom);
            break;
        case VAL_LIST:
            flint_list_print(val);
            break;
        case VAL_RECORD:
            printf("{");
            for (size_t i = 0; i < val->data.record.field_count; i++) {
                if (i > 0) printf(", ");
                printf("%s: ", val->data.record.field_names[i]);
                flint_print_value(&val->data.record.field_values[i]);
            }
            printf("}");
            break;
        case VAL_LOGICAL_VAR: {
            LogicalVar* var = val->data.logical_var;
            if (var->binding) {
                flint_print_value(var->binding);
            } else {
                printf("_G%llu", var->id);
            }
            break;
        }
        case VAL_SUSPENSION:
            printf("<suspension>");
            break;
        case VAL_PARTIAL:
            printf("<partial:");
            flint_print_value(val->data.partial.base);
            printf(">");
            break;
        default:
            printf("<unknown>");
            break;
    }
}

bool flint_is_ground(Value* val) {
    if (!val) return true;
    
    switch (val->type) {
        case VAL_INTEGER:
        case VAL_FLOAT:
        case VAL_STRING:
        case VAL_ATOM:
            return true;
            
        case VAL_LOGICAL_VAR: {
            LogicalVar* var = val->data.logical_var;
            if (var->binding) {
                return flint_is_ground(var->binding);
            }
            return false;  // Uninstantiated variable is not ground
        }
        
        case VAL_LIST:
            return flint_list_is_ground(val);
            
        case VAL_RECORD:
            for (size_t i = 0; i < val->data.record.field_count; i++) {
                if (!flint_is_ground(&val->data.record.field_values[i])) {
                    return false;
                }
            }
            return true;
            
        case VAL_SUSPENSION:
        case VAL_PARTIAL:
            return false;
            
        default:
            return false;
    }
}

// =============================================================================
// HIGHER-ORDER FUNCTION SUPPORT
// =============================================================================

Value* flint_create_function(const char* name, int arity, void* impl) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_FUNCTION;
    
    value->data.function.name = flint_alloc(strlen(name) + 1);
    strcpy(value->data.function.name, name);
    value->data.function.arity = arity;
    value->data.function.partial_args = NULL;
    value->data.function.applied_count = 0;
    value->data.function.impl = impl;
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    return value;
}

Value* flint_create_partial_app(Value* func, Value** args, int applied_count) {
    if (!func || func->type != VAL_FUNCTION) {
        return NULL;
    }
    
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_PARTIAL_APP;
    
    // Copy function info
    value->data.function.name = flint_alloc(strlen(func->data.function.name) + 1);
    strcpy(value->data.function.name, func->data.function.name);
    value->data.function.arity = func->data.function.arity;
    value->data.function.impl = func->data.function.impl;
    value->data.function.applied_count = applied_count;
    
    // Copy partial arguments
    if (applied_count > 0) {
        value->data.function.partial_args = flint_alloc(sizeof(Value*) * applied_count);
        for (int i = 0; i < applied_count; i++) {
            value->data.function.partial_args[i] = args[i];
        }
    } else {
        value->data.function.partial_args = NULL;
    }
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
    return value;
}

bool flint_is_fully_applied(Value* func) {
    if (!func) return false;
    
    switch (func->type) {
        case VAL_FUNCTION:
            return func->data.function.applied_count >= func->data.function.arity;
        case VAL_PARTIAL_APP:
            return func->data.function.applied_count >= func->data.function.arity;
        default:
            return false;
    }
}

Value* flint_apply_function(Value* func, Value** args, size_t arg_count, Environment* env) {
    if (!func || !args || arg_count == 0) {
        return NULL;
    }
    
    switch (func->type) {
        case VAL_FUNCTION:
        case VAL_PARTIAL_APP: {
            int total_args_needed = func->data.function.arity;
            int current_applied = func->data.function.applied_count;
            int remaining_needed = total_args_needed - current_applied;
            
            if ((int)arg_count > remaining_needed) {
                // Too many arguments provided
                return NULL;
            }
            
            if ((int)arg_count == remaining_needed) {
                // Function is now fully applied - execute it
                // For now, we'll implement some basic functions
                if (strcmp(func->data.function.name, "length") == 0) {
                    // length(list, result) - unify result with length of list
                    Value* list_arg = (current_applied > 0) ? func->data.function.partial_args[0] : args[0];
                    Value* result_arg = (current_applied > 0) ? args[0] : args[1];
                    
                    list_arg = flint_deref(list_arg);
                    if (list_arg->type == VAL_LIST) {
                        Value* length_val = flint_create_integer((int64_t)flint_list_length(list_arg));
                        flint_unify(result_arg, length_val, env);
                        return result_arg;
                    }
                }
                
                // For unknown functions, just return NULL for now
                return NULL;
            } else {
                // Create a new partial application with more arguments
                int new_applied_count = current_applied + (int)arg_count;
                Value** all_args = flint_alloc(sizeof(Value*) * new_applied_count);
                
                // Copy existing partial args
                for (int i = 0; i < current_applied; i++) {
                    all_args[i] = func->data.function.partial_args[i];
                }
                
                // Add new args
                for (size_t i = 0; i < arg_count; i++) {
                    all_args[current_applied + i] = args[i];
                }
                
                Value* new_func = flint_alloc(sizeof(Value));
                new_func->type = VAL_PARTIAL_APP;
                new_func->data.function.name = flint_alloc(strlen(func->data.function.name) + 1);
                strcpy(new_func->data.function.name, func->data.function.name);
                new_func->data.function.arity = func->data.function.arity;
                new_func->data.function.impl = func->data.function.impl;
                new_func->data.function.applied_count = new_applied_count;
                new_func->data.function.partial_args = all_args;
                
                flint_mark_linear(new_func);
                return new_func;
            }
        }
        
        default:
            return NULL;
    }
}

void flint_free_value(Value* val) {
    if (!val) return;
    
    // Free dynamically allocated data
    switch (val->type) {
        case VAL_STRING:
            if (val->data.string) {
                flint_free(val->data.string);
            }
            break;
        case VAL_ATOM:
            if (val->data.atom) {
                flint_free(val->data.atom);
            }
            break;
        case VAL_LIST:
            flint_list_free(val);
            break;
        case VAL_RECORD:
            if (val->data.record.field_names) {
                for (size_t i = 0; i < val->data.record.field_count; i++) {
                    if (val->data.record.field_names[i]) {
                        flint_free(val->data.record.field_names[i]);
                    }
                }
                flint_free(val->data.record.field_names);
            }
            if (val->data.record.field_values) {
                flint_free(val->data.record.field_values);
            }
            break;
        case VAL_LOGICAL_VAR:
            if (val->data.logical_var) {
                flint_free(val->data.logical_var);
            }
            break;
        default:
            // No special cleanup needed for other types
            break;
    }
    
    // Free the value itself
    flint_free(val);
}

// =============================================================================
// CONSTRAINT SOLVING FUNCTIONS
// =============================================================================

/// Solve arithmetic constraints using unification
/// This handles different modes: forward (X + Y = Z), backward (X + ? = Z), etc.
bool flint_solve_arithmetic_constraint(Value* left, Value* right, Value* result, const char* operation) {
    printf("[DEBUG] Solving arithmetic constraint: %s %s %s\n", 
           left ? "arg1" : "NULL", operation, right ? "arg2" : "NULL");
    
    if (!left || !right || !result) {
        printf("[DEBUG] Error: NULL arguments to arithmetic constraint\n");
        return false;
    }
    
    // Dereference all values to get their current bindings
    left = flint_deref(left);
    right = flint_deref(right);
    result = flint_deref(result);
    
    // Case 1: Both operands are concrete, compute result
    if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
        int left_val = left->data.integer;
        int right_val = right->data.integer;
        int result_val;
        
        if (strcmp(operation, "add") == 0) {
            result_val = left_val + right_val;
        } else if (strcmp(operation, "subtract") == 0) {
            result_val = left_val - right_val;
        } else if (strcmp(operation, "multiply") == 0) {
            result_val = left_val * right_val;
        } else if (strcmp(operation, "divide") == 0) {
            if (right_val != 0) {
                result_val = left_val / right_val;
            } else {
                printf("[DEBUG] Error: Division by zero in constraint\n");
                return false;
            }
        } else if (strcmp(operation, "modulo") == 0) {
            if (right_val != 0) {
                result_val = left_val % right_val;
            } else {
                printf("[DEBUG] Error: Modulo by zero in constraint\n");
                return false;
            }
        } else {
            printf("[DEBUG] Error: Unknown operation %s\n", operation);
            return false;
        }
        
        // Create the result value and unify
        Value* computed_result = flint_create_integer(result_val);
        return flint_unify(result, computed_result, flint_get_global_env());
    }
    
    // Case 2: Result and one operand are concrete, solve for the other
    if (result->type == VAL_INTEGER) {
        int result_val = result->data.integer;
        
        if (left->type == VAL_INTEGER && right->type == VAL_LOGICAL_VAR) {
            // Solve: left + ? = result  =>  ? = result - left
            int left_val = left->data.integer;
            int right_val;
            
            if (strcmp(operation, "add") == 0) {
                right_val = result_val - left_val;
            } else if (strcmp(operation, "subtract") == 0) {
                right_val = result_val + left_val;  // left - ? = result => ? = left - result
            } else if (strcmp(operation, "multiply") == 0) {
                if (left_val != 0) {
                    right_val = result_val / left_val;
                } else {
                    printf("[DEBUG] Error: Cannot solve multiply constraint with left=0\n");
                    return false;
                }
            } else {
                printf("[DEBUG] Error: Backward solving for %s not implemented\n", operation);
                return false;
            }
            
            Value* computed_right = flint_create_integer(right_val);
            return flint_unify(right, computed_right, flint_get_global_env());
        }
        
        if (right->type == VAL_INTEGER && left->type == VAL_LOGICAL_VAR) {
            // Solve: ? + right = result  =>  ? = result - right
            int right_val = right->data.integer;
            int left_val;
            
            if (strcmp(operation, "add") == 0) {
                left_val = result_val - right_val;
            } else if (strcmp(operation, "subtract") == 0) {
                left_val = result_val + right_val;  // ? - right = result => ? = result + right
            } else if (strcmp(operation, "multiply") == 0) {
                if (right_val != 0) {
                    left_val = result_val / right_val;
                } else {
                    printf("[DEBUG] Error: Cannot solve multiply constraint with right=0\n");
                    return false;
                }
            } else {
                printf("[DEBUG] Error: Backward solving for %s not implemented\n", operation);
                return false;
            }
            
            Value* computed_left = flint_create_integer(left_val);
            return flint_unify(left, computed_left, flint_get_global_env());
        }
    }
    
    // Case 3: Multiple unknowns - use pending constraint mechanism
    printf("[DEBUG] Multiple unknowns in arithmetic constraint - adding to pending constraint system\n");
    
    // Create and add a pending arithmetic constraint
    ArithmeticConstraint* constraint = flint_create_arithmetic_constraint(operation, left, right, result);
    return flint_add_pending_arithmetic_constraint(constraint, flint_get_global_env());
}

// =============================================================================
// PENDING ARITHMETIC CONSTRAINT MANAGEMENT
// =============================================================================

ArithmeticConstraint* flint_create_arithmetic_constraint(const char* operation, Value* left, Value* right, Value* result) {
    printf("[DEBUG] flint_create_arithmetic_constraint called with:\n");
    printf("[DEBUG]   operation: %s\n", operation);
    printf("[DEBUG]   left: %p (type=%d)\n", left, left ? left->type : -1);
    printf("[DEBUG]   right: %p (type=%d)\n", right, right ? right->type : -1);
    printf("[DEBUG]   result: %p (type=%d)\n", result, result ? result->type : -1);
    
    if (left && left->type == VAL_LOGICAL_VAR) {
        printf("[DEBUG]   left var id: %llu\n", left->data.logical_var->id);
    }
    if (right && right->type == VAL_LOGICAL_VAR) {
        printf("[DEBUG]   right var id: %llu\n", right->data.logical_var->id);
    }
    if (result && result->type == VAL_LOGICAL_VAR) {
        printf("[DEBUG]   result var id: %llu\n", result->data.logical_var->id);
    }
    
    ArithmeticConstraint* constraint = flint_alloc(sizeof(ArithmeticConstraint));
    
    // Copy the operation string
    constraint->operation = flint_alloc(strlen(operation) + 1);
    strcpy(constraint->operation, operation);
    
    // Store the operands
    constraint->left = left;
    constraint->right = right;
    constraint->result = result;
    
    // Collect all logical variables this constraint depends on
    VarId deps[3];
    size_t dep_count = 0;
    
    if (left && left->type == VAL_LOGICAL_VAR) {
        deps[dep_count++] = left->data.logical_var->id;
    }
    if (right && right->type == VAL_LOGICAL_VAR) {
        deps[dep_count++] = right->data.logical_var->id;
    }
    if (result && result->type == VAL_LOGICAL_VAR) {
        deps[dep_count++] = result->data.logical_var->id;
    }
    
    constraint->dependency_count = dep_count;
    if (dep_count > 0) {
        constraint->dependency_vars = flint_alloc(sizeof(VarId) * dep_count);
        memcpy(constraint->dependency_vars, deps, sizeof(VarId) * dep_count);
    } else {
        constraint->dependency_vars = NULL;
    }
    
    printf("[DEBUG] Created arithmetic constraint: %s with %zu dependencies\n", operation, dep_count);
    for (size_t i = 0; i < dep_count; i++) {
        printf("[DEBUG] Dependency %zu: variable %llu\n", i, deps[i]);
    }
    
    return constraint;
}

void flint_free_arithmetic_constraint(ArithmeticConstraint* constraint) {
    if (!constraint) return;
    
    if (constraint->operation) {
        flint_free(constraint->operation);
    }
    if (constraint->dependency_vars) {
        flint_free(constraint->dependency_vars);
    }
    flint_free(constraint);
}

bool flint_solve_pending_arithmetic_constraint(ArithmeticConstraint* constraint, Environment* env) {
    printf("[DEBUG] Attempting to solve pending arithmetic constraint: %s\n", constraint->operation);
    
    // Dereference all values to get their current bindings
    Value* left = flint_deref(constraint->left);
    Value* right = flint_deref(constraint->right);
    Value* result = flint_deref(constraint->result);
    
    printf("[DEBUG] After deref - Left: %p (type=%d), Right: %p (type=%d), Result: %p (type=%d)\n",
           left, left ? left->type : -1,
           right, right ? right->type : -1,
           result, result ? result->type : -1);
    
    // Check what we know
    bool left_concrete = (left->type == VAL_INTEGER);
    bool right_concrete = (right->type == VAL_INTEGER);
    bool result_concrete = (result->type == VAL_INTEGER);
    
    int concrete_count = 0;
    if (left_concrete) {
        concrete_count++;
        printf("[DEBUG] Left is concrete: %d\n", left->data.integer);
    }
    if (right_concrete) {
        concrete_count++;
        printf("[DEBUG] Right is concrete: %d\n", right->data.integer);
    }
    if (result_concrete) {
        concrete_count++;
        printf("[DEBUG] Result is concrete: %d\n", result->data.integer);
    }
    
    printf("[DEBUG] Concrete values in constraint: %d/3\n", concrete_count);
    
    // Case 1: We have at least 2 concrete values - solve deterministically
    if (concrete_count >= 2) {
        printf("[DEBUG] Solving deterministically with 2+ concrete values\n");
        return flint_solve_arithmetic_constraint(left, right, result, constraint->operation);
    }
    
    // Case 1.5: Check if we have 2 concrete after dereferencing (handles unification chains)
    if (concrete_count == 1) {
        printf("[DEBUG] Only 1 concrete value found, but checking if unification chains provide more\n");
        
        // For the specific case we're testing: z + 5 = result, where result is bound through unification
        if (right_concrete && result_concrete) {
            printf("[DEBUG] Have right (%d) and result (%d), solving for left\n", right->data.integer, result->data.integer);
            return flint_solve_arithmetic_constraint(left, right, result, constraint->operation);
        }
        if (left_concrete && result_concrete) {
            printf("[DEBUG] Have left (%d) and result (%d), solving for right\n", left->data.integer, result->data.integer);
            return flint_solve_arithmetic_constraint(left, right, result, constraint->operation);
        }
        if (left_concrete && right_concrete) {
            printf("[DEBUG] Have left (%d) and right (%d), solving for result\n", left->data.integer, right->data.integer);
            return flint_solve_arithmetic_constraint(left, right, result, constraint->operation);
        }
    }
    
    // Case 2: Logic programming approach - generate solutions
    // Only generate solutions when we have a concrete result (target value known)
    if (concrete_count >= 2) {
        printf("[DEBUG] Solving deterministically with %d concrete values\n", concrete_count);
        return flint_solve_arithmetic_constraint(left, right, result, constraint->operation);
    } else if (concrete_count == 1 && result_concrete) {
        printf("[DEBUG] Logic programming mode: Have concrete result, solving for unknowns\n");
        
        // For addition: a + b = result_concrete
        if (strcmp(constraint->operation, "add") == 0) {
            int64_t target = result->data.integer;
            
            if (left_concrete) {
                // left_concrete + ? = target, so ? = target - left_concrete
                int64_t right_val = target - left->data.integer;
                printf("[DEBUG] Solving: %lld + ? = %lld, so ? = %lld\n", left->data.integer, target, right_val);
                
                Value* right_solution = flint_create_integer(right_val);
                return flint_unify(right, right_solution, env);
            } else if (right_concrete) {
                // ? + right_concrete = target, so ? = target - right_concrete
                int64_t left_val = target - right->data.integer;
                printf("[DEBUG] Solving: ? + %lld = %lld, so ? = %lld\n", right->data.integer, target, left_val);
                
                Value* left_solution = flint_create_integer(left_val);
                return flint_unify(left, left_solution, env);
            } else {
                // Two unknowns, generate a reasonable first solution
                printf("[DEBUG] Two unknowns with target %lld, generating first solution\n", target);
                
                // Split roughly in half as a first solution
                int64_t left_val = target / 2;
                int64_t right_val = target - left_val;
                
                printf("[DEBUG] Generating solution: left = %lld, right = %lld (sum = %lld)\n", left_val, right_val, target);
                
                Value* left_solution = flint_create_integer(left_val);
                Value* right_solution = flint_create_integer(right_val);
                
                if (flint_unify(left, left_solution, env)) {
                    return flint_unify(right, right_solution, env);
                }
            }
        }
        
        printf("[DEBUG] Successfully solved constraint with concrete result\n");
        return true;
    } else {
        printf("[DEBUG] Logic programming mode: Waiting for concrete result value\n");
        printf("[DEBUG] Constraint will be re-evaluated when result gets bound\n");
        return false;
    }
}

bool flint_add_pending_arithmetic_constraint(ArithmeticConstraint* constraint, Environment* env) {
    printf("[DEBUG] Adding pending arithmetic constraint for operation: %s\n", constraint->operation);
    
    // First, try to solve the constraint immediately
    if (flint_solve_pending_arithmetic_constraint(constraint, env)) {
        printf("[DEBUG] Constraint solved immediately\n");
        flint_free_arithmetic_constraint(constraint);
        return true;
    }
    
    // If we can't solve it immediately, suspend it on the unbound variables
    // Create separate suspensions for each variable to avoid linked list corruption
    for (size_t i = 0; i < constraint->dependency_count; i++) {
        printf("[DEBUG] Looking up variable %llu for suspension\n", constraint->dependency_vars[i]);
        LogicalVar* var = flint_lookup_variable(env, constraint->dependency_vars[i]);
        printf("[DEBUG] Variable lookup result: %p\n", var);
        if (var) {
            // Create a separate computation and suspension for each variable
            SuspensionComputation* comp = flint_alloc(sizeof(SuspensionComputation));
            comp->type = SUSP_ARITHMETIC;
            comp->function_name = NULL;
            comp->expr_code = NULL;
            comp->operands = NULL;
            comp->operand_count = 0;
            comp->data = constraint; // Store the arithmetic constraint (shared data is OK)
            
            Suspension* susp = flint_create_suspension(SUSP_ARITHMETIC, constraint->dependency_vars, 
                                                      constraint->dependency_count, comp);
            
            printf("[DEBUG] Adding arithmetic constraint suspension to variable %llu\n", constraint->dependency_vars[i]);
            flint_add_suspension_to_var(var, susp);
        } else {
            printf("[DEBUG] Failed to find variable %llu for suspension!\n", constraint->dependency_vars[i]);
        }
    }
    
    printf("[DEBUG] Arithmetic constraint suspended on %zu variables\n", constraint->dependency_count);
    return true;
}

void flint_check_pending_constraints_for_var(VarId var_id, Environment* env) {
    printf("[DEBUG] Checking pending constraints for variable %llu\n", var_id);
    
    LogicalVar* var = flint_lookup_variable(env, var_id);
    if (!var || !var->waiters) {
        return;
    }
    
    // Process all suspensions waiting on this variable
    Suspension* current = var->waiters;
    Suspension* prev = NULL;
    
    while (current) {
        Suspension* next = current->next;
        
        if (current->is_active && current->type == SUSP_ARITHMETIC) {
            printf("[DEBUG] Found arithmetic suspension on variable %llu\n", var_id);
            
            SuspensionComputation* comp = (SuspensionComputation*)current->computation;
            ArithmeticConstraint* constraint = (ArithmeticConstraint*)comp->data;
            
            // Try to solve the constraint
            if (flint_solve_pending_arithmetic_constraint(constraint, env)) {
                printf("[DEBUG] Successfully solved pending arithmetic constraint\n");
                
                // Remove this suspension from the list
                if (prev) {
                    prev->next = next;
                } else {
                    var->waiters = next;
                }
                
                // Mark as inactive and clean up
                current->is_active = false;
                flint_free_arithmetic_constraint(constraint);
                flint_free(comp);
                flint_free(current);
                
                current = next;
                continue;
            } else {
                printf("[DEBUG] Arithmetic constraint still cannot be solved\n");
            }
        }
        
        prev = current;
        current = next;
    }
}

void flint_check_all_pending_constraints(Environment* env) {
    printf("[DEBUG] Checking all pending arithmetic constraints\n");
    
    if (!env || !global_logic_vars) {
        printf("[DEBUG] No environment or logic vars to check\n");
        return;
    }
    
    printf("[DEBUG] Checking %zu global logic variables for pending constraints\n", global_logic_var_count);
    
    // Check all logical variables for pending arithmetic constraints
    for (size_t i = 0; i < global_logic_var_count; i++) {
        Value* val = global_logic_vars[i];
        if (val && val->type == VAL_LOGICAL_VAR) {
            LogicalVar* var = val->data.logical_var;
            printf("[DEBUG] Checking logic var %zu (id=%llu) for waiters (var=%p)\n", i, var->id, var);
            
            if (var && var->waiters) {
                printf("[DEBUG] Found waiters on variable %llu (waiters=%p)\n", var->id, var->waiters);
                Suspension* current = var->waiters;
                while (current) {
                    printf("[DEBUG] Checking suspension type: %d, active: %s\n", current->type, current->is_active ? "yes" : "no");
                    
                    if (current->is_active && current->type == SUSP_ARITHMETIC) {
                        SuspensionComputation* comp = (SuspensionComputation*)current->computation;
                        ArithmeticConstraint* constraint = (ArithmeticConstraint*)comp->data;
                        
                        printf("[DEBUG] Checking constraint on variable %llu: %s\n", var->id, constraint->operation);
                        printf("[DEBUG] Left: %p (type=%d), Right: %p (type=%d), Result: %p (type=%d)\n",
                               constraint->left, constraint->left ? constraint->left->type : -1,
                               constraint->right, constraint->right ? constraint->right->type : -1,
                               constraint->result, constraint->result ? constraint->result->type : -1);
                        
                        // Check dereferenced values too
                        Value* left_deref = flint_deref(constraint->left);
                        Value* right_deref = flint_deref(constraint->right);
                        Value* result_deref = flint_deref(constraint->result);
                        printf("[DEBUG] After deref - Left: %p (type=%d), Right: %p (type=%d), Result: %p (type=%d)\n",
                               left_deref, left_deref ? left_deref->type : -1,
                               right_deref, right_deref ? right_deref->type : -1,
                               result_deref, result_deref ? result_deref->type : -1);
                        
                        if (flint_solve_pending_arithmetic_constraint(constraint, env)) {
                            printf("[DEBUG] Solved constraint during global check\n");
                            // Mark as solved but don't remove here to avoid iterator issues
                            current->is_active = false;
                        }
                    }
                    current = current->next;
                }
            } else {
                printf("[DEBUG] No waiters on variable %llu (var=%p, waiters=%p)\n", var->id, var, var->waiters);
            }
        }
    }
}

// =============================================================================
// LOGIC PROGRAMMING SOLUTION GENERATORS
// =============================================================================

bool flint_generate_add_solutions(Value* left, Value* right, Value* result, Environment* env) {
    printf("[DEBUG] Generating solutions for add constraint\n");
    
    // Dereference to get current bindings
    left = flint_deref(left);
    right = flint_deref(right);
    result = flint_deref(result);
    
    bool left_concrete = (left->type == VAL_INTEGER);
    bool right_concrete = (right->type == VAL_INTEGER);
    bool result_concrete = (result->type == VAL_INTEGER);
    
    // Case: add(X, Y, Z) where one is concrete, generate solutions for the others
    if (result_concrete && !left_concrete && !right_concrete) {
        // add(X, Y, Result) - generate pairs that sum to Result
        int target = result->data.integer;
        printf("[DEBUG] Generating pairs that sum to %d\n", target);
        
        // For now, generate a simple solution: X=0, Y=target
        // In a full logic programming system, this would generate all possible pairs
        Value* left_val = flint_create_integer(0);
        Value* right_val = flint_create_integer(target);
        
        bool left_unified = flint_unify(left, left_val, env);
        bool right_unified = flint_unify(right, right_val, env);
        
        printf("[DEBUG] Generated solution: 0 + %d = %d (unified: %s, %s)\n", 
               target, target, left_unified ? "yes" : "no", right_unified ? "yes" : "no");
        
        return left_unified && right_unified;
    }
    
    if (left_concrete && !right_concrete && !result_concrete) {
        // add(Left, Y, Z) - generate pairs where Left + Y = Z
        int left_val = left->data.integer;
        printf("[DEBUG] Generating solutions for %d + Y = Z\n", left_val);
        
        // Generate a solution: Y=5, Z=Left+5 (arbitrary choice)
        Value* right_val = flint_create_integer(5);
        Value* result_val = flint_create_integer(left_val + 5);
        
        bool right_unified = flint_unify(right, right_val, env);
        bool result_unified = flint_unify(result, result_val, env);
        
        printf("[DEBUG] Generated solution: %d + 5 = %d (unified: %s, %s)\n", 
               left_val, left_val + 5, right_unified ? "yes" : "no", result_unified ? "yes" : "no");
        
        return right_unified && result_unified;
    }
    
    if (!left_concrete && right_concrete && !result_concrete) {
        // add(X, Right, Z) - generate pairs where X + Right = Z
        int right_val = right->data.integer;
        printf("[DEBUG] Generating solutions for X + %d = Z\n", right_val);
        
        // Generate a solution: X=5, Z=5+Right (arbitrary choice)
        Value* left_val = flint_create_integer(5);
        Value* result_val = flint_create_integer(5 + right_val);
        
        bool left_unified = flint_unify(left, left_val, env);
        bool result_unified = flint_unify(result, result_val, env);
        
        printf("[DEBUG] Generated solution: 5 + %d = %d (unified: %s, %s)\n", 
               right_val, 5 + right_val, left_unified ? "yes" : "no", result_unified ? "yes" : "no");
        
        return left_unified && result_unified;
    }
    
    if (!left_concrete && !right_concrete && !result_concrete) {
        // add(X, Y, Z) - all variables, generate an arbitrary solution
        printf("[DEBUG] Generating arbitrary solution for add(X, Y, Z)\n");
        
        // Generate: X=3, Y=4, Z=7
        Value* left_val = flint_create_integer(3);
        Value* right_val = flint_create_integer(4);
        Value* result_val = flint_create_integer(7);
        
        bool left_unified = flint_unify(left, left_val, env);
        bool right_unified = flint_unify(right, right_val, env);
        bool result_unified = flint_unify(result, result_val, env);
        
        printf("[DEBUG] Generated solution: 3 + 4 = 7 (unified: %s, %s, %s)\n", 
               left_unified ? "yes" : "no", right_unified ? "yes" : "no", result_unified ? "yes" : "no");
        
        return left_unified && right_unified && result_unified;
    }
    
    printf("[DEBUG] No solution generation pattern matched for add constraint\n");
    return false;
}

bool flint_generate_subtract_solutions(Value* left, Value* right, Value* result, Environment* env) {
    printf("[DEBUG] Generating solutions for subtract constraint\n");
    
    // Similar logic for subtraction - for now, just a simple implementation
    left = flint_deref(left);
    right = flint_deref(right);
    result = flint_deref(result);
    
    if (result->type == VAL_INTEGER && left->type == VAL_LOGICAL_VAR && right->type == VAL_LOGICAL_VAR) {
        int target = result->data.integer;
        Value* left_val = flint_create_integer(target + 5);  // X - Y = target, so X = target + Y, let Y=5
        Value* right_val = flint_create_integer(5);
        
        return flint_unify(left, left_val, env) && flint_unify(right, right_val, env);
    }
    
    return false;
}

bool flint_generate_multiply_solutions(Value* left, Value* right, Value* result, Environment* env) {
    printf("[DEBUG] Generating solutions for multiply constraint\n");
    
    // Similar logic for multiplication
    left = flint_deref(left);
    right = flint_deref(right);
    result = flint_deref(result);
    
    if (result->type == VAL_INTEGER && left->type == VAL_LOGICAL_VAR && right->type == VAL_LOGICAL_VAR) {
        int target = result->data.integer;
        // Find factors of target
        if (target != 0) {
            Value* left_val = flint_create_integer(1);
            Value* right_val = flint_create_integer(target);
            
            return flint_unify(left, left_val, env) && flint_unify(right, right_val, env);
        }
    }
    
    return false;
}
