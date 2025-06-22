#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// UNIFICATION ENGINE
// =============================================================================

Value* flint_deref(Value* val) {
    if (!val) return NULL;
    
    if (val->type == VAL_LOGICAL_VAR) {
        LogicalVar* var = val->data.logical_var;
        if (var->binding) {
            // Follow the binding chain
            return flint_deref(var->binding);
        }
    }
    
    return val;
}

bool flint_occurs_check(VarId var_id, Value* val) {
    if (!val) return false;
    
    val = flint_deref(val);
    
    switch (val->type) {
        case VAL_LOGICAL_VAR: {
            LogicalVar* var = val->data.logical_var;
            return var->id == var_id;
        }
        
        case VAL_LIST:
            for (size_t i = 0; i < val->data.list.length; i++) {
                if (flint_occurs_check(var_id, &val->data.list.elements[i])) {
                    return true;
                }
            }
            return false;
            
        case VAL_RECORD:
            for (size_t i = 0; i < val->data.record.field_count; i++) {
                if (flint_occurs_check(var_id, &val->data.record.field_values[i])) {
                    return true;
                }
            }
            return false;
            
        case VAL_PARTIAL:
            return flint_occurs_check(var_id, val->data.partial.base);
            
        default:
            return false;
    }
}

bool flint_unify_variables(LogicalVar* var1, LogicalVar* var2, Environment* env) {
    // Handle variable-variable unification
    if (var1->id == var2->id) {
        return true;  // Same variable
    }
    
    // Choose which variable to bind (prefer binding the younger variable)
    LogicalVar* bind_var = (var1->id < var2->id) ? var2 : var1;
    LogicalVar* bind_to = (var1->id < var2->id) ? var1 : var2;
    
    // Create a value wrapper for the target variable
    Value* target_val = flint_alloc(sizeof(Value));
    target_val->type = VAL_LOGICAL_VAR;
    target_val->data.logical_var = bind_to;
    
    // Bind the variable
    bind_var->binding = target_val;
    
    // Resume any suspensions waiting on the bound variable
    flint_resume_suspensions(bind_var->id, env);
    
    return true;
}

bool flint_unify_lists(Value* list1, Value* list2, Environment* env) {
    if (list1->data.list.length != list2->data.list.length) {
        return false;
    }
    
    for (size_t i = 0; i < list1->data.list.length; i++) {
        if (!flint_unify(&list1->data.list.elements[i], &list2->data.list.elements[i], env)) {
            return false;
        }
    }
    
    return true;
}

bool flint_unify_records(Value* rec1, Value* rec2, Environment* env) {
    if (rec1->data.record.field_count != rec2->data.record.field_count) {
        return false;
    }
    
    // Check that all field names match (order matters for now - could be improved)
    for (size_t i = 0; i < rec1->data.record.field_count; i++) {
        if (strcmp(rec1->data.record.field_names[i], rec2->data.record.field_names[i]) != 0) {
            return false;
        }
        
        if (!flint_unify(&rec1->data.record.field_values[i], &rec2->data.record.field_values[i], env)) {
            return false;
        }
    }
    
    return true;
}

bool flint_unify(Value* val1, Value* val2, Environment* env) {
    if (!val1 || !val2) return false;
    
    // Dereference both values to get their actual values
    val1 = flint_deref(val1);
    val2 = flint_deref(val2);
    
    // Handle logical variables
    if (val1->type == VAL_LOGICAL_VAR && val2->type == VAL_LOGICAL_VAR) {
        return flint_unify_variables(val1->data.logical_var, val2->data.logical_var, env);
    }
    
    if (val1->type == VAL_LOGICAL_VAR) {
        LogicalVar* var = val1->data.logical_var;
        
        // Occurs check
        if (flint_occurs_check(var->id, val2)) {
            return false;
        }
        
        // Bind the variable
        var->binding = val2;
        
        // Resume suspensions
        flint_resume_suspensions(var->id, env);
        
        return true;
    }
    
    if (val2->type == VAL_LOGICAL_VAR) {
        LogicalVar* var = val2->data.logical_var;
        
        // Occurs check
        if (flint_occurs_check(var->id, val1)) {
            return false;
        }
        
        // Bind the variable
        var->binding = val1;
        
        // Resume suspensions
        flint_resume_suspensions(var->id, env);
        
        return true;
    }
    
    // Handle unification of ground terms
    if (val1->type != val2->type) {
        return false;
    }
    
    switch (val1->type) {
        case VAL_INTEGER:
            return val1->data.integer == val2->data.integer;
            
        case VAL_FLOAT:
            return val1->data.float_val == val2->data.float_val;
            
        case VAL_STRING:
            return strcmp(val1->data.string, val2->data.string) == 0;
            
        case VAL_ATOM:
            return strcmp(val1->data.atom, val2->data.atom) == 0;
            
        case VAL_LIST:
            return flint_unify_lists(val1, val2, env);
            
        case VAL_RECORD:
            return flint_unify_records(val1, val2, env);
            
        case VAL_SUSPENSION:
            // Suspensions don't unify directly
            return false;
            
        case VAL_PARTIAL:
            // Partial structures require special handling
            return flint_unify(val1->data.partial.base, val2->data.partial.base, env);
            
        default:
            return false;
    }
}

bool flint_can_unify(Value* val1, Value* val2) {
    // This is a non-destructive check - we don't actually perform the unification
    // For now, we'll use a simple heuristic
    
    val1 = flint_deref(val1);
    val2 = flint_deref(val2);
    
    // Variables can unify with anything (subject to occurs check)
    if (val1->type == VAL_LOGICAL_VAR || val2->type == VAL_LOGICAL_VAR) {
        return true;
    }
    
    // Same type ground terms might unify
    if (val1->type == val2->type) {
        switch (val1->type) {
            case VAL_INTEGER:
                return val1->data.integer == val2->data.integer;
            case VAL_FLOAT:
                return val1->data.float_val == val2->data.float_val;
            case VAL_STRING:
                return strcmp(val1->data.string, val2->data.string) == 0;
            case VAL_ATOM:
                return strcmp(val1->data.atom, val2->data.atom) == 0;
            case VAL_LIST:
                return val1->data.list.length == val2->data.list.length;
            case VAL_RECORD:
                return val1->data.record.field_count == val2->data.record.field_count;
            default:
                return false;
        }
    }
    
    return false;
}

// =============================================================================
// FREE VARIABLE EXTRACTION
// =============================================================================

static void collect_free_vars_recursive(Value* val, VarId** vars, size_t* count, size_t* capacity) {
    if (!val) return;
    
    val = flint_deref(val);
    
    if (val->type == VAL_LOGICAL_VAR) {
        LogicalVar* var = val->data.logical_var;
        if (!var->binding) {  // Only uninstantiated variables
            // Check if we already have this variable
            for (size_t i = 0; i < *count; i++) {
                if ((*vars)[i] == var->id) {
                    return;  // Already in the list
                }
            }
            
            // Expand array if needed
            if (*count >= *capacity) {
                *capacity = (*capacity == 0) ? 8 : (*capacity * 2);
                *vars = realloc(*vars, sizeof(VarId) * (*capacity));
            }
            
            (*vars)[*count] = var->id;
            (*count)++;
        }
        return;
    }
    
    // Recursively collect from compound structures
    switch (val->type) {
        case VAL_LIST:
            for (size_t i = 0; i < val->data.list.length; i++) {
                collect_free_vars_recursive(&val->data.list.elements[i], vars, count, capacity);
            }
            break;
            
        case VAL_RECORD:
            for (size_t i = 0; i < val->data.record.field_count; i++) {
                collect_free_vars_recursive(&val->data.record.field_values[i], vars, count, capacity);
            }
            break;
            
        case VAL_PARTIAL:
            collect_free_vars_recursive(val->data.partial.base, vars, count, capacity);
            break;
            
        default:
            break;
    }
}

VarId* flint_get_free_vars(Value* val, size_t* count) {
    VarId* vars = NULL;
    size_t capacity = 0;
    *count = 0;
    
    collect_free_vars_recursive(val, &vars, count, &capacity);
    
    return vars;
}
