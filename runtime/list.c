#include "runtime.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// LIST CREATION AND BASIC OPERATIONS
// =============================================================================

Value* flint_list_create(Value** elements, size_t count) {
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

Value* flint_list_create_empty(void) {
    return flint_list_create(NULL, 0);
}

Value* flint_list_create_single(Value* element) {
    Value** elements = &element;
    return flint_list_create(elements, 1);
}

size_t flint_list_length(Value* list) {
    if (!list || list->type != VAL_LIST) {
        return 0;
    }
    list = flint_deref(list);
    return list->data.list.length;
}

bool flint_list_is_empty(Value* list) {
    return flint_list_length(list) == 0;
}

// =============================================================================
// LIST ACCESS AND MANIPULATION
// =============================================================================

Value* flint_list_get_element(Value* list, size_t index) {
    if (!list || list->type != VAL_LIST) {
        return NULL;
    }
    
    list = flint_deref(list);
    if (index >= list->data.list.length) {
        return NULL;
    }
    
    return &list->data.list.elements[index];
}

Value* flint_list_get_head(Value* list) {
    return flint_list_get_element(list, 0);
}

Value* flint_list_get_tail(Value* list) {
    if (!list || list->type != VAL_LIST) {
        return NULL;
    }
    
    list = flint_deref(list);
    if (list->data.list.length <= 1) {
        return flint_list_create_empty();
    }
    
    // Create tail list (all elements except the first)
    Value* tail = flint_alloc(sizeof(Value));
    tail->type = VAL_LIST;
    tail->data.list.length = list->data.list.length - 1;
    tail->data.list.capacity = tail->data.list.length;
    
    if (tail->data.list.length > 0) {
        tail->data.list.elements = flint_alloc(sizeof(Value) * tail->data.list.length);
        for (size_t i = 0; i < tail->data.list.length; i++) {
            tail->data.list.elements[i] = list->data.list.elements[i + 1];
        }
    } else {
        tail->data.list.elements = NULL;
    }
    
    flint_mark_linear(tail);
    return tail;
}

Value* flint_list_prepend(Value* element, Value* list) {
    if (!list || list->type != VAL_LIST) {
        return NULL;
    }
    
    list = flint_deref(list);
    size_t new_length = list->data.list.length + 1;
    
    Value* new_list = flint_alloc(sizeof(Value));
    new_list->type = VAL_LIST;
    new_list->data.list.length = new_length;
    new_list->data.list.capacity = new_length;
    new_list->data.list.elements = flint_alloc(sizeof(Value) * new_length);
    
    // First element is the prepended element
    new_list->data.list.elements[0] = *element;
    
    // Copy the rest from the original list
    for (size_t i = 0; i < list->data.list.length; i++) {
        new_list->data.list.elements[i + 1] = list->data.list.elements[i];
    }
    
    flint_mark_linear(new_list);
    return new_list;
}

Value* flint_list_append_element(Value* list, Value* element) {
    if (!list || list->type != VAL_LIST) {
        return NULL;
    }
    
    list = flint_deref(list);
    size_t new_length = list->data.list.length + 1;
    
    Value* new_list = flint_alloc(sizeof(Value));
    new_list->type = VAL_LIST;
    new_list->data.list.length = new_length;
    new_list->data.list.capacity = new_length;
    new_list->data.list.elements = flint_alloc(sizeof(Value) * new_length);
    
    // Copy original elements
    for (size_t i = 0; i < list->data.list.length; i++) {
        new_list->data.list.elements[i] = list->data.list.elements[i];
    }
    
    // Add the new element at the end
    new_list->data.list.elements[list->data.list.length] = *element;
    
    flint_mark_linear(new_list);
    return new_list;
}

// =============================================================================
// LIST OPERATIONS (APPEND, REVERSE, etc.)
// =============================================================================

Value* flint_list_append(Value* list1, Value* list2) {
    if (!list1 || !list2 || list1->type != VAL_LIST || list2->type != VAL_LIST) {
        return NULL;
    }
    
    list1 = flint_deref(list1);
    list2 = flint_deref(list2);
    
    // If first list is empty, return second list
    if (list1->data.list.length == 0) {
        return list2;
    }
    
    // If second list is empty, return first list
    if (list2->data.list.length == 0) {
        return list1;
    }
    
    size_t new_length = list1->data.list.length + list2->data.list.length;
    
    Value* result = flint_alloc(sizeof(Value));
    result->type = VAL_LIST;
    result->data.list.length = new_length;
    result->data.list.capacity = new_length;
    result->data.list.elements = flint_alloc(sizeof(Value) * new_length);
    
    // Copy elements from first list
    for (size_t i = 0; i < list1->data.list.length; i++) {
        result->data.list.elements[i] = list1->data.list.elements[i];
    }
    
    // Copy elements from second list
    for (size_t i = 0; i < list2->data.list.length; i++) {
        result->data.list.elements[list1->data.list.length + i] = list2->data.list.elements[i];
    }
    
    flint_mark_linear(result);
    return result;
}

Value* flint_list_reverse(Value* list) {
    if (!list || list->type != VAL_LIST) {
        return NULL;
    }
    
    list = flint_deref(list);
    
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
    
    flint_mark_linear(reversed);
    return reversed;
}

// =============================================================================
// LIST PRINTING AND GROUNDNESS
// =============================================================================

void flint_list_print(Value* list) {
    if (!list || list->type != VAL_LIST) {
        printf("<<invalid_list>>");
        return;
    }
    
    list = flint_deref(list);
    printf("[");
    for (size_t i = 0; i < list->data.list.length; i++) {
        if (i > 0) printf(", ");
        flint_print_value(&list->data.list.elements[i]);
    }
    printf("]");
}

bool flint_list_is_ground(Value* list) {
    if (!list || list->type != VAL_LIST) {
        return false;
    }
    
    list = flint_deref(list);
    for (size_t i = 0; i < list->data.list.length; i++) {
        if (!flint_is_ground(&list->data.list.elements[i])) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// LINEAR LIST OPERATIONS
// =============================================================================

Value* flint_list_linear_access(Value* list, size_t index) {
    // This is a non-consumptive operation by default
    // If you want to consume the list, use flint_list_linear_destructure
    
    if (!list || list->type != VAL_LIST || index >= list->data.list.length) {
        return NULL;
    }
    
    list = flint_deref(list);
    
    // Return a copy of the element to maintain linearity
    return flint_copy_for_sharing(&list->data.list.elements[index]);
}

LinearListDestructure flint_list_linear_destructure(Value* list) {
    LinearListDestructure result = {0};
    
    if (!list || list->type != VAL_LIST) {
        return result;
    }
    
    list = flint_deref(list);
    
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

Value* flint_list_deep_copy(Value* list) {
    if (!list || list->type != VAL_LIST) {
        return NULL;
    }
    
    list = flint_deref(list);
    
    Value* copy = flint_alloc(sizeof(Value));
    copy->type = VAL_LIST;
    copy->data.list.length = list->data.list.length;
    copy->data.list.capacity = list->data.list.capacity;
    
    if (list->data.list.length > 0) {
        copy->data.list.elements = flint_alloc(sizeof(Value) * list->data.list.length);
        for (size_t i = 0; i < list->data.list.length; i++) {
            copy->data.list.elements[i] = *flint_deep_copy_value(&list->data.list.elements[i]);
        }
    } else {
        copy->data.list.elements = NULL;
    }
    
    flint_mark_linear(copy);
    return copy;
}

void flint_list_free(Value* list) {
    if (!list || list->type != VAL_LIST) {
        return;
    }
    
    if (list->data.list.elements) {
        flint_free(list->data.list.elements);
    }
}

// =============================================================================
// LIST PATTERN MATCHING
// =============================================================================

bool flint_list_match_pattern(Value* list_val, Pattern* pattern, Environment* env) {
    if (!list_val || list_val->type != VAL_LIST || !pattern) {
        return false;
    }
    
    list_val = flint_deref(list_val);
    
    if (pattern->data.list_pattern.has_tail) {
        // Pattern like [H|T] or [A, B|T]
        if (list_val->data.list.length < pattern->data.list_pattern.count) {
            return false; // Not enough elements
        }
        
        // Match the head elements
        for (size_t i = 0; i < pattern->data.list_pattern.count; i++) {
            if (!flint_pattern_match(&list_val->data.list.elements[i], 
                                   &pattern->data.list_pattern.elements[i], env)) {
                return false;
            }
        }
        
        // Create the tail list and match it
        size_t tail_length = list_val->data.list.length - pattern->data.list_pattern.count;
        Value** tail_elements = NULL;
        
        if (tail_length > 0) {
            tail_elements = flint_alloc(sizeof(Value*) * tail_length);
            for (size_t i = 0; i < tail_length; i++) {
                tail_elements[i] = &list_val->data.list.elements[pattern->data.list_pattern.count + i];
            }
        }
        
        Value* tail_list = flint_list_create(tail_elements, tail_length);
        bool tail_match = flint_pattern_match(tail_list, pattern->data.list_pattern.tail, env);
        
        if (tail_elements) {
            flint_free(tail_elements);
        }
        
        return tail_match;
    } else {
        // Fixed-length pattern like [A, B, C]
        if (list_val->data.list.length != pattern->data.list_pattern.count) {
            return false;
        }
        
        for (size_t i = 0; i < pattern->data.list_pattern.count; i++) {
            if (!flint_pattern_match(&list_val->data.list.elements[i], 
                                   &pattern->data.list_pattern.elements[i], env)) {
                return false;
            }
        }
        
        return true;
    }
}

// =============================================================================
// LIST UNIFICATION
// =============================================================================

bool flint_list_unify(Value* val1, Value* val2, Environment* env) {
    if (!val1 || !val2 || val1->type != VAL_LIST || val2->type != VAL_LIST) {
        return false;
    }
    
    val1 = flint_deref(val1);
    val2 = flint_deref(val2);
    
    if (val1->data.list.length != val2->data.list.length) {
        return false;
    }
    
    for (size_t i = 0; i < val1->data.list.length; i++) {
        if (!flint_unify(&val1->data.list.elements[i], &val2->data.list.elements[i], env)) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// NARROWING OPERATIONS (CONSTRAINT-BASED LIST OPERATIONS)
// =============================================================================

Value* flint_list_narrow_append(Value** args, size_t arg_count, Environment* env) {
    // append(list1, list2, result)
    if (arg_count != 3) return NULL;
    
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
        Value* head = flint_list_get_head(list1);
        Value* tail = flint_list_get_tail(list1);
        
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
            Value* tail_append_result = flint_list_narrow_append(recursive_args, 3, env);
            
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
        } else {
            // Result is ground, try direct append
            Value* appended = flint_list_append(list1, list2);
            if (appended && flint_unify(result, appended, env)) {
                return result;
            }
        }
    }
    
    return NULL;
}

Value* flint_list_narrow_reverse(Value** args, size_t arg_count, Environment* env) {
    // reverse(list, result)
    if (arg_count != 2) return NULL;
    
    Value* list = flint_deref(args[0]);
    Value* result = flint_deref(args[1]);
    
    if (list->type != VAL_LIST) {
        return NULL;
    }
    
    Value* reversed = flint_list_reverse(list);
    
    if (flint_unify(result, reversed, env)) {
        return result;
    }
    
    return NULL;
}

Value* flint_list_narrow_length(Value** args, size_t arg_count, Environment* env) {
    // length(list, result)
    if (arg_count != 2) return NULL;
    
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
