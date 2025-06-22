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
    
    // Initialize the linear resource management system
    flint_init_linear_system();
}

void flint_cleanup_runtime(void) {
    // Cleanup linear system first
    flint_cleanup_linear_system();
    
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
    
    // Initialize linear tracking
    flint_mark_linear(value);
    
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
                        Value* length_val = flint_create_integer((int64_t)list_arg->data.list.length);
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
