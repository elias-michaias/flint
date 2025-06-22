#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// SUSPENSION MANAGEMENT
// =============================================================================

Suspension* flint_create_suspension(SuspensionType type, VarId* deps, size_t dep_count, void* computation) {
    Suspension* susp = flint_alloc(sizeof(Suspension));
    susp->type = type;
    susp->var_count = dep_count;
    susp->computation = computation;
    susp->next = NULL;
    susp->is_active = true;
    
    if (dep_count > 0) {
        susp->dependent_vars = flint_alloc(sizeof(VarId) * dep_count);
        memcpy(susp->dependent_vars, deps, sizeof(VarId) * dep_count);
    } else {
        susp->dependent_vars = NULL;
    }
    
    return susp;
}

void flint_add_suspension_to_var(LogicalVar* var, Suspension* susp) {
    // Add suspension to the front of the linked list
    susp->next = var->waiters;
    var->waiters = susp;
}

void flint_resume_suspensions(VarId var_id, Environment* env) {
    // Find the variable in the environment
    LogicalVar* var = flint_lookup_variable(env, var_id);
    if (!var || !var->waiters) {
        return;
    }
    
    // Process all suspensions waiting on this variable
    Suspension* current = var->waiters;
    Suspension* active_suspensions = NULL;
    
    while (current) {
        Suspension* next = current->next;
        
        if (current->is_active) {
            // Check if all dependencies are satisfied
            bool can_resume = true;
            for (size_t i = 0; i < current->var_count; i++) {
                LogicalVar* dep_var = flint_lookup_variable(env, current->dependent_vars[i]);
                if (!dep_var || !dep_var->binding) {
                    can_resume = false;
                    break;
                }
            }
            
            if (can_resume) {
                // Resume this suspension
                switch (current->type) {
                    case SUSP_UNIFICATION: {
                        // Resume delayed unification
                        Value** unify_args = (Value**)current->computation;
                        flint_unify(unify_args[0], unify_args[1], env);
                        current->is_active = false;
                        break;
                    }
                    
                    case SUSP_FUNCTION_CALL: {
                        // Resume delayed function call
                        // Implementation depends on function call structure
                        current->is_active = false;
                        break;
                    }
                    
                    case SUSP_NARROWING: {
                        // Resume narrowing operation
                        current->is_active = false;
                        break;
                    }
                    
                    case SUSP_CONSTRAINT: {
                        // Resume constraint checking
                        current->is_active = false;
                        break;
                    }
                }
            } else {
                // Keep this suspension active
                current->next = active_suspensions;
                active_suspensions = current;
            }
        }
        
        current = next;
    }
    
    // Update the variable's suspension list to only include active ones
    var->waiters = active_suspensions;
}

// =============================================================================
// NARROWING ENGINE
// =============================================================================

// Function table for built-in narrowing functions
typedef struct {
    char* name;
    NarrowingFunc func;
    int arity;
} NarrowingFuncEntry;

// Forward declarations of built-in functions
Value* narrow_append(Value** args, size_t arg_count, Environment* env);
Value* narrow_reverse(Value** args, size_t arg_count, Environment* env);
Value* narrow_length(Value** args, size_t arg_count, Environment* env);

// Built-in function table
static NarrowingFuncEntry builtin_functions[] = {
    {"append", narrow_append, 3},      // append(list1, list2, result)
    {"reverse", narrow_reverse, 2},    // reverse(list, result)
    {"length", narrow_length, 2},      // length(list, result)
    {NULL, NULL, 0}
};

NarrowingFuncEntry* find_narrowing_function(const char* name) {
    for (int i = 0; builtin_functions[i].name != NULL; i++) {
        if (strcmp(builtin_functions[i].name, name) == 0) {
            return &builtin_functions[i];
        }
    }
    return NULL;
}

Value* flint_narrow_call(char* func_name, Value** args, size_t arg_count, Environment* env) {
    NarrowingFuncEntry* func_entry = find_narrowing_function(func_name);
    if (!func_entry) {
        printf("Error: Unknown function '%s'\n", func_name);
        return NULL;
    }
    
    if (arg_count != func_entry->arity) {
        printf("Error: Function '%s' expects %d arguments, got %zu\n", 
               func_name, func_entry->arity, arg_count);
        return NULL;
    }
    
    // For now, let's try to evaluate all functions immediately
    // Later we can add more sophisticated suspension logic based on function requirements
    return func_entry->func(args, arg_count, env);
}

// =============================================================================
// BUILT-IN NARROWING FUNCTIONS
// =============================================================================

Value* narrow_append(Value** args, size_t arg_count, Environment* env) {
    return flint_list_narrow_append(args, arg_count, env);
}

Value* narrow_reverse(Value** args, size_t arg_count, Environment* env) {
    return flint_list_narrow_reverse(args, arg_count, env);
}

Value* narrow_length(Value** args, size_t arg_count, Environment* env) {
    return flint_list_narrow_length(args, arg_count, env);
}
