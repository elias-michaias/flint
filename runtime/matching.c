#include "runtime.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// PATTERN CREATION AND MANAGEMENT
// =============================================================================

Pattern* flint_create_pattern(ValueType type) {
    Pattern* pattern = (Pattern*)flint_alloc(sizeof(Pattern));
    pattern->type = type;
    return pattern;
}

void flint_free_pattern(Pattern* pattern) {
    if (!pattern) return;
    
    switch (pattern->type) {
        case VAL_LIST:
            if (pattern->data.list_pattern.elements) {
                for (size_t i = 0; i < pattern->data.list_pattern.count; i++) {
                    flint_free_pattern(&pattern->data.list_pattern.elements[i]);
                }
                flint_free(pattern->data.list_pattern.elements);
            }
            if (pattern->data.list_pattern.tail) {
                flint_free_pattern(pattern->data.list_pattern.tail);
                flint_free(pattern->data.list_pattern.tail);
            }
            break;
        case VAL_RECORD:
            if (pattern->data.record_pattern.field_names) {
                for (size_t i = 0; i < pattern->data.record_pattern.field_count; i++) {
                    flint_free(pattern->data.record_pattern.field_names[i]);
                    flint_free_pattern(&pattern->data.record_pattern.field_patterns[i]);
                }
                flint_free(pattern->data.record_pattern.field_names);
                flint_free(pattern->data.record_pattern.field_patterns);
            }
            break;
        case VAL_ATOM:
            if (pattern->data.atom) flint_free(pattern->data.atom);
            break;
        default:
            break;
    }
    flint_free(pattern);
}

// =============================================================================
// PATTERN MATCHING ENGINE
// =============================================================================

bool flint_pattern_match(Value* val, Pattern* pattern, Environment* env) {
    if (!val || !pattern) return false;
    
    // Dereference the value first
    val = flint_deref(val);
    
    switch (pattern->type) {
        case VAL_INTEGER:
            if (val->type != VAL_INTEGER) return false;
            return val->data.integer == pattern->data.integer;
            
        case VAL_ATOM:
            if (val->type != VAL_ATOM) return false;
            return strcmp(val->data.atom, pattern->data.atom) == 0;
            
        case VAL_LOGICAL_VAR:
            // Pattern variable - bind it to the value
            {
                // First check if this variable already exists in the environment
                LogicalVar* var = flint_lookup_variable(env, pattern->data.variable);
                if (!var) {
                    // Create new variable and bind it to the value
                    flint_bind_variable(env, pattern->data.variable, val);
                    return true;
                } else {
                    // Variable already exists, unify with existing binding
                    if (var->binding) {
                        return flint_unify(val, var->binding, env);
                    } else {
                        // Bind the unbound variable
                        var->binding = val;
                        return true;
                    }
                }
            }
            
        case VAL_LIST:
            if (val->type != VAL_LIST) return false;
            return flint_list_match_pattern(val, pattern, env);
            
        case VAL_RECORD:
            if (val->type != VAL_RECORD) return false;
            return flint_match_record_pattern(val, pattern, env);
            
        default:
            return false;
    }
}

bool flint_match_record_pattern(Value* record_val, Pattern* pattern, Environment* env) {
    // Check that all pattern fields exist in the record
    for (size_t i = 0; i < pattern->data.record_pattern.field_count; i++) {
        char* field_name = pattern->data.record_pattern.field_names[i];
        bool found = false;
        
        for (size_t j = 0; j < record_val->data.record.field_count; j++) {
            if (strcmp(record_val->data.record.field_names[j], field_name) == 0) {
                // Found the field, match the pattern
                if (!flint_pattern_match(&record_val->data.record.field_values[j],
                                       &pattern->data.record_pattern.field_patterns[i], env)) {
                    return false;
                }
                found = true;
                break;
            }
        }
        
        if (!found) return false; // Required field not found
    }
    
    return true;
}

// =============================================================================
// NON-DETERMINISTIC CHOICE
// =============================================================================

Value* flint_create_choice(Value** alternatives, size_t count, Environment* env) {
    if (count == 0) return NULL;
    if (count == 1) return alternatives[0];
    
    // Create a choice point
    ChoicePoint* choice = flint_create_choice_point(env, alternatives, count);
    
    // Return the first alternative (backtracking will explore others)
    return alternatives[0];
}

Value** flint_get_all_solutions(Value* expr, Environment* env, size_t* solution_count) {
    // This is a simplified implementation
    // In a full implementation, this would explore all choice points
    
    *solution_count = 1;
    Value** solutions = (Value**)flint_alloc(sizeof(Value*));
    solutions[0] = expr;
    return solutions;
}
