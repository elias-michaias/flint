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
    // append(list1, list2, result)
    Value* list1 = flint_deref(args[0]);
    Value* list2 = flint_deref(args[1]);
    Value* result = flint_deref(args[2]);
    
    // Case 1: append([], Ys, Ys)
    if (list1->type == VAL_LIST && list1->data.list.length == 0) {
        if (flint_unify(list2, result, env)) {
            return result;
        }
        return NULL;
    }
    
    // Case 2: append([H|T], Ys, [H|R]) :- append(T, Ys, R)
    if (list1->type == VAL_LIST && list1->data.list.length > 0) {
        // Extract head and tail
        Value* head = &list1->data.list.elements[0];
        
        // Create tail list
        Value* tail = flint_alloc(sizeof(Value));
        tail->type = VAL_LIST;
        tail->data.list.length = list1->data.list.length - 1;
        tail->data.list.capacity = tail->data.list.length;
        
        if (tail->data.list.length > 0) {
            tail->data.list.elements = flint_alloc(sizeof(Value) * tail->data.list.length);
            for (size_t i = 0; i < tail->data.list.length; i++) {
                tail->data.list.elements[i] = list1->data.list.elements[i + 1];
            }
        } else {
            tail->data.list.elements = NULL;
        }
        
        // Create result with head prepended
        if (result->type == VAL_LOGICAL_VAR) {
            // Result is a variable, create the structure
            Value* new_result = flint_alloc(sizeof(Value));
            new_result->type = VAL_LIST;
            new_result->data.list.length = 1;  // We'll extend this recursively
            new_result->data.list.capacity = 1;
            new_result->data.list.elements = flint_alloc(sizeof(Value));
            new_result->data.list.elements[0] = *head;
            
            // Recursive call for the tail
            Value* tail_result = flint_create_logical_var(false);
            Value* recursive_args[] = {tail, list2, tail_result};
            Value* tail_append_result = narrow_append(recursive_args, 3, env);
            
            if (tail_append_result) {
                // Extend the result list
                tail_append_result = flint_deref(tail_append_result);
                if (tail_append_result->type == VAL_LIST) {
                    size_t new_length = 1 + tail_append_result->data.list.length;
                    new_result->data.list.elements = realloc(new_result->data.list.elements, 
                                                            sizeof(Value) * new_length);
                    new_result->data.list.length = new_length;
                    new_result->data.list.capacity = new_length;
                    
                    for (size_t i = 0; i < tail_append_result->data.list.length; i++) {
                        new_result->data.list.elements[i + 1] = tail_append_result->data.list.elements[i];
                    }
                }
            }
            
            if (flint_unify(result, new_result, env)) {
                return result;
            }
        }
    }
    
    return NULL;
}

Value* narrow_reverse(Value** args, size_t arg_count, Environment* env) {
    // reverse(list, result)
    Value* list = flint_deref(args[0]);
    Value* result = flint_deref(args[1]);
    
    if (list->type != VAL_LIST) {
        return NULL;
    }
    
    // Create reversed list
    Value* reversed = flint_alloc(sizeof(Value));
    reversed->type = VAL_LIST;
    reversed->data.list.length = list->data.list.length;
    reversed->data.list.capacity = list->data.list.length;
    
    if (list->data.list.length > 0) {
        reversed->data.list.elements = flint_alloc(sizeof(Value) * list->data.list.length);
        for (size_t i = 0; i < list->data.list.length; i++) {
            reversed->data.list.elements[i] = list->data.list.elements[list->data.list.length - 1 - i];
        }
    } else {
        reversed->data.list.elements = NULL;
    }
    
    if (flint_unify(result, reversed, env)) {
        return result;
    }
    
    return NULL;
}

Value* narrow_length(Value** args, size_t arg_count, Environment* env) {
    // length(list, result)
    Value* list = flint_deref(args[0]);
    Value* result = flint_deref(args[1]);
    
    if (list->type != VAL_LIST) {
        return NULL;
    }
    
    Value* length_val = flint_create_integer((int64_t)list->data.list.length);
    
    if (flint_unify(result, length_val, env)) {
        return result;
    }
    
    return NULL;
}
