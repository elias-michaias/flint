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
    printf("[DEBUG] flint_add_suspension_to_var: Adding suspension %p to variable %llu (var=%p)\n", 
           susp, var->id, var);
    printf("[DEBUG] flint_add_suspension_to_var: Current waiters list: %p\n", var->waiters);
    
    // Add suspension to the front of the linked list
    susp->next = var->waiters;
    var->waiters = susp;
    
    printf("[DEBUG] flint_add_suspension_to_var: Updated waiters list: %p, suspension next: %p\n", 
           var->waiters, susp->next);
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
                    
                    case SUSP_ARITHMETIC: {
                        // Resume arithmetic constraint checking
                        // This is handled by flint_check_pending_constraints_for_var
                        // which is called from the unification process
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
// LAZY EVALUATION SUPPORT
// =============================================================================

Value* flint_create_suspended_value(Suspension* suspension) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_SUSPENSION;
    value->is_consumed = false;
    value->consumption_count = 0;
    value->data.suspension = suspension;
    return value;
}

Value* flint_force_value(Value* value) {
    if (!value) return NULL;
    
    // If it's not a suspension, return as-is
    if (value->type != VAL_SUSPENSION) {
        return value;
    }
    
    Suspension* susp = value->data.suspension;
    if (!susp || !susp->is_active) {
        return value; // Can't force inactive suspension
    }
    
    // Try to evaluate the suspension
    SuspensionComputation* comp = (SuspensionComputation*)susp->computation;
    if (!comp) return value;
    
    switch (comp->type) {
        case SUSP_FUNCTION_CALL: {
            // For arithmetic operations, check if operands are bound
            if (comp->operand_count >= 2) {
                Value* left = flint_force_value(comp->operands[0]);
                Value* right = flint_force_value(comp->operands[1]);
                
                // Check if both operands are now concrete values
                if (left && left->type == VAL_INTEGER && 
                    right && right->type == VAL_INTEGER) {
                    
                    // Perform the operation
                    if (strcmp(comp->function_name, "add") == 0) {
                        Value* result = flint_create_integer(left->data.integer + right->data.integer);
                        susp->is_active = false; // Mark suspension as resolved
                        return result;
                    } else if (strcmp(comp->function_name, "sub") == 0) {
                        Value* result = flint_create_integer(left->data.integer - right->data.integer);
                        susp->is_active = false;
                        return result;
                    } else if (strcmp(comp->function_name, "mul") == 0) {
                        Value* result = flint_create_integer(left->data.integer * right->data.integer);
                        susp->is_active = false;
                        return result;
                    } else if (strcmp(comp->function_name, "div") == 0) {
                        if (right->data.integer != 0) {
                            Value* result = flint_create_integer(left->data.integer / right->data.integer);
                            susp->is_active = false;
                            return result;
                        }
                    }
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    // If we can't evaluate the suspension, return the original suspended value
    return value;
}

// =============================================================================
// LAZY EVALUATION / SUSPENSION SUPPORT
// =============================================================================

// Create an arithmetic suspension (for binary operations like x + 5)
Value* flint_create_arithmetic_suspension(const char* op, Value* left, Value* right) {
    if (!op || !left || !right) {
        fprintf(stderr, "[ERROR] flint_create_arithmetic_suspension: null arguments\n");
        return flint_create_integer(0);
    }
    
    printf("[DEBUG] Creating arithmetic suspension: %s with left=%p, right=%p\n", op, left, right);
    
    // For now, if left is bound and right is bound, evaluate immediately
    if (left->type != VAL_LOGICAL_VAR && right->type != VAL_LOGICAL_VAR) {
        if (strcmp(op, "add") == 0) {
            return flint_create_integer(flint_deref(left)->data.integer + flint_deref(right)->data.integer);
        } else if (strcmp(op, "sub") == 0) {
            return flint_create_integer(flint_deref(left)->data.integer - flint_deref(right)->data.integer);
        } else if (strcmp(op, "mul") == 0) {
            return flint_create_integer(flint_deref(left)->data.integer * flint_deref(right)->data.integer);
        } else if (strcmp(op, "div") == 0) {
            return flint_create_integer(flint_deref(left)->data.integer / flint_deref(right)->data.integer);
        }
    }
    
    // Create a suspension
    Value* suspension_val = flint_alloc(sizeof(Value));
    suspension_val->type = VAL_SUSPENSION;
    suspension_val->is_consumed = false;
    suspension_val->consumption_count = 0;
    
    // Create the suspension computation
    SuspensionComputation* comp = flint_alloc(sizeof(SuspensionComputation));
    comp->type = SUSP_FUNCTION_CALL;
    comp->function_name = flint_alloc(strlen(op) + 1);
    strcpy(comp->function_name, op);
    comp->operand_count = 2;
    comp->operands = flint_alloc(sizeof(Value*) * 2);
    comp->operands[0] = left;
    comp->operands[1] = right;
    comp->expr_code = NULL;
    comp->data = NULL;
    
    // Create the actual suspension
    VarId deps[2];
    size_t dep_count = 0;
    
    if (left->type == VAL_LOGICAL_VAR) {
        deps[dep_count++] = left->data.logical_var->id;
    }
    if (right->type == VAL_LOGICAL_VAR) {
        deps[dep_count++] = right->data.logical_var->id;
    }
    
    Suspension* susp = flint_create_suspension(SUSP_FUNCTION_CALL, dep_count > 0 ? deps : NULL, dep_count, comp);
    suspension_val->data.suspension = susp;
    
    // If there are dependencies, add this suspension to the waiting lists
    if (dep_count > 0) {
        Environment* env = flint_get_global_env();
        for (size_t i = 0; i < dep_count; i++) {
            LogicalVar* var = flint_lookup_variable(env, deps[i]);
            if (var) {
                flint_add_suspension_to_var(var, susp);
            }
        }
    }
    
    return suspension_val;
}

// Create a function call suspension
Value* flint_create_function_call_suspension(const char* func_name, Value* args[], size_t arg_count) {
    if (!func_name) {
        fprintf(stderr, "[ERROR] flint_create_function_call_suspension: null function name\n");
        return flint_create_integer(0);
    }
    
    printf("[DEBUG] Creating function call suspension: %s with %zu args\n", func_name, arg_count);
    
    // For now, create a simple suspension value
    Value* suspension_val = flint_alloc(sizeof(Value));
    suspension_val->type = VAL_SUSPENSION;
    suspension_val->is_consumed = false;
    suspension_val->consumption_count = 0;
    
    // Create a basic suspension - for now this just represents the function call
    Suspension* susp = flint_create_suspension(SUSP_FUNCTION_CALL, NULL, 0, (void*)func_name);
    suspension_val->data.suspension = susp;
    
    return suspension_val;
}

// Create a generic suspension
Value* flint_create_generic_suspension() {
    printf("[DEBUG] Creating generic suspension\n");
    
    Value* suspension_val = flint_alloc(sizeof(Value));
    suspension_val->type = VAL_SUSPENSION;
    suspension_val->is_consumed = false;
    suspension_val->consumption_count = 0;
    
    Suspension* susp = flint_create_suspension(SUSP_FUNCTION_CALL, NULL, 0, NULL);
    suspension_val->data.suspension = susp;
    
    return suspension_val;
}


