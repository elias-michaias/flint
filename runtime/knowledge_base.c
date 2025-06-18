#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "knowledge_base.h"
#include "unification.h"
#include "memory.h"
#include "symbol_table.h"

// TODO: Extract knowledge base functions from runtime.c
// This is a placeholder - implementations will be moved from runtime.c

linear_kb_t* create_linear_kb(symbol_table_t* symbols) {
    linear_kb_t* kb = malloc(sizeof(linear_kb_t));
    kb->symbols = symbols;
    kb->resources = NULL;
    kb->rules = malloc(sizeof(clause_t) * MAX_CLAUSES);
    kb->rule_count = 0;
    kb->resource_count = 0;
    kb->type_mappings = NULL;
    kb->union_mappings = NULL;
    kb->persistent_facts = NULL;
    kb->auto_deallocate = 0; // Initialize auto_deallocate flag
    kb->total_memory_allocated = 0;
    kb->peak_memory_usage = 0;
    kb->checkpoint_count = 0;
    kb->checkpoints = NULL;
    kb->consumption_metadata = NULL; // Initialize consumption metadata
    return kb;
}

void free_linear_kb(linear_kb_t* kb) {
    if (!kb) return;
    
    // Free resources
    linear_resource_t* resource = kb->resources;
    while (resource) {
        linear_resource_t* next = resource->next;
        if (resource->fact) {
            free_term(resource->fact);
        }
        free(resource);
        resource = next;
    }
    
    // Free rules
    for (int i = 0; i < kb->rule_count; i++) {
        if (kb->rules[i].head) {
            free_term(kb->rules[i].head);
        }
        for (int j = 0; j < kb->rules[i].body_size; j++) {
            if (kb->rules[i].body[j]) {
                free_term(kb->rules[i].body[j]);
            }
        }
        if (kb->rules[i].body) {
            free(kb->rules[i].body);
        }
        if (kb->rules[i].production) {
            free_term(kb->rules[i].production);
        }
    }
    
    // Free type mappings
    type_mapping_t* type_mapping = kb->type_mappings;
    while (type_mapping) {
        type_mapping_t* next = type_mapping->next;
        free(type_mapping->term_name);
        free(type_mapping->type_name);
        free(type_mapping);
        type_mapping = next;
    }
    
    // Free union mappings  
    union_mapping_t* union_mapping = kb->union_mappings;
    while (union_mapping) {
        union_mapping_t* next = union_mapping->next;
        free(union_mapping->variant_type);
        free(union_mapping->parent_type);
        free(union_mapping);
        union_mapping = next;
    }
    
    // Free consumption metadata
    consumption_metadata_t* metadata = kb->consumption_metadata;
    while (metadata) {
        consumption_metadata_t* next = metadata->next;
        free(metadata->resource_name);
        free(metadata->consumption_point);
        free(metadata);
        metadata = next;
    }
    
    free(kb);
}

linear_kb_t* create_kb_copy(linear_kb_t* kb) {
    (void)kb;
    // TODO: Move implementation from runtime.c
    return NULL;
}

void add_linear_fact(linear_kb_t* kb, term_t* fact) {
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = copy_term(fact);
    resource->flags = 0;  // Clear all flags (consumed=0, deallocated=0, persistent=0)
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = symbol_table_intern(kb->symbols, "fact");
    resource->next = kb->resources;
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: MEMORY ALLOCATED - Added resource: ");
    print_term(fact, kb->symbols);
    printf(" (allocated %hu bytes, total resources: %d)\n", resource->memory_size, kb->resource_count);
    #endif
}

void add_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production) {
    if (kb->rule_count < MAX_CLAUSES) {
        kb->rules[kb->rule_count].head = copy_term(head);
        kb->rules[kb->rule_count].body_size = body_size;
        if (body_size > 0) {
            kb->rules[kb->rule_count].body = malloc(sizeof(term_t*) * body_size);
            for (int i = 0; i < body_size; i++) {
                kb->rules[kb->rule_count].body[i] = copy_term(body[i]);
            }
        } else {
            kb->rules[kb->rule_count].body = NULL;
        }
        kb->rules[kb->rule_count].production = production ? copy_term(production) : NULL;
        kb->rule_count++;
    }
}

void add_recursive_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production) {
    (void)kb;
    (void)head;
    (void)body;
    (void)body_size;
    (void)production;
    // TODO: Move implementation from runtime.c
}

void add_persistent_fact(linear_kb_t* kb, term_t* fact) {
    if (!kb || !fact) return;
    
    // Create a linear resource for this persistent fact
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = copy_term(fact);
    resource->flags = 0;
    SET_PERSISTENT(resource, 3); // Persistent level 3 (highest)
    resource->memory_size = sizeof(term_t); // Approximate size
    resource->allocation_site = intern_symbol(kb->symbols, "persistent_fact");
    resource->next = kb->resources;
    
    // Add to the front of the resource list
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: Added persistent fact: ");
    print_term(fact, kb->symbols);
    printf(" (resource count now: %d)\n", kb->resource_count);
    #endif
}

void add_type_mapping(linear_kb_t* kb, const char* term_name, const char* type_name) {
    (void)kb;
    (void)term_name;
    (void)type_name;
    // TODO: Move implementation from runtime.c
}

void add_union_mapping(linear_kb_t* kb, const char* variant_type, const char* parent_type) {
    (void)kb;
    (void)variant_type;
    (void)parent_type;
    // TODO: Move implementation from runtime.c
}

int is_variant_of(linear_kb_t* kb, const char* variant_type, const char* parent_type) {
    (void)kb;
    (void)variant_type;
    (void)parent_type;
    // TODO: Move implementation from runtime.c
    return 0;
}

const char* get_term_type(linear_kb_t* kb, const char* term_name) {
    (void)kb;
    (void)term_name;
    // TODO: Move implementation from runtime.c
    return NULL;
}

int can_unify_with_type(linear_kb_t* kb, term_t* goal, term_t* fact) {
    (void)kb;
    (void)goal;
    (void)fact;
    // TODO: Move implementation from runtime.c
    return 0;
}

void init_goal_stack(goal_stack_t* stack) {
    (void)stack;
    // TODO: Move implementation from runtime.c
}

int push_goal(goal_stack_t* stack, term_t* goal) {
    (void)stack;
    (void)goal;
    // TODO: Move implementation from runtime.c
    return 0;
}

void pop_goal(goal_stack_t* stack) {
    (void)stack;
    // TODO: Move implementation from runtime.c
}

int is_goal_in_stack(goal_stack_t* stack, term_t* goal) {
    (void)stack;
    (void)goal;
    // TODO: Move implementation from runtime.c
    return 0;
}

int is_goal_pattern_in_stack(goal_stack_t* stack, term_t* goal) {
    (void)stack;
    (void)goal;
    // TODO: Move implementation from runtime.c
    return 0;
}

int goals_have_same_pattern(term_t* goal1, term_t* goal2) {
    (void)goal1;
    (void)goal2;
    // TODO: Move implementation from runtime.c
    return 0;
}

void init_goal_cache(goal_cache_t* cache) {
    (void)cache;
    // TODO: Move implementation from runtime.c
}

int check_goal_cache(goal_cache_t* cache, term_t* goal) {
    (void)cache;
    (void)goal;
    // TODO: Move implementation from runtime.c
    return 0;
}

void add_goal_cache(goal_cache_t* cache, term_t* goal, int result) {
    (void)cache;
    (void)goal;
    (void)result;
    // TODO: Move implementation from runtime.c
}

void reset_consumed_resources(linear_kb_t* kb) {
    (void)kb;
    // TODO: Move implementation from runtime.c
}

consumed_state_t* save_consumed_state(linear_kb_t* kb) {
    (void)kb;
    // TODO: Move implementation from runtime.c
    return NULL;
}

void restore_consumed_state(consumed_state_t* state) {
    (void)state;
    // TODO: Move implementation from runtime.c
}

void set_auto_deallocation(linear_kb_t* kb, int enabled) {
    kb->auto_deallocate = enabled;
    #ifdef DEBUG
    printf("DEBUG: Auto deallocation %s\n", enabled ? "enabled" : "disabled");
    #endif
}

// Mark a fact as optional (won't cause errors if not consumed)
void add_optional_linear_fact(linear_kb_t* kb, term_t* fact) {
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = fact;
    resource->flags = 0;  // Clear all flags (persistent=0, consumed=0, deallocated=0)
    SET_PERSISTENT(resource, 0);  // Linear but optional
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = symbol_table_intern(kb->symbols, "optional_fact");
    resource->next = kb->resources;
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: Added optional linear fact: ");
    print_term(fact, kb->symbols);
    printf(" (estimated size: %hu bytes)\n", resource->memory_size);
    #endif
}

// Enhanced fact addition with exponential support
void add_exponential_linear_fact(linear_kb_t* kb, term_t* fact) {
    // Exponential facts are essentially persistent but with different semantics
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = fact;
    resource->flags = 0;  // Clear all flags
    SET_PERSISTENT(resource, 2);  // 2 = exponential (can be used multiple times)
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = symbol_table_intern(kb->symbols, "exponential_fact");
    resource->next = kb->resources;
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: Added exponential linear fact: ");
    print_term(fact, kb->symbols);
    printf(" (estimated size: %hu bytes)\n", resource->memory_size);
    #endif
}

// Enhanced resource consumption with automatic deallocation
int consume_linear_resource_enhanced(linear_kb_t* kb, term_t* goal, substitution_t* subst) {
    linear_resource_t* current = kb->resources;
    
    while (current != NULL) {
        if (!IS_CONSUMED(current)) {
            // Try to unify with the goal
            substitution_t temp_subst = {0};
            init_substitution(&temp_subst);
            copy_substitution(&temp_subst, subst);
            
            if (unify_terms(current->fact, goal, &temp_subst)) {
                // Resource matches - for persistent resources, don't mark as consumed
                if (GET_PERSISTENT(current) == 0) {
                    // Linear resource: consume it  
                    SET_CONSUMED(current);
                    #ifdef DEBUG
                    printf("DEBUG: Consumed linear resource: ");
                    print_term(current->fact, kb->symbols);
                    printf("\n");
                    #endif
                } else {
                    // Persistent resource: use but don't consume
                    #ifdef DEBUG
                    printf("DEBUG: Used persistent resource (not consumed): ");
                    print_term(current->fact, kb->symbols);
                    printf("\n");
                    #endif
                }
                
                copy_substitution(subst, &temp_subst);
                
                // Auto-deallocate if enabled and not persistent/exponential
                if (GET_PERSISTENT(current) == 0) {  // Only auto-deallocate truly linear resources
                    auto_deallocate_resource(kb, current);
                }
                
                free_substitution(&temp_subst);
                return 1;  // Successfully consumed
            }
            
            free_substitution(&temp_subst);
        }
        current = current->next;
    }
    
    return 0;  // No matching resource found
}
