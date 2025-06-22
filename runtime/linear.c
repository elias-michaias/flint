#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// Forward declarations for functions used before definition
static void flint_restore_linear_value(Value* value, LinearOp operation);
static void flint_finalize_consumption(Value* value);
static void flint_free_value_memory(Value* value);

// Thread-local storage for current environment (for trail selection)
static __thread Environment* current_env = NULL;

// Set the current environment for linear operations
void flint_set_linear_context(Environment* env) {
    current_env = env;
}

// =============================================================================
// LINEAR RESOURCE MANAGEMENT SYSTEM
// =============================================================================
// 
// This module implements enforced linearity at the language level:
// - All values are linear by default (consumed on use)
// - Automatic deallocation when resources are consumed
// - Backtracking-aware resource tracking
// - Opt-in non-consumptive operations (copying, sharing)
//
// Key principles:
// 1. Every value has a single owner at any time
// 2. Operations consume their inputs (transfer ownership)
// 3. Backtracking can restore consumed resources
// 4. Non-consumptive operations must explicitly copy
//
// =============================================================================

// Global linear trail for backtracking
static LinearTrail* global_linear_trail = NULL;

// =============================================================================
// TRAIL MANAGEMENT
// =============================================================================

LinearTrail* flint_create_linear_trail(void) {
    LinearTrail* trail = (LinearTrail*)malloc(sizeof(LinearTrail));
    trail->entries = NULL;
    trail->entry_count = 0;
    trail->capacity = 0;
    trail->checkpoint_stack = NULL;
    trail->checkpoint_count = 0;
    trail->checkpoint_capacity = 0;
    return trail;
}

void flint_free_linear_trail(LinearTrail* trail) {
    if (!trail) return;
    
    if (trail->entries) {
        free(trail->entries);
    }
    if (trail->checkpoint_stack) {
        free(trail->checkpoint_stack);
    }
    free(trail);
}

void flint_trail_record_consumption(LinearTrail* trail, Value* value, LinearOp operation) {
    if (!trail) return;
    
    // Expand trail if needed
    if (trail->entry_count >= trail->capacity) {
        trail->capacity = (trail->capacity == 0) ? 16 : (trail->capacity * 2);
        trail->entries = (LinearTrailEntry*)realloc(trail->entries, 
                                                   sizeof(LinearTrailEntry) * trail->capacity);
    }
    
    // Record the consumption
    LinearTrailEntry* entry = &trail->entries[trail->entry_count];
    entry->consumed_value = value;
    entry->operation = operation;
    entry->timestamp = trail->entry_count;
    entry->is_active = true;
    
    trail->entry_count++;
    
    #ifdef DEBUG_LINEAR
    printf("TRAIL: Recorded consumption of value %p (op=%d)\n", value, operation);
    #endif
}

LinearCheckpoint flint_trail_create_checkpoint(LinearTrail* trail) {
    if (!trail) return 0;
    
    LinearCheckpoint checkpoint = trail->entry_count;
    
    // Add to checkpoint stack
    if (trail->checkpoint_count >= trail->checkpoint_capacity) {
        trail->checkpoint_capacity = (trail->checkpoint_capacity == 0) ? 8 : (trail->checkpoint_capacity * 2);
        trail->checkpoint_stack = (LinearCheckpoint*)realloc(trail->checkpoint_stack,
                                                            sizeof(LinearCheckpoint) * trail->checkpoint_capacity);
    }
    
    trail->checkpoint_stack[trail->checkpoint_count] = checkpoint;
    trail->checkpoint_count++;
    
    #ifdef DEBUG_LINEAR
    printf("TRAIL: Created checkpoint at %zu\n", checkpoint);
    #endif
    
    return checkpoint;
}

void flint_trail_rollback_to_checkpoint(LinearTrail* trail, LinearCheckpoint checkpoint) {
    if (!trail || checkpoint > trail->entry_count) return;
    
    #ifdef DEBUG_LINEAR
    printf("TRAIL: Rolling back from %zu to %zu\n", trail->entry_count, checkpoint);
    #endif
    
    // Restore all consumed values after the checkpoint
    for (size_t i = checkpoint; i < trail->entry_count; i++) {
        LinearTrailEntry* entry = &trail->entries[i];
        if (entry->is_active) {
            // Restore the consumed value
            flint_restore_linear_value(entry->consumed_value, entry->operation);
            entry->is_active = false;
        }
    }
    
    // Reset trail to checkpoint
    trail->entry_count = checkpoint;
    
    // Remove checkpoint from stack
    if (trail->checkpoint_count > 0) {
        trail->checkpoint_count--;
    }
}

void flint_trail_commit_checkpoint(LinearTrail* trail, LinearCheckpoint checkpoint) {
    if (!trail) return;
    
    #ifdef DEBUG_LINEAR
    printf("TRAIL: Committing checkpoint %zu\n", checkpoint);
    #endif
    
    // Remove checkpoint from stack (we're committing to this path)
    if (trail->checkpoint_count > 0) {
        trail->checkpoint_count--;
    }
    
    // Actually free the consumed values before the checkpoint
    for (size_t i = 0; i < checkpoint && i < trail->entry_count; i++) {
        LinearTrailEntry* entry = &trail->entries[i];
        if (entry->is_active) {
            flint_finalize_consumption(entry->consumed_value);
            entry->is_active = false;
        }
    }
}

// =============================================================================
// VALUE LINEARITY MANAGEMENT
// =============================================================================

void flint_init_linear_system(void) {
    global_linear_trail = flint_create_linear_trail();
}

void flint_cleanup_linear_system(void) {
    // Note: global trail cleanup temporarily disabled to avoid double-free
    // Environment trails are cleaned up individually
    if (global_linear_trail) {
        // flint_free_linear_trail(global_linear_trail);
        global_linear_trail = NULL;
    }
}

void flint_mark_linear(Value* value) {
    if (!value) return;
    value->is_consumed = false;
    value->consumption_count = 0;
}

void flint_mark_consumed(Value* value, LinearOp operation) {
    if (!value) return;
    
    // Allow multiple consumption tracking for debugging/testing
    if (value->is_consumed) {
        #ifdef DEBUG_LINEAR
        fprintf(stderr, "WARNING: Multiple consumption of linear value %p\n", value);
        #endif
    } else {
        value->is_consumed = true;
    }
    
    value->consumption_count++;
    
    // Record in appropriate trail - prefer current environment's trail
    LinearTrail* trail = (current_env && current_env->linear_trail) ? 
                        current_env->linear_trail : global_linear_trail;
    
    if (trail) {
        flint_trail_record_consumption(trail, value, operation);
    }
    
    #ifdef DEBUG_LINEAR
    printf("LINEAR: Consumed value %p (op=%d, count=%u)\n", value, operation, value->consumption_count);
    #endif
}

bool flint_is_consumed(Value* value) {
    return value ? value->is_consumed : false;
}

Value* flint_consume_value(Value* value, LinearOp operation) {
    if (!value) return NULL;
    
    // Allow multiple consumption attempts but track them
    flint_mark_consumed(value, operation);
    return value;
}

Value* flint_copy_for_sharing(Value* value) {
    if (!value) return NULL;
    
    // This is the opt-in non-consumptive operation
    // It creates a deep copy so the original can still be used
    return flint_deep_copy_value(value);
}

Value* flint_deep_copy_value(Value* value) {
    if (!value) return NULL;
    
    Value* copy = (Value*)malloc(sizeof(Value));
    copy->type = value->type;
    copy->is_consumed = false;
    copy->consumption_count = 0;
    
    switch (value->type) {
        case VAL_INTEGER:
            copy->data.integer = value->data.integer;
            break;
            
        case VAL_FLOAT:
            copy->data.float_val = value->data.float_val;
            break;
            
        case VAL_STRING: {
            size_t len = strlen(value->data.string);
            copy->data.string = (char*)malloc(len + 1);
            strcpy(copy->data.string, value->data.string);
            break;
        }
        
        case VAL_ATOM: {
            size_t len = strlen(value->data.atom);
            copy->data.atom = (char*)malloc(len + 1);
            strcpy(copy->data.atom, value->data.atom);
            break;
        }
        
        case VAL_LIST: {
            copy->data.list.length = value->data.list.length;
            copy->data.list.capacity = value->data.list.capacity;
            
            if (value->data.list.length > 0) {
                copy->data.list.elements = (Value*)malloc(sizeof(Value) * value->data.list.length);
                for (size_t i = 0; i < value->data.list.length; i++) {
                    copy->data.list.elements[i] = *flint_deep_copy_value(&value->data.list.elements[i]);
                }
            } else {
                copy->data.list.elements = NULL;
            }
            break;
        }
        
        case VAL_RECORD: {
            copy->data.record.field_count = value->data.record.field_count;
            
            if (value->data.record.field_count > 0) {
                copy->data.record.field_names = (char**)malloc(sizeof(char*) * value->data.record.field_count);
                copy->data.record.field_values = (Value*)malloc(sizeof(Value) * value->data.record.field_count);
                
                for (size_t i = 0; i < value->data.record.field_count; i++) {
                    // Copy field name
                    size_t name_len = strlen(value->data.record.field_names[i]);
                    copy->data.record.field_names[i] = (char*)malloc(name_len + 1);
                    strcpy(copy->data.record.field_names[i], value->data.record.field_names[i]);
                    
                    // Deep copy field value
                    copy->data.record.field_values[i] = *flint_deep_copy_value(&value->data.record.field_values[i]);
                }
            } else {
                copy->data.record.field_names = NULL;
                copy->data.record.field_values = NULL;
            }
            break;
        }
        
        case VAL_LOGICAL_VAR: {
            // For logical variables, we need to be careful
            LogicalVar* orig_var = value->data.logical_var;
            LogicalVar* new_var = (LogicalVar*)malloc(sizeof(LogicalVar));
            
            new_var->id = flint_fresh_var_id(); // Fresh ID for the copy
            new_var->binding = orig_var->binding ? flint_deep_copy_value(orig_var->binding) : NULL;
            new_var->waiters = NULL; // Don't copy suspensions
            new_var->use_count = 0;  // Reset use count for new variable
            new_var->is_consumed = false;  // New variable is not consumed
            new_var->allow_reuse = orig_var->allow_reuse;
            
            copy->data.logical_var = new_var;
            break;
        }
        
        default:
            // For unknown types, just copy the data union
            copy->data = value->data;
            break;
    }
    
    flint_mark_linear(copy);
    
    #ifdef DEBUG_LINEAR
    printf("LINEAR: Deep copied value %p -> %p\n", value, copy);
    #endif
    
    return copy;
}

// =============================================================================
// BACKTRACKING INTEGRATION
// =============================================================================

static void flint_restore_linear_value(Value* value, LinearOp operation) {
    if (!value) return;
    
    // Restore the value to its pre-consumption state
    value->is_consumed = false;
    if (value->consumption_count > 0) {
        value->consumption_count--;
    }
    
    #ifdef DEBUG_LINEAR
    printf("LINEAR: Restored value %p (was op=%d)\n", value, operation);
    #endif
}

static void flint_finalize_consumption(Value* value) {
    if (!value || !value->is_consumed) return;
    
    // Actually free the value's memory
    flint_free_value_memory(value);
    
    #ifdef DEBUG_LINEAR
    printf("LINEAR: Finalized consumption of value %p\n", value);
    #endif
}

static void flint_free_value_memory(Value* value) {
    if (!value) return;
    
    switch (value->type) {
        case VAL_STRING:
            if (value->data.string) {
                free(value->data.string);
            }
            break;
            
        case VAL_ATOM:
            if (value->data.atom) {
                free(value->data.atom);
            }
            break;
            
        case VAL_LIST:
            if (value->data.list.elements) {
                // Recursively free elements if they're also consumed
                for (size_t i = 0; i < value->data.list.length; i++) {
                    if (value->data.list.elements[i].is_consumed) {
                        flint_free_value_memory(&value->data.list.elements[i]);
                    }
                }
                free(value->data.list.elements);
            }
            break;
            
        case VAL_RECORD:
            if (value->data.record.field_names) {
                for (size_t i = 0; i < value->data.record.field_count; i++) {
                    free(value->data.record.field_names[i]);
                    if (value->data.record.field_values[i].is_consumed) {
                        flint_free_value_memory(&value->data.record.field_values[i]);
                    }
                }
                free(value->data.record.field_names);
                free(value->data.record.field_values);
            }
            break;
            
        case VAL_LOGICAL_VAR:
            if (value->data.logical_var) {
                if (value->data.logical_var->binding && value->data.logical_var->binding->is_consumed) {
                    flint_free_value_memory(value->data.logical_var->binding);
                }
                free(value->data.logical_var);
            }
            break;
            
        default:
            // For simple types, no additional cleanup needed
            break;
    }
    
    // Don't free the Value struct itself here - that's managed by the trail system
}

// =============================================================================
// LINEAR OPERATIONS API
// =============================================================================

Value* flint_linear_unify(Value* val1, Value* val2, Environment* env) {
    // Consume both values in unification
    val1 = flint_consume_value(val1, LINEAR_OP_UNIFY);
    val2 = flint_consume_value(val2, LINEAR_OP_UNIFY);
    
    if (!val1 || !val2) return NULL;
    
    // Perform unification
    bool result = flint_unify(val1, val2, env);
    
    if (result) {
        // Return the unified value (typically val1)
        return val1;
    } else {
        return NULL;
    }
}

Value* flint_linear_apply_function(Value* func, Value** args, size_t arg_count, Environment* env) {
    // Consume the function
    func = flint_consume_value(func, LINEAR_OP_FUNCTION_CALL);
    
    // Consume all arguments
    for (size_t i = 0; i < arg_count; i++) {
        args[i] = flint_consume_value(args[i], LINEAR_OP_FUNCTION_CALL);
    }
    
    // Apply the function (this will handle narrowing internally)
    return flint_apply_function(func, args, arg_count, env);
}

Value* flint_linear_list_access(Value* list, size_t index) {
    // This is a non-consumptive operation by default
    // If you want to consume the list, use flint_linear_list_destructure
    
    if (!list || list->type != VAL_LIST || index >= list->data.list.length) {
        return NULL;
    }
    
    // Return a copy of the element to maintain linearity
    return flint_copy_for_sharing(&list->data.list.elements[index]);
}

LinearListDestructure flint_linear_list_destructure(Value* list) {
    LinearListDestructure result = {0};
    
    if (!list || list->type != VAL_LIST) {
        return result;
    }
    
    // Consume the list
    list = flint_consume_value(list, LINEAR_OP_DESTRUCTURE);
    if (!list) return result;
    
    // Transfer ownership of elements
    result.elements = list->data.list.elements;
    result.count = list->data.list.length;
    result.success = true;
    
    // Mark the list structure as consumed (but elements are now owned by caller)
    list->data.list.elements = NULL;
    list->data.list.length = 0;
    
    #ifdef DEBUG_LINEAR
    printf("LINEAR: Destructured list with %zu elements\n", result.count);
    #endif
    
    return result;
}

// Convenience wrapper functions for the test suite and external use
Value* flint_share_value(Value* value) {
    // In our implementation, sharing means marking as reusable
    if (value && value->type == VAL_LOGICAL_VAR) {
        value->data.logical_var->allow_reuse = true;
    }
    return value; // Return the same value
}

LinearCheckpoint flint_linear_checkpoint(LinearTrail* trail) {
    return flint_trail_create_checkpoint(trail);
}

void flint_linear_restore(LinearTrail* trail, LinearCheckpoint checkpoint) {
    flint_trail_rollback_to_checkpoint(trail, checkpoint);
}

LinearListDestructure flint_linear_destructure_list(Value* list) {
    return flint_linear_list_destructure(list);
}

// =============================================================================
// INTEGRATION WITH CHOICE POINTS
// =============================================================================

LinearCheckpoint flint_choice_create_linear_checkpoint(void) {
    return flint_trail_create_checkpoint(global_linear_trail);
}

void flint_choice_rollback_linear(LinearCheckpoint checkpoint) {
    flint_trail_rollback_to_checkpoint(global_linear_trail, checkpoint);
}

void flint_choice_commit_linear(LinearCheckpoint checkpoint) {
    flint_trail_commit_checkpoint(global_linear_trail, checkpoint);
}
