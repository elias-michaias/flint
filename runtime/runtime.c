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
    next_var_id = 1;
    global_env = flint_create_environment(NULL);
    global_constraints = flint_create_constraint_store();
    allocated_blocks = NULL;
}

void flint_cleanup_runtime(void) {
    // Free all tracked memory blocks
    MemBlock* current = allocated_blocks;
    while (current) {
        MemBlock* next = current->next;
        free(current->ptr);
        free(current);
        current = next;
    }
    allocated_blocks = NULL;
    
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
    
    // Remove from tracking list
    MemBlock** current = &allocated_blocks;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemBlock* to_free = *current;
            *current = (*current)->next;
            free(to_free);
            break;
        }
        current = &(*current)->next;
    }
    
    free(ptr);
}

// =============================================================================
// VALUE MANAGEMENT
// =============================================================================

Value* flint_create_integer(int64_t val) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_INTEGER;
    value->data.integer = val;
    return value;
}

Value* flint_create_float(double val) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_FLOAT;
    value->data.float_val = val;
    return value;
}

Value* flint_create_string(const char* str) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_STRING;
    value->data.string = flint_alloc(strlen(str) + 1);
    strcpy(value->data.string, str);
    return value;
}

Value* flint_create_atom(const char* atom) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_ATOM;
    value->data.atom = flint_alloc(strlen(atom) + 1);
    strcpy(value->data.atom, atom);
    return value;
}

Value* flint_create_list(Value** elements, size_t count) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_LIST;
    value->data.list.length = count;
    value->data.list.capacity = count;
    
    if (count > 0) {
        value->data.list.elements = flint_alloc(sizeof(Value) * count);
        for (size_t i = 0; i < count; i++) {
            value->data.list.elements[i] = *elements[i];
        }
    } else {
        value->data.list.elements = NULL;
    }
    
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
    
    return value;
}

Value* flint_create_logical_var(bool is_linear) {
    Value* value = flint_alloc(sizeof(Value));
    value->type = VAL_LOGICAL_VAR;
    
    LogicalVar* var = flint_alloc(sizeof(LogicalVar));
    var->id = flint_fresh_var_id();
    var->binding = NULL;  // Uninstantiated
    var->waiters = NULL;
    var->is_linear = is_linear;
    var->ref_count = is_linear ? 1 : 0;
    
    value->data.logical_var = var;
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

// =============================================================================
// FUNCTION VALUE MANAGEMENT
// =============================================================================

Value* flint_create_function(const char* name, int arity, void* impl) {
    Value* val = (Value*)flint_alloc(sizeof(Value));
    val->type = VAL_FUNCTION;
    val->data.function.name = strdup(name);
    val->data.function.arity = arity;
    val->data.function.partial_args = NULL;
    val->data.function.applied_count = 0;
    val->data.function.impl = impl;
    return val;
}

Value* flint_create_partial_app(Value* func, Value** args, int applied_count) {
    if (func->type != VAL_FUNCTION) return NULL;
    
    Value* val = (Value*)flint_alloc(sizeof(Value));
    val->type = VAL_PARTIAL_APP;
    val->data.function.name = strdup(func->data.function.name);
    val->data.function.arity = func->data.function.arity;
    val->data.function.impl = func->data.function.impl;
    val->data.function.applied_count = applied_count;
    
    // Copy partial arguments
    if (applied_count > 0) {
        val->data.function.partial_args = (Value**)flint_alloc(sizeof(Value*) * applied_count);
        for (int i = 0; i < applied_count; i++) {
            val->data.function.partial_args[i] = flint_copy_value(args[i]);
        }
    } else {
        val->data.function.partial_args = NULL;
    }
    
    return val;
}

bool flint_is_fully_applied(Value* func) {
    if (func->type != VAL_FUNCTION && func->type != VAL_PARTIAL_APP) return false;
    return func->data.function.applied_count >= func->data.function.arity;
}

// =============================================================================
// HIGHER-ORDER FUNCTION APPLICATION
// =============================================================================

Value* flint_apply_function(Value* func, Value** args, size_t arg_count, Environment* env) {
    if (func->type != VAL_FUNCTION && func->type != VAL_PARTIAL_APP) {
        return NULL;
    }
    
    int total_args = func->data.function.applied_count + arg_count;
    
    // If we have enough arguments, evaluate the function
    if (total_args >= func->data.function.arity) {
        // Combine existing partial args with new args
        Value** all_args = (Value**)flint_alloc(sizeof(Value*) * func->data.function.arity);
        
        // Copy partial arguments
        for (int i = 0; i < func->data.function.applied_count; i++) {
            all_args[i] = func->data.function.partial_args[i];
        }
        
        // Copy new arguments
        for (size_t i = 0; i < (size_t)(func->data.function.arity - func->data.function.applied_count); i++) {
            all_args[func->data.function.applied_count + i] = args[i];
        }
        
        // Call the function via narrowing
        Value* result = flint_narrow_call(func->data.function.name, all_args, func->data.function.arity, env);
        flint_free(all_args);
        return result;
    } else {
        // Create partial application
        Value** combined_args = (Value**)flint_alloc(sizeof(Value*) * total_args);
        
        // Copy existing partial args
        for (int i = 0; i < func->data.function.applied_count; i++) {
            combined_args[i] = func->data.function.partial_args[i];
        }
        
        // Copy new args
        for (size_t i = 0; i < arg_count; i++) {
            combined_args[func->data.function.applied_count + i] = flint_copy_value(args[i]);
        }
        
        Value* partial = flint_create_partial_app(func, combined_args, total_args);
        flint_free(combined_args);
        return partial;
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
            printf("[");
            for (size_t i = 0; i < val->data.list.length; i++) {
                if (i > 0) printf(", ");
                flint_print_value(&val->data.list.elements[i]);
            }
            printf("]");
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
        case VAL_FUNCTION:
            printf("function<%s/%d>", val->data.function.name, val->data.function.arity);
            break;
        case VAL_PARTIAL_APP:
            printf("partial<%s/%d applied:%d>", val->data.function.name, 
                   val->data.function.arity, val->data.function.applied_count);
            break;
        default:
            printf("unknown_value");
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
            for (size_t i = 0; i < val->data.list.length; i++) {
                if (!flint_is_ground(&val->data.list.elements[i])) {
                    return false;
                }
            }
            return true;
            
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

Value* flint_copy_value(Value* val) {
    if (!val) return NULL;
    
    Value* result = flint_alloc(sizeof(Value));
    result->type = val->type;
    
    switch (val->type) {
        case VAL_INTEGER:
            result->data.integer = val->data.integer;
            break;
        case VAL_FLOAT:
            result->data.float_val = val->data.float_val;
            break;
        case VAL_STRING:
            result->data.string = flint_alloc(strlen(val->data.string) + 1);
            strcpy(result->data.string, val->data.string);
            break;
        case VAL_ATOM:
            result->data.atom = flint_alloc(strlen(val->data.atom) + 1);
            strcpy(result->data.atom, val->data.atom);
            break;
        case VAL_LIST:
            result->data.list.length = val->data.list.length;
            result->data.list.capacity = val->data.list.capacity;
            result->data.list.elements = flint_alloc(sizeof(Value) * val->data.list.length);
            for (size_t i = 0; i < val->data.list.length; i++) {
                result->data.list.elements[i] = *flint_copy_value(&val->data.list.elements[i]);
            }
            break;
        case VAL_RECORD:
            result->data.record.field_count = val->data.record.field_count;
            result->data.record.field_names = flint_alloc(sizeof(char*) * val->data.record.field_count);
            result->data.record.field_values = flint_alloc(sizeof(Value) * val->data.record.field_count);
            for (size_t i = 0; i < val->data.record.field_count; i++) {
                // Copy field name
                size_t name_len = strlen(val->data.record.field_names[i]);
                result->data.record.field_names[i] = flint_alloc(name_len + 1);
                strcpy(result->data.record.field_names[i], val->data.record.field_names[i]);
                
                // Copy field value
                result->data.record.field_values[i] = *flint_copy_value(&val->data.record.field_values[i]);
            }
            break;
        case VAL_LOGICAL_VAR:
            result->data.logical_var = val->data.logical_var; // Shallow copy for variables
            break;
        case VAL_FUNCTION:
            result->data.function.name = strdup(val->data.function.name);
            result->data.function.arity = val->data.function.arity;
            result->data.function.applied_count = val->data.function.applied_count;
            result->data.function.impl = val->data.function.impl;
            result->data.function.partial_args = NULL;
            if (val->data.function.partial_args && val->data.function.applied_count > 0) {
                result->data.function.partial_args = (Value**)flint_alloc(sizeof(Value*) * val->data.function.applied_count);
                for (int i = 0; i < val->data.function.applied_count; i++) {
                    result->data.function.partial_args[i] = flint_copy_value(val->data.function.partial_args[i]);
                }
            }
            break;
        case VAL_PARTIAL_APP:
            // Same as function
            result->data.function.name = strdup(val->data.function.name);
            result->data.function.arity = val->data.function.arity;
            result->data.function.applied_count = val->data.function.applied_count;
            result->data.function.impl = val->data.function.impl;
            result->data.function.partial_args = NULL;
            if (val->data.function.partial_args && val->data.function.applied_count > 0) {
                result->data.function.partial_args = (Value**)flint_alloc(sizeof(Value*) * val->data.function.applied_count);
                for (int i = 0; i < val->data.function.applied_count; i++) {
                    result->data.function.partial_args[i] = flint_copy_value(val->data.function.partial_args[i]);
                }
            }
            break;
        default:
            break;
    }
    
    return result;
}
