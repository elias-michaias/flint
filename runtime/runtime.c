#include "runtime.h"
#include <string.h>
#include <stdio.h>

// #define DEBUG 1

// Linear path tracking functions
linear_path_t* create_linear_path() {
    linear_path_t* path = malloc(sizeof(linear_path_t));
    path->steps = NULL;
    path->last_step = NULL;
    return path;
}

void add_path_consume(linear_path_t* path, const char* resource_name) {
    path_step_t* step = malloc(sizeof(path_step_t));
    step->type = PATH_CONSUME;
    step->item_name = malloc(strlen(resource_name) + 1);
    strcpy(step->item_name, resource_name);
    step->produced_name = NULL;
    step->next = NULL;
    
    if (path->last_step) {
        path->last_step->next = step;
    } else {
        path->steps = step;
    }
    path->last_step = step;
}

void add_path_rule_apply(linear_path_t* path, const char* rule_name) {
    path_step_t* step = malloc(sizeof(path_step_t));
    step->type = PATH_RULE_APPLY;
    step->item_name = malloc(strlen(rule_name) + 1);
    strcpy(step->item_name, rule_name);
    step->produced_name = NULL;
    step->next = NULL;
    
    if (path->last_step) {
        path->last_step->next = step;
    } else {
        path->steps = step;
    }
    path->last_step = step;
}

void add_path_produce(linear_path_t* path, const char* rule_name, const char* produced_name) {
    path_step_t* step = malloc(sizeof(path_step_t));
    step->type = PATH_PRODUCE;
    step->item_name = malloc(strlen(rule_name) + 1);
    strcpy(step->item_name, rule_name);
    step->produced_name = malloc(strlen(produced_name) + 1);
    strcpy(step->produced_name, produced_name);
    step->next = NULL;
    
    if (path->last_step) {
        path->last_step->next = step;
    } else {
        path->steps = step;
    }
    path->last_step = step;
}

void print_linear_path(linear_path_t* path) {
    printf("true.\n");
}

void free_linear_path(linear_path_t* path) {
    path_step_t* step = path->steps;
    while (step) {
        path_step_t* next = step->next;
        free(step->item_name);
        if (step->produced_name) {
            free(step->produced_name);
        }
        free(step);
        step = next;
    }
    free(path);
}

linear_path_t* copy_linear_path(linear_path_t* path) {
    linear_path_t* new_path = create_linear_path();
    path_step_t* step = path->steps;
    
    while (step) {
        switch (step->type) {
            case PATH_CONSUME:
                add_path_consume(new_path, step->item_name);
                break;
            case PATH_RULE_APPLY:
                add_path_rule_apply(new_path, step->item_name);
                break;
            case PATH_PRODUCE:
                add_path_produce(new_path, step->item_name, step->produced_name);
                break;
        }
        step = step->next;
    }
    return new_path;
}

// Linear memory allocation
linear_ptr_t linear_alloc(size_t size) {
    linear_ptr_t lptr;
    lptr.ptr = malloc(size);
    lptr.size = size;
    if (!lptr.ptr) {
        fprintf(stderr, "Linear allocation failed for size %zu\n", size);
        exit(1);
    }
    return lptr;
}

// Linear memory deallocation
void linear_free(linear_ptr_t lptr) {
    if (lptr.ptr) {
        free(lptr.ptr);
    }
}

// Load value from linear pointer
int64_t linear_load(linear_ptr_t lptr) {
    if (!lptr.ptr || lptr.size < sizeof(int64_t)) {
        fprintf(stderr, "Invalid linear pointer access\n");
        exit(1);
    }
    return *(int64_t*)lptr.ptr;
}

// Store value to linear pointer
void linear_store(linear_ptr_t lptr, int64_t value) {
    if (!lptr.ptr || lptr.size < sizeof(int64_t)) {
        fprintf(stderr, "Invalid linear pointer access\n");
        exit(1);
    }
    *(int64_t*)lptr.ptr = value;
}

// Create linear string
linear_string_t linear_string_create(const char* str) {
    linear_string_t lstr;
    lstr.length = strlen(str);
    lstr.data = malloc(lstr.length + 1);
    if (!lstr.data) {
        fprintf(stderr, "String allocation failed\n");
        exit(1);
    }
    strcpy(lstr.data, str);
    return lstr;
}

// Free linear string
void linear_string_free(linear_string_t str) {
    if (str.data) {
        free(str.data);
    }
}

// Concatenate two linear strings (consuming both)
linear_string_t linear_string_concat(linear_string_t a, linear_string_t b) {
    linear_string_t result;
    result.length = a.length + b.length;
    result.data = malloc(result.length + 1);
    if (!result.data) {
        fprintf(stderr, "String concatenation allocation failed\n");
        exit(1);
    }
    
    strcpy(result.data, a.data);
    strcat(result.data, b.data);
    
    // Free the input strings (linear consumption)
    linear_string_free(a);
    linear_string_free(b);
    
    return result;
}

// Logical programming implementation

int string_equal(const char* s1, const char* s2) {
    return strcmp(s1, s2) == 0;
}

term_t* create_atom(const char* name) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_ATOM;
    term->data.atom = malloc(strlen(name) + 1);
    strcpy(term->data.atom, name);
    return term;
}

term_t* create_var(const char* name) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_VAR;
    term->data.var = malloc(strlen(name) + 1);
    strcpy(term->data.var, name);
    return term;
}

term_t* create_integer(int64_t value) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_INTEGER;
    term->data.integer = value;
    return term;
}

term_t* create_compound(const char* functor, term_t** args, int arity) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_COMPOUND;
    term->data.compound.functor = malloc(strlen(functor) + 1);
    strcpy(term->data.compound.functor, functor);
    term->data.compound.arity = arity;
    
    if (arity > 0) {
        term->data.compound.args = malloc(sizeof(term_t*) * arity);
        for (int i = 0; i < arity; i++) {
            term->data.compound.args[i] = args[i];
        }
    } else {
        term->data.compound.args = NULL;
    }
    
    return term;
}

term_t* create_clone(term_t* inner) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_CLONE;
    term->data.cloned = inner;
    return term;
}

void free_term(term_t* term) {
    if (!term) return;
    
    switch (term->type) {
        case TERM_ATOM:
            free(term->data.atom);
            break;
        case TERM_VAR:
            free(term->data.var);
            break;
        case TERM_COMPOUND:
            free(term->data.compound.functor);
            if (term->data.compound.args) {
                for (int i = 0; i < term->data.compound.arity; i++) {
                    free_term(term->data.compound.args[i]);
                }
                free(term->data.compound.args);
            }
            break;
        case TERM_INTEGER:
            // Nothing to free
            break;
        case TERM_CLONE:
            free_term(term->data.cloned);
            break;
    }
    free(term);
}

term_t* copy_term(term_t* term) {
    if (!term) return NULL;
    
    switch (term->type) {
        case TERM_ATOM:
            return create_atom(term->data.atom);
        case TERM_VAR:
            return create_var(term->data.var);
        case TERM_INTEGER:
            return create_integer(term->data.integer);
        case TERM_COMPOUND: {
            term_t** new_args = NULL;
            if (term->data.compound.arity > 0) {
                new_args = malloc(sizeof(term_t*) * term->data.compound.arity);
                for (int i = 0; i < term->data.compound.arity; i++) {
                    new_args[i] = copy_term(term->data.compound.args[i]);
                }
            }
            return create_compound(term->data.compound.functor, new_args, term->data.compound.arity);
        }
        case TERM_CLONE:
            return create_clone(copy_term(term->data.cloned));
    }
    return NULL;
}

term_t* apply_substitution(term_t* term, substitution_t* subst) {
    if (!term || !subst) return copy_term(term);
    
    switch (term->type) {
        case TERM_VAR:
            for (int i = 0; i < subst->count; i++) {
                if (string_equal(term->data.var, subst->bindings[i].var)) {
                    return apply_substitution(subst->bindings[i].term, subst);
                }
            }
            return copy_term(term);
            
        case TERM_COMPOUND: {
            term_t** new_args = NULL;
            if (term->data.compound.arity > 0) {
                new_args = malloc(sizeof(term_t*) * term->data.compound.arity);
                for (int i = 0; i < term->data.compound.arity; i++) {
                    new_args[i] = apply_substitution(term->data.compound.args[i], subst);
                }
            }
            return create_compound(term->data.compound.functor, new_args, term->data.compound.arity);
        }
        
        default:
            return copy_term(term);
    }
}

// Compose two substitutions: dest = compose(dest, src)
void compose_substitutions(substitution_t* dest, substitution_t* src) {
    if (!dest || !src) return;
    
    // Apply src to all terms in dest
    for (int i = 0; i < dest->count; i++) {
        term_t* new_term = apply_substitution(dest->bindings[i].term, src);
        free_term(dest->bindings[i].term);
        dest->bindings[i].term = new_term;
    }
    
    // Add bindings from src that are not in dest
    for (int i = 0; i < src->count; i++) {
        int found = 0;
        for (int j = 0; j < dest->count; j++) {
            if (string_equal(src->bindings[i].var, dest->bindings[j].var)) {
                found = 1;
                break;
            }
        }
        if (!found && dest->count < MAX_VARS) {
            dest->bindings[dest->count].var = malloc(strlen(src->bindings[i].var) + 1);
            strcpy(dest->bindings[dest->count].var, src->bindings[i].var);
            dest->bindings[dest->count].term = copy_term(src->bindings[i].term);
            dest->count++;
        }
    }
}

int unify(term_t* t1, term_t* t2, substitution_t* subst) {
    if (!t1 || !t2) return 0;
    
    // Apply current substitution
    term_t* term1 = apply_substitution(t1, subst);
    term_t* term2 = apply_substitution(t2, subst);
    
    int result = 0;
    
    // Variable unification
    if (term1->type == TERM_VAR) {
        if (subst->count < MAX_VARS) {
            subst->bindings[subst->count].var = malloc(strlen(term1->data.var) + 1);
            strcpy(subst->bindings[subst->count].var, term1->data.var);
            subst->bindings[subst->count].term = copy_term(term2);
            subst->count++;
            result = 1;
        }
    } else if (term2->type == TERM_VAR) {
        if (subst->count < MAX_VARS) {
            subst->bindings[subst->count].var = malloc(strlen(term2->data.var) + 1);
            strcpy(subst->bindings[subst->count].var, term2->data.var);
            subst->bindings[subst->count].term = copy_term(term1);
            subst->count++;
            result = 1;
        }
    }
    // Atom unification
    else if (term1->type == TERM_ATOM && term2->type == TERM_ATOM) {
        result = string_equal(term1->data.atom, term2->data.atom);
    }
    // Integer unification
    else if (term1->type == TERM_INTEGER && term2->type == TERM_INTEGER) {
        result = term1->data.integer == term2->data.integer;
    }
    // Compound term unification
    else if (term1->type == TERM_COMPOUND && term2->type == TERM_COMPOUND) {
        if (string_equal(term1->data.compound.functor, term2->data.compound.functor) &&
            term1->data.compound.arity == term2->data.compound.arity) {
            result = 1;
            for (int i = 0; i < term1->data.compound.arity && result; i++) {
                result = unify(term1->data.compound.args[i], term2->data.compound.args[i], subst);
            }
        }
    }
    
    free_term(term1);
    free_term(term2);
    return result;
}

void print_term(term_t* term) {
    if (!term) {
        printf("NULL");
        return;
    }
    
    switch (term->type) {
        case TERM_ATOM:
            printf("%s", term->data.atom);
            break;
        case TERM_VAR:
            printf("%s", term->data.var);
            break;
        case TERM_INTEGER:
            printf("%lld", term->data.integer);
            break;
        case TERM_COMPOUND:
            printf("%s", term->data.compound.functor);
            if (term->data.compound.arity > 0) {
                printf("(");
                for (int i = 0; i < term->data.compound.arity; i++) {
                    if (i > 0) printf(", ");
                    print_term(term->data.compound.args[i]);
                }
                printf(")");
            }
            break;
        case TERM_CLONE:
            printf("!");
            print_term(term->data.cloned);
            break;
    }
}

void print_substitution(substitution_t* subst) {
    if (!subst || subst->count == 0) {
        printf("{}");
        return;
    }
    
    printf("{");
    for (int i = 0; i < subst->count; i++) {
        if (i > 0) printf(", ");
        printf("%s/", subst->bindings[i].var);
        print_term(subst->bindings[i].term);
    }
    printf("}");
}

// Simplified resolution - just try to match the first goal against facts
int resolve_query(clause_t* clauses, int clause_count, term_t** goals, int goal_count) {
    if (goal_count == 0) return 1; // Success
    
    int solutions = 0;
    term_t* goal = goals[0];
    
#ifdef DEBUG
    printf("Trying to resolve: ");
    print_term(goal);
    printf("\n");
#endif
    
    for (int i = 0; i < clause_count; i++) {
        if (clauses[i].body_size == 0) { // Only facts for now
            substitution_t subst = {0};
            
            if (unify(goal, clauses[i].head, &subst)) {
#ifdef DEBUG
                printf("Unified with fact: ");
                print_term(clauses[i].head);
                printf(" with substitution: ");
                print_substitution(&subst);
                printf("\n");
#endif
                solutions++;
                
                // For now, just count solutions
                // In a full implementation, we'd continue with remaining goals
            }
        }
    }
    
    return solutions;
}

// LINEAR LOGIC IMPLEMENTATION

// Create a new linear knowledge base
linear_kb_t* create_linear_kb() {
    linear_kb_t* kb = malloc(sizeof(linear_kb_t));
    kb->resources = NULL;
    kb->rules = malloc(sizeof(clause_t) * MAX_CLAUSES);
    kb->rule_count = 0;
    kb->type_mappings = NULL;
    kb->union_mappings = NULL;
    return kb;
}

// Add a linear fact (resource that can be consumed)
void add_linear_fact(linear_kb_t* kb, term_t* fact) {
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = copy_term(fact);
    resource->consumed = 0;
    resource->next = kb->resources;
    kb->resources = resource;
}

// Add a rule (can be reused)
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

// Add a type mapping for a term
void add_type_mapping(linear_kb_t* kb, const char* term_name, const char* type_name) {
    type_mapping_t* mapping = malloc(sizeof(type_mapping_t));
    mapping->term_name = malloc(strlen(term_name) + 1);
    strcpy(mapping->term_name, term_name);
    mapping->type_name = malloc(strlen(type_name) + 1);
    strcpy(mapping->type_name, type_name);
    mapping->next = kb->type_mappings;
    kb->type_mappings = mapping;
}

// Get the type of a term
const char* get_term_type(linear_kb_t* kb, const char* term_name) {
    type_mapping_t* mapping = kb->type_mappings;
    while (mapping) {
        if (strcmp(mapping->term_name, term_name) == 0) {
            return mapping->type_name;
        }
        mapping = mapping->next;
    }
    return NULL;
}

// Add a union hierarchy mapping (variant -> parent type)
void add_union_mapping(linear_kb_t* kb, const char* variant_type, const char* parent_type) {
    union_mapping_t* mapping = malloc(sizeof(union_mapping_t));
    mapping->variant_type = malloc(strlen(variant_type) + 1);
    strcpy(mapping->variant_type, variant_type);
    mapping->parent_type = malloc(strlen(parent_type) + 1);
    strcpy(mapping->parent_type, parent_type);
    mapping->next = kb->union_mappings;
    kb->union_mappings = mapping;
}

// Check if a variant type can be treated as a parent type (with transitive closure)
int is_variant_of(linear_kb_t* kb, const char* variant_type, const char* parent_type) {
    // Direct match
    if (strcmp(variant_type, parent_type) == 0) {
        return 1;
    }
    
    // Look for direct mapping
    union_mapping_t* mapping = kb->union_mappings;
    while (mapping) {
        if (strcmp(mapping->variant_type, variant_type) == 0) {
            // Found a mapping for variant_type -> intermediate_type
            if (strcmp(mapping->parent_type, parent_type) == 0) {
                return 1; // Direct match
            }
            // Recursively check if intermediate_type is a variant of parent_type
            if (is_variant_of(kb, mapping->parent_type, parent_type)) {
                return 1;
            }
        }
        mapping = mapping->next;
    }
    return 0;
}

// Check if a resource is persistent (cloned)
int is_persistent_resource(term_t* fact) {
    return fact->type == TERM_CLONE;
}

// Get the inner term from a cloned term
term_t* get_inner_term(term_t* term) {
    if (term->type == TERM_CLONE) {
        return term->data.cloned;
    }
    return term;
}

// Check if a goal can unify with a fact based on type matching
int can_unify_with_type(linear_kb_t* kb, term_t* goal, term_t* fact) {
    // Handle cloned facts - unify with the inner term
    term_t* actual_fact = get_inner_term(fact);
    
    // First try direct unification
    substitution_t temp_subst = {0};
    if (unify(goal, actual_fact, &temp_subst)) {
        return 1;
    }
    
    // If direct unification fails, try type-based matching
    if (goal->type == TERM_ATOM && actual_fact->type == TERM_ATOM) {
        // Check if goal is a type name and fact is an instance of that type
        const char* fact_type = get_term_type(kb, actual_fact->data.atom);
        if (fact_type) {
            // Direct type match
            if (strcmp(goal->data.atom, fact_type) == 0) {
                return 1;
            }
            // Union hierarchy match: check if fact_type is a variant of goal type
            if (is_variant_of(kb, fact_type, goal->data.atom)) {
                return 1;
            }
        }
    }
    
    return 0;
}

// Reset all consumed resources (for new queries)
void reset_consumed_resources(linear_kb_t* kb) {
    linear_resource_t* resource = kb->resources;
    while (resource) {
        resource->consumed = 0;
        resource = resource->next;
    }
}

// Save the current state of consumed resources for backtracking
consumed_state_t* save_consumed_state(linear_kb_t* kb) {
    consumed_state_t* state = NULL;
    linear_resource_t* resource = kb->resources;
    
    while (resource) {
        consumed_state_t* entry = malloc(sizeof(consumed_state_t));
        entry->resource = resource;
        entry->was_consumed = resource->consumed;
        entry->next = state;
        state = entry;
        resource = resource->next;
    }
    return state;
}

void restore_consumed_state(consumed_state_t* state) {
    while (state) {
        state->resource->consumed = state->was_consumed;
        consumed_state_t* next = state->next;
        free(state);
        state = next;
    }
}

// Linear resolution - resources are consumed when used
// original_query is the query as typed by the user (with variables)
int linear_resolve_query_with_substitution(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst) {
    if (goal_count == 0) {
        // Success - check if original query has variables
        if (has_variables(original_query)) {
            // Print the result with substitutions applied
            term_t* result = apply_substitution(original_query, global_subst);
            print_term(result);
            printf(".\n");
            free_term(result);
        } else {
            // No variables - just print true
            printf("true.\n");
        }
        return 1;
    }
    
    term_t* goal = goals[0];
    
#ifdef DEBUG
    printf("Linear resolving: ");
    print_term(goal);
    printf("\n");
#endif
    
    int solutions = 0;
    
    // First, try to match against available (unconsumed) linear facts
    linear_resource_t* resource = kb->resources;
    while (resource) {
        if (!resource->consumed) {
            substitution_t subst = {0};
            
            if (can_unify_with_type(kb, goal, resource->fact)) {
                // Save current state for backtracking
                consumed_state_t* saved_state = save_consumed_state(kb);
                
                // Handle cloned vs regular facts
                term_t* actual_fact = get_inner_term(resource->fact);
                int is_persistent = is_persistent_resource(resource->fact);
                
                // For type-based matching, we need to create a proper substitution
                if (goal->type == TERM_ATOM && actual_fact->type == TERM_ATOM) {
                    const char* fact_type = get_term_type(kb, actual_fact->data.atom);
                    if (fact_type && strcmp(goal->data.atom, fact_type) == 0) {
                        // This is a type match - goal "coin" matches fact "c1" of type "coin"
                        // No variable substitution needed for this case
                    } else {
                        // Direct unification 
                        unify(goal, actual_fact, &subst);
                    }
                } else {
                    unify(goal, actual_fact, &subst);
                }
                
#ifdef DEBUG
                printf("Unified with %s fact: ", is_persistent ? "persistent" : "linear");
                print_term(resource->fact);
                if (!is_persistent) printf(" (consuming resource)");
                printf("\n");
#endif
                
                // Mark the resource as consumed (only if it's not persistent)
                if (!is_persistent) {
                    resource->consumed = 1;
                }
                
                // Compose the substitution with the global substitution
                substitution_t composed_subst = *global_subst;
                compose_substitutions(&composed_subst, &subst);
                
                // Apply substitution to remaining goals
                term_t** remaining_goals = NULL;
                int remaining_count = goal_count - 1;
                
                if (remaining_count > 0) {
                    remaining_goals = malloc(sizeof(term_t*) * remaining_count);
                    for (int i = 0; i < remaining_count; i++) {
                        remaining_goals[i] = apply_substitution(goals[i + 1], &subst);
                    }
                }
                
                // Recursively resolve remaining goals
                int success = linear_resolve_query_with_substitution(kb, remaining_goals, remaining_count, original_query, &composed_subst);
                
                if (success) {
                    solutions += success;
                }
                
                // Clean up remaining goals
                if (remaining_goals) {
                    for (int i = 0; i < remaining_count; i++) {
                        free_term(remaining_goals[i]);
                    }
                    free(remaining_goals);
                }
                
                // RESTORE the resource state for backtracking to find more solutions
                restore_consumed_state(saved_state);
            }
        }
        resource = resource->next;
    }
    
    // Second, try to match against rules
    for (int i = 0; i < kb->rule_count; i++) {
        clause_t* rule = &kb->rules[i];
        substitution_t rule_subst = {0};
        
        // Try to unify the goal with the head of the rule
        if (unify(goal, rule->head, &rule_subst)) {
#ifdef DEBUG
            printf("Rule matches: ");
            print_term(rule->head);
            printf(" :- ");
            for (int j = 0; j < rule->body_size; j++) {
                if (j > 0) printf(", ");
                print_term(rule->body[j]);
            }
            printf("\n");
#endif
            
            // Save current state for backtracking
            consumed_state_t* saved_state = save_consumed_state(kb);
            
            // Compose the rule substitution with the global substitution
            substitution_t composed_subst = *global_subst;
            compose_substitutions(&composed_subst, &rule_subst);
            
            // First, resolve just the rule body
            int body_success = 0;
            if (rule->body_size > 0) {
                term_t** body_goals = malloc(sizeof(term_t*) * rule->body_size);
                for (int j = 0; j < rule->body_size; j++) {
                    body_goals[j] = apply_substitution(rule->body[j], &rule_subst);
                }
                body_success = linear_resolve_query_with_substitution(kb, body_goals, rule->body_size, original_query, &composed_subst);
                
                // Clean up body goals
                for (int j = 0; j < rule->body_size; j++) {
                    free_term(body_goals[j]);
                }
                free(body_goals);
            } else {
                // Rule with empty body always succeeds
                body_success = 1;
            }
            
            // If the rule body succeeded and there's a production, add it to the KB
            // But only add one production per rule match, not per body success
            if (body_success > 0 && rule->production) {
                term_t* produced_fact = apply_substitution(rule->production, &rule_subst);
#ifdef DEBUG
                printf("Rule fired, producing fact: ");
                print_term(produced_fact);
                printf("\n");
#endif
                add_linear_fact(kb, produced_fact);
            }
            
            // Now resolve the remaining goals (if the body succeeded)
            int remaining_success = 0;
            if (body_success && goal_count > 1) {
                int remaining_count = goal_count - 1;
                term_t** remaining_goals = malloc(sizeof(term_t*) * remaining_count);
                for (int j = 1; j < goal_count; j++) {
                    remaining_goals[j - 1] = apply_substitution(goals[j], &rule_subst);
                }
                remaining_success = linear_resolve_query_with_substitution(kb, remaining_goals, remaining_count, original_query, &composed_subst);
                
                // Clean up remaining goals
                for (int j = 0; j < remaining_count; j++) {
                    free_term(remaining_goals[j]);
                }
                free(remaining_goals);
            } else if (body_success) {
                // No remaining goals, so success
                remaining_success = 1;
            }
            
            if (body_success && remaining_success) {
                solutions += remaining_success;
            }
            
            // Restore the resource state for backtracking
            restore_consumed_state(saved_state);
        }
    }
    
    return solutions;
}

// Forward declarations for forward chaining helper functions
int build_solution_path(linear_kb_t* kb, linear_resource_t* start_resource, term_t** goals, int goal_count, linear_path_t* path);
int consume_resource_and_apply_rules(linear_kb_t* kb, linear_resource_t* resource, term_t** goals, int goal_count, int* remaining_goals, int* unsatisfied_count, linear_path_t* path);

// Forward chaining linear resolution - follows distinct linear paths
int linear_resolve_forward_chaining(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path) {
    if (goal_count == 0) {
        // All goals satisfied - this is a complete solution
        printf("true.\n");
        return 1;
    }
    
    int solutions = 0;
    
    // For each initial resource, try to build a complete solution path
    linear_resource_t* resource = kb->resources;
    while (resource) {
        if (!resource->consumed) {
            // Save the current state for backtracking
            consumed_state_t* saved_state = save_consumed_state(kb);
            
            // Try to build a solution starting from this resource
            linear_path_t* new_path = copy_linear_path(path);
            
            if (build_solution_path(kb, resource, goals, goal_count, new_path)) {
                solutions++;
                printf("true.\n");
            }
            
            free_linear_path(new_path);
            restore_consumed_state(saved_state);
        }
        resource = resource->next;
    }
    
    return solutions;
}

// Build a complete solution path starting from a specific resource
int build_solution_path(linear_kb_t* kb, linear_resource_t* start_resource, term_t** goals, int goal_count, linear_path_t* path) {
    // Keep track of goals that still need to be satisfied
    int remaining_goals[goal_count];
    for (int i = 0; i < goal_count; i++) {
        remaining_goals[i] = 1; // 1 means goal still needs to be satisfied
    }
    int unsatisfied_count = goal_count;
    
    // Start the forward chaining process with the initial resource
    if (!consume_resource_and_apply_rules(kb, start_resource, goals, goal_count, remaining_goals, &unsatisfied_count, path)) {
        return 0; // Failed to make progress
    }
    
    // Continue applying rules until all goals are satisfied or no more progress can be made
    int made_progress = 1;
    while (unsatisfied_count > 0 && made_progress) {
        made_progress = 0;
        
        // Look for any available resource that can make progress on remaining goals
        linear_resource_t* resource = kb->resources;
        while (resource && unsatisfied_count > 0) {
            if (!resource->consumed) {
                if (consume_resource_and_apply_rules(kb, resource, goals, goal_count, remaining_goals, &unsatisfied_count, path)) {
                    made_progress = 1;
                }
            }
            resource = resource->next;
        }
    }
    
    return (unsatisfied_count == 0); // Success if all goals are satisfied
}

// Try to consume a resource and apply rules to make progress toward goals
int consume_resource_and_apply_rules(linear_kb_t* kb, linear_resource_t* resource, term_t** goals, int goal_count, int* remaining_goals, int* unsatisfied_count, linear_path_t* path) {
    int made_progress = 0;
    
    // First check if this resource directly satisfies any goal
    for (int g = 0; g < goal_count; g++) {
        if (remaining_goals[g]) {
            substitution_t direct_subst = {0};
            if (unify(goals[g], resource->fact, &direct_subst)) {
                // Direct satisfaction
                term_t* actual_fact = get_inner_term(resource->fact);
                int is_persistent = is_persistent_resource(resource->fact);
                
                if (!is_persistent) {
                    resource->consumed = 1;
                }
                
                add_path_consume(path, actual_fact->data.atom);
                remaining_goals[g] = 0;
                (*unsatisfied_count)--;
                made_progress = 1;
                
#ifdef DEBUG
                printf("Forward chaining: directly consumed %s to satisfy goal %s\n", 
                       actual_fact->data.atom, goals[g]->data.atom);
#endif
                return made_progress;
            }
        }
    }
    
    // Try to find a rule that can consume this resource
    for (int rule_idx = 0; rule_idx < kb->rule_count; rule_idx++) {
        clause_t* rule = &kb->rules[rule_idx];
        
        if (rule->body_size == 1) { // Handle single-body rules
            substitution_t rule_subst = {0};
            
            // Check if this rule can consume our resource
            if (can_unify_with_type(kb, rule->body[0], resource->fact)) {
                // Consume the resource
                term_t* actual_fact = get_inner_term(resource->fact);
                int is_persistent = is_persistent_resource(resource->fact);
                
                if (!is_persistent) {
                    resource->consumed = 1;
                }
                
                add_path_consume(path, actual_fact->data.atom);
                add_path_rule_apply(path, rule->head->data.atom);
                
                // Check if the rule head directly satisfies any remaining goal
                for (int g = 0; g < goal_count; g++) {
                    if (remaining_goals[g]) {
                        substitution_t goal_subst = {0};
                        if (unify(goals[g], rule->head, &goal_subst)) {
                            remaining_goals[g] = 0;
                            (*unsatisfied_count)--;
                            made_progress = 1;
                            
#ifdef DEBUG
                            printf("Forward chaining: consumed %s, applied rule %s, satisfied goal %s\n", 
                                   actual_fact->data.atom, rule->head->data.atom, goals[g]->data.atom);
#endif
                        }
                    }
                }
                
                // If rule produces a fact, add it to the knowledge base
                if (rule->production) {
                    term_t* produced_fact = apply_substitution(rule->production, &rule_subst);
                    add_linear_fact(kb, produced_fact);
                    add_path_produce(path, rule->head->data.atom, produced_fact->data.atom);
                    made_progress = 1;
                    
#ifdef DEBUG
                    printf("Forward chaining: consumed %s, applied rule %s, produced %s\n", 
                           actual_fact->data.atom, rule->head->data.atom, produced_fact->data.atom);
#endif
                }
                
                return made_progress;
            }
        }
    }
    
    return made_progress;
}

// Wrapper function for the original interface
int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count) {
    if (goal_count == 0) {
        printf("true.\n");
        return 1;
    }
    
    // Create an empty path for tracking
    linear_path_t* path = create_linear_path();
    
    // Use forward chaining approach
    int result = linear_resolve_forward_chaining(kb, goals, goal_count, path);
    
    free_linear_path(path);
    return result;
}

// Free linear knowledge base
void free_linear_kb(linear_kb_t* kb) {
    if (!kb) return;
    
    // Free resources
    linear_resource_t* resource = kb->resources;
    while (resource) {
        linear_resource_t* next = resource->next;
        free_term(resource->fact);
        free(resource);
        resource = next;
    }
    
    // Free rules
    for (int i = 0; i < kb->rule_count; i++) {
        free_term(kb->rules[i].head);
        if (kb->rules[i].body) {
            for (int j = 0; j < kb->rules[i].body_size; j++) {
                free_term(kb->rules[i].body[j]);
            }
            free(kb->rules[i].body);
        }
    }
    free(kb->rules);
    
    // Free type mappings
    type_mapping_t* mapping = kb->type_mappings;
    while (mapping) {
        type_mapping_t* next = mapping->next;
        free(mapping->term_name);
        free(mapping->type_name);
        free(mapping);
        mapping = next;
    }
    
    // Free union mappings
    union_mapping_t* umapping = kb->union_mappings;
    while (umapping) {
        union_mapping_t* next = umapping->next;
        free(umapping->variant_type);
        free(umapping->parent_type);
        free(umapping);
        umapping = next;
    }
    
    free(kb);
}

// Helper function to check if a term contains variables
int has_variables(term_t* term) {
    if (!term) return 0;
    
    switch (term->type) {
        case TERM_VAR:
            return 1;
        case TERM_ATOM:
        case TERM_INTEGER:
            return 0;
        case TERM_COMPOUND:
            for (int i = 0; i < term->data.compound.arity; i++) {
                if (has_variables(term->data.compound.args[i])) {
                    return 1;
                }
            }
            return 0;
        case TERM_CLONE:
            return has_variables(term->data.cloned);
        default:
            return 0;
    }
}
