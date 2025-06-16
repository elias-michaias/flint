#include "runtime.h"
#include <string.h>
#include <stdio.h>

// #define DEBUG 1

// Forward declarations
int find_matching_resources(linear_kb_t* kb, clause_t* rule, linear_resource_t** resources, substitution_t* subst);
void init_substitution(substitution_t* subst);
void free_substitution(substitution_t* subst);
int linear_resolve_query_with_substitution_enhanced_internal(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                  term_t** original_goals, int original_goal_count, substitution_t* global_subst, 
                                                  enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack);

// Terms equality function - check if two terms are equal
int terms_equal(term_t* t1, term_t* t2) {
    if (t1 == NULL && t2 == NULL) return 1;
    if (t1 == NULL || t2 == NULL) return 0;
    
    if (t1->type != t2->type) return 0;
    
    switch (t1->type) {
        case TERM_ATOM:
            return strcmp(t1->data.atom, t2->data.atom) == 0;
        case TERM_VAR:
            return strcmp(t1->data.var, t2->data.var) == 0;
        case TERM_COMPOUND:
            if (strcmp(t1->data.compound.functor, t2->data.compound.functor) != 0) return 0;
            if (t1->data.compound.arity != t2->data.compound.arity) return 0;
            for (int i = 0; i < t1->data.compound.arity; i++) {
                if (!terms_equal(t1->data.compound.args[i], t2->data.compound.args[i])) {
                    return 0;
                }
            }
            return 1;
        case TERM_INTEGER:
            return t1->data.integer == t2->data.integer;
        case TERM_CLONE:
            return terms_equal(t1->data.cloned, t2->data.cloned);
    }
    return 0;
}

// Linear resolve query
int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count) {
    substitution_t global_subst = {0};
    return linear_resolve_query_with_substitution(kb, goals, goal_count, goals[0], &global_subst);
}

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
    (void)path; // Suppress unused parameter warning
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
    
    // Handle cloned terms by using their inner term
    if (term1->type == TERM_CLONE) {
        result = unify(term1->data.cloned, term2, subst);
    } else if (term2->type == TERM_CLONE) {
        result = unify(term1, term2->data.cloned, subst);
    }
    // Variable unification
    else if (term1->type == TERM_VAR) {
        // Occurs check: don't bind a variable to a term containing itself
        if (!occurs_in_term(term1->data.var, term2)) {
            if (subst->count < MAX_VARS) {
                subst->bindings[subst->count].var = malloc(strlen(term1->data.var) + 1);
                strcpy(subst->bindings[subst->count].var, term1->data.var);
                subst->bindings[subst->count].term = copy_term(term2);
                subst->count++;
                result = 1;
            }
        }
    } else if (term2->type == TERM_VAR) {
        // Occurs check: don't bind a variable to a term containing itself
        if (!occurs_in_term(term2->data.var, term1)) {
            if (subst->count < MAX_VARS) {
                subst->bindings[subst->count].var = malloc(strlen(term2->data.var) + 1);
                strcpy(subst->bindings[subst->count].var, term2->data.var);
                subst->bindings[subst->count].term = copy_term(term1);
                subst->count++;
                result = 1;
            }
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

// Check if a variable occurs in a term (occurs check for unification)
int occurs_in_term(const char* var, term_t* term) {
    if (!term || !var) return 0;
    
    switch (term->type) {
        case TERM_VAR:
            return string_equal(var, term->data.var);
            
        case TERM_COMPOUND:
            for (int i = 0; i < term->data.compound.arity; i++) {
                if (occurs_in_term(var, term->data.compound.args[i])) {
                    return 1;
                }
            }
            return 0;
            
        case TERM_CLONE:
            return occurs_in_term(var, term->data.cloned);
            
        case TERM_ATOM:
        case TERM_INTEGER:
            return 0;
    }
    return 0;
}

// Rename variables in a term to avoid conflicts (for rule instances)
term_t* rename_variables_in_term(term_t* term, int instance_id) {
    if (!term) return NULL;
    
    switch (term->type) {
        case TERM_VAR: {
            // Create a new variable name by appending the instance ID
            int new_name_len = strlen(term->data.var) + 20; // Extra space for _inst_N
            char* new_name = malloc(new_name_len);
            snprintf(new_name, new_name_len, "%s_inst_%d", term->data.var, instance_id);
            term_t* renamed = create_var(new_name);
            free(new_name);
            return renamed;
        }
        
        case TERM_COMPOUND: {
            term_t** new_args = NULL;
            if (term->data.compound.arity > 0) {
                new_args = malloc(sizeof(term_t*) * term->data.compound.arity);
                for (int i = 0; i < term->data.compound.arity; i++) {
                    new_args[i] = rename_variables_in_term(term->data.compound.args[i], instance_id);
                }
            }
            return create_compound(term->data.compound.functor, new_args, term->data.compound.arity);
        }
        
        case TERM_CLONE:
            return create_clone(rename_variables_in_term(term->data.cloned, instance_id));
            
        case TERM_ATOM:
        case TERM_INTEGER:
            return copy_term(term);
    }
    return NULL;
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
    kb->persistent_facts = NULL;
    return kb;
}

// Add a linear fact (resource that can be consumed)
void add_linear_fact(linear_kb_t* kb, term_t* fact) {
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = copy_term(fact);
    resource->consumed = 0;
    resource->persistent = 0; // Mark as linear (consumable)
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
        kb->rules[kb->rule_count].is_recursive = 0;  // Default to non-recursive
        kb->rule_count++;
    }
}

// Add a recursive rule (can be reused, marked as recursive)
void add_recursive_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production) {
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
        kb->rules[kb->rule_count].is_recursive = 1;  // Mark as recursive
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

// Goal stack functions for recursion detection
void init_goal_stack(goal_stack_t* stack) {
    stack->depth = 0;
}

int push_goal(goal_stack_t* stack, term_t* goal) {
    if (stack->depth >= MAX_GOAL_STACK_DEPTH) {
        return 0; // Stack overflow
    }
    stack->goals[stack->depth] = copy_term(goal);
    stack->depth++;
    return 1;
}

void pop_goal(goal_stack_t* stack) {
    if (stack->depth > 0) {
        stack->depth--;
        free_term(stack->goals[stack->depth]);
    }
}

int is_goal_in_stack(goal_stack_t* stack, term_t* goal) {
    for (int i = 0; i < stack->depth; i++) {
        if (terms_equal(stack->goals[i], goal)) {
            return 1;
        }
    }
    return 0;
}

// Check if a goal is recursively related to any goal in the stack
// This checks for same functor/arity pattern to catch recursive calls with different variables
int is_goal_pattern_in_stack(goal_stack_t* stack, term_t* goal) {
    for (int i = 0; i < stack->depth; i++) {
        if (goals_have_same_pattern(stack->goals[i], goal)) {
            return 1;
        }
    }
    return 0;
}

// Check if two goals have the same pattern for recursion detection
// For compound terms: same functor, same arity, and same ground arguments in key positions
int goals_have_same_pattern(term_t* goal1, term_t* goal2) {
    if (goal1->type != goal2->type) return 0;
    
    if (goal1->type == TERM_COMPOUND && goal2->type == TERM_COMPOUND) {
        // Must have same functor and arity
        if (strcmp(goal1->data.compound.functor, goal2->data.compound.functor) != 0) return 0;
        if (goal1->data.compound.arity != goal2->data.compound.arity) return 0;
        
        // For recursion detection, check if the first ground argument is the same
        // This handles cases like ancestor(alice, $y) vs ancestor(alice, $z)
        if (goal1->data.compound.arity > 0) {
            term_t* arg1_1 = goal1->data.compound.args[0];
            term_t* arg1_2 = goal2->data.compound.args[0];
            
            // If both first arguments are ground (atoms), they must be equal
            if (arg1_1->type == TERM_ATOM && arg1_2->type == TERM_ATOM) {
                return strcmp(arg1_1->data.atom, arg1_2->data.atom) == 0;
            }
            
            // If one is ground and one is variable, consider them different patterns
            if ((arg1_1->type == TERM_ATOM && arg1_2->type == TERM_VAR) ||
                (arg1_1->type == TERM_VAR && arg1_2->type == TERM_ATOM)) {
                return 0;
            }
        }
        
        return 1; // Same pattern for recursion purposes
    }
    
    if (goal1->type == TERM_ATOM && goal2->type == TERM_ATOM) {
        return strcmp(goal1->data.atom, goal2->data.atom) == 0;
    }
    
    return terms_equal(goal1, goal2);
}

// Goal cache functions for memoization
void init_goal_cache(goal_cache_t* cache) {
    cache->count = 0;
}

int check_goal_cache(goal_cache_t* cache, term_t* goal) {
    for (int i = 0; i < cache->count; i++) {
        if (terms_equal(cache->goals[i], goal)) {
            return cache->results[i];
        }
    }
    return 0; // Not found in cache
}

void add_goal_cache(goal_cache_t* cache, term_t* goal, int result) {
    if (cache->count < MAX_GOAL_CACHE) {
        cache->goals[cache->count] = copy_term(goal);
        cache->results[cache->count] = result;
        cache->count++;
    }
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

// Check if a fact exists in the knowledge base
int fact_exists(linear_kb_t* kb, term_t* fact) {
    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
        if (!resource->consumed && terms_equal(resource->fact, fact)) {
            return 1;
        }
    }
    return 0;
}

// Linear resolve disjunctive query
int linear_resolve_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path) {
    // Try to satisfy any one of the goals (OR logic)
    for (int i = 0; i < goal_count; i++) {
        // Create a single-goal array
        term_t* single_goal[1] = { goals[i] };
        
        // Save the consumed state before trying this goal
        consumed_state_t* saved_state = save_consumed_state(kb);
        
        // Try to resolve this single goal
        int result = linear_resolve_query_with_path(kb, single_goal, 1, goals[i], NULL, path);
        
        if (result) {
            // Success! This goal was satisfied
            return 1;
        }
        
        // Failed, restore state and try next goal
        restore_consumed_state(saved_state);
    }
    
    // None of the goals could be satisfied
    return 0;
}

// Find matching resources for a rule
int find_matching_resources(linear_kb_t* kb, clause_t* rule, linear_resource_t** resources, substitution_t* subst) {
    (void)subst; // Suppress unused parameter warning
    // This is a simplified implementation
    // In practice, this would need to find all combinations of resources
    // that can unify with the rule's body terms
    
    int matched_count = 0;
    
    for (int i = 0; i < rule->body_size && matched_count < 10; i++) {
        term_t* body_term = rule->body[i];
        
        // Find a resource that matches this body term
        for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
            if (!resource->consumed) {
                substitution_t temp_subst = {0};
                if (unify(body_term, resource->fact, &temp_subst)) {
                    resources[matched_count++] = resource;
                    // In a full implementation, we'd compose substitutions here
                    break;
                }
            }
        }
    }
    
    return matched_count == rule->body_size;
}

// Check if body conditions can be satisfied
int can_satisfy_body_conditions(linear_kb_t* kb, clause_t* rule, int body_index, 
                               linear_resource_t** used_resources) {
    // Simplified implementation
    if (body_index >= rule->body_size) {
        return 1; // All body terms satisfied
    }
    
    term_t* body_term = rule->body[body_index];
    
    // Find an available resource that matches this body term
    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
        if (!resource->consumed && terms_equal(resource->fact, body_term)) {
            used_resources[body_index] = resource;
            return can_satisfy_body_conditions(kb, rule, body_index + 1, used_resources);
        }
    }
    
    return 0;
}

// Check if substitutions are equal
int substitutions_equal(substitution_t* s1, substitution_t* s2) {
    if (s1->count != s2->count) {
        return 0;
    }
    
    for (int i = 0; i < s1->count; i++) {
        int found = 0;
        for (int j = 0; j < s2->count; j++) {
            if (strcmp(s1->bindings[i].var, s2->bindings[j].var) == 0 &&
                terms_equal(s1->bindings[i].term, s2->bindings[j].term)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    
    return 1;
}

// Wrapper function for external calls (top-level)
int linear_resolve_query_with_substitution_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                  term_t** original_goals, int original_goal_count, substitution_t* global_subst, 
                                                  enhanced_solution_list_t* solutions) {
    goal_stack_t stack;
    init_goal_stack(&stack);
    return linear_resolve_query_with_substitution_enhanced_internal(kb, goals, goal_count, original_goals, original_goal_count, global_subst, solutions, 1, &stack);
}

// Free linear knowledge base
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
    
    free(kb);
}

// Linear resolve query with path
int linear_resolve_query_with_path(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst, linear_path_t* path) {
    (void)path; // Suppress unused parameter warning
    // This is a more advanced version that tracks the path of resource consumption
    // For now, delegate to the simpler version
    return linear_resolve_query_with_substitution(kb, goals, goal_count, original_query, global_subst);
}

// Linear resolve query with type checking
int linear_resolve_query_with_type(linear_kb_t* kb, term_t** goals, int goal_count, int is_disjunctive) {
    if (is_disjunctive) {
        linear_path_t* path = create_linear_path();
        int result = linear_resolve_disjunctive(kb, goals, goal_count, path);
        free_linear_path(path);
        return result;
    } else {
        return linear_resolve_query(kb, goals, goal_count);
    }
}

// Linear resolve query with substitution - FIXED for forward chaining
int linear_resolve_query_with_substitution(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst) {
    // This is the core query resolution function
    // It tries to satisfy all goals using available resources
    // For conjunctive queries, KB state changes persist between goals (forward chaining)
    
    #ifdef DEBUG
    printf("DEBUG: Resolving query with %d goals\n", goal_count);
    for (int i = 0; i < goal_count; i++) {
        printf("DEBUG: Goal %d: ", i);
        print_term(goals[i]);
        printf("\n");
    }
    printf("DEBUG: Knowledge base has %d rules and resources:\n", kb->rule_count);
    for (linear_resource_t* r = kb->resources; r != NULL; r = r->next) {
        printf("DEBUG: Resource: ");
        print_term(r->fact);
        printf(" (consumed: %d)\n", r->consumed);
    }
    for (int i = 0; i < kb->rule_count; i++) {
        printf("DEBUG: Rule %d head: ", i);
        print_term(kb->rules[i].head);
        printf(" body_size: %d\n", kb->rules[i].body_size);
    }
    #endif
    
    if (goal_count == 0) {
        return 1; // All goals satisfied
    }
    
    // Try to satisfy the first goal
    term_t* current_goal = goals[0];
    
    // First, check if the goal is a rule name and try to apply that specific rule
    if (current_goal->type == TERM_ATOM) {
        for (int rule_idx = 0; rule_idx < kb->rule_count; rule_idx++) {
            clause_t* rule = &kb->rules[rule_idx];
            
            // Check if this rule's head matches the goal (rule name)
            if (rule->head && rule->head->type == TERM_ATOM && 
                strcmp(rule->head->data.atom, current_goal->data.atom) == 0) {
                
                #ifdef DEBUG
                printf("DEBUG: Attempting to apply rule '%s'\n", current_goal->data.atom);
                #endif
                
                // Check if we can consume all the rule's body requirements
                int can_apply = 1;
                linear_resource_t* consumed_resources[10] = {0};
                int consumed_count = 0;
                
                for (int body_idx = 0; body_idx < rule->body_size; body_idx++) {
                    term_t* body_term = rule->body[body_idx];
                    int found = 0;
                    
                    // Try to find a resource that matches this body term (with type compatibility)
                    // For now, we still take the first match, but we could implement backtracking here
                    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
                        if (!resource->consumed && can_unify_with_type(kb, body_term, resource->fact)) {
                            resource->consumed = 1;
                            consumed_resources[consumed_count++] = resource;
                            found = 1;
                            #ifdef DEBUG
                            printf("DEBUG: Consuming resource: ");
                            print_term(resource->fact);
                            printf("\n");
                            #endif
                            break; // TODO: For full backtracking, we'd need to try all alternatives
                        }
                    }
                    
                    if (!found) {
                        #ifdef DEBUG
                        printf("DEBUG: Cannot find resource for: ");
                        print_term(body_term);
                        printf("\n");
                        #endif
                        can_apply = 0;
                        break;
                    }
                }
                
                if (can_apply && rule->production) {
                    // Rule can be applied, add its production as a new fact
                    linear_resource_t* new_resource = malloc(sizeof(linear_resource_t));
                    new_resource->fact = copy_term(rule->production);
                    new_resource->consumed = 0;
                    new_resource->next = kb->resources;
                    kb->resources = new_resource;
                    
                    #ifdef DEBUG
                    printf("DEBUG: Rule applied successfully, produced: ");
                    print_term(new_resource->fact);
                    printf("\n");
                    #endif
                    
                    // Try to satisfy remaining goals WITHOUT backtracking
                    // This allows forward chaining to work properly
                    if (goal_count > 1) {
                        int result = linear_resolve_query_with_substitution(kb, goals + 1, goal_count - 1, original_query, global_subst);
                        if (result) {
                            return 1; // Success - keep all state changes
                        }
                        // If remaining goals fail, we still don't backtrack individual rule applications
                        // This is the key change for forward chaining
                    } else {
                        return 1; // This was the last goal and rule applied successfully
                    }
                } else if (!can_apply) {
                    // Restore consumed resources for this failed attempt
                    for (int i = 0; i < consumed_count; i++) {
                        consumed_resources[i]->consumed = 0;
                    }
                }
            }
        }
    }
    
    // Check if we have a direct fact that matches this goal
    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
        if (!resource->consumed) {
            substitution_t temp_subst = {0};
            if (unify(current_goal, resource->fact, &temp_subst)) {
                #ifdef DEBUG
                printf("DEBUG: Found direct fact match: ");
                print_term(resource->fact);
                printf("\n");
                #endif
                
                // Found a matching fact, consume it and continue with remaining goals
                resource->consumed = 1;
                
                // Compose the substitution
                if (global_subst) {
                    compose_substitutions(global_subst, &temp_subst);
                }
                
                // Try to satisfy remaining goals
                if (goal_count > 1) {
                    int result = linear_resolve_query_with_substitution(kb, goals + 1, goal_count - 1, original_query, global_subst);
                    if (result) {
                        return 1; // Success - keep the consumed state
                    }
                    // Don't backtrack here for forward chaining
                } else {
                    return 1; // This was the last goal and it matched
                }
                
                // Only restore if no remaining goals or they all failed
                resource->consumed = 0;
            }
        }
    }
    
    return 0; // Failed to satisfy all goals
}

// Check if an enhanced solution is equivalent to a substitution
int enhanced_solutions_are_equivalent(enhanced_solution_t* solution, substitution_t* subst) {
    if (solution->binding_count != subst->count) {
        return 0;
    }
    
    for (int i = 0; i < subst->count; i++) {
        int found = 0;
        for (int j = 0; j < solution->binding_count; j++) {
            if (string_equal(subst->bindings[i].var, solution->bindings[j].var_name) &&
                terms_equal(subst->bindings[i].term, solution->bindings[j].value)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    
    return 1;
}

// Simple solution list for backward compatibility
solution_list_t* create_solution_list() {
    solution_list_t* list = malloc(sizeof(solution_list_t));
    list->count = 0;
    list->capacity = 10;
    list->solutions = malloc(sizeof(substitution_t) * list->capacity);
    return list;
}

void add_solution(solution_list_t* list, substitution_t* solution) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->solutions = realloc(list->solutions, sizeof(substitution_t) * list->capacity);
    }
    // Copy the solution
    list->solutions[list->count] = *solution;
    list->count++;
}

void free_solution_list(solution_list_t* list) {
    if (list) {
        free(list->solutions);
        free(list);
    }
}

// Persistent fact functions
void add_persistent_fact(linear_kb_t* kb, term_t* fact) {
    // Add as a linear resource but mark it as persistent
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = fact;
    resource->consumed = 0;
    resource->persistent = 1; // Mark as persistent (non-consumable)
    resource->next = kb->resources;
    kb->resources = resource;
}

// Enhanced solution functions for variable binding support
enhanced_solution_list_t* create_enhanced_solution_list() {
    enhanced_solution_list_t* list = malloc(sizeof(enhanced_solution_list_t));
    list->count = 0;
    list->capacity = 10;
    list->solutions = malloc(sizeof(enhanced_solution_t) * list->capacity);
    return list;
}

// Check if two solutions are equivalent (same variable bindings)
int solutions_are_equivalent(substitution_t* s1, substitution_t* s2) {
    if (s1->count != s2->count) {
        return 0;
    }
    
    for (int i = 0; i < s1->count; i++) {
        int found = 0;
        for (int j = 0; j < s2->count; j++) {
            if (strcmp(s1->bindings[i].var, s2->bindings[j].var) == 0 &&
                terms_equal(s1->bindings[i].term, s2->bindings[j].term)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    
    return 1;
}

void add_enhanced_solution(enhanced_solution_list_t* list, substitution_t* subst) {
    // Check for duplicate solutions before adding
    for (int i = 0; i < list->count; i++) {
        if (enhanced_solutions_are_equivalent(&list->solutions[i], subst)) {
#ifdef DEBUG
            printf("DEBUG: Skipping duplicate solution\n");
#endif
            return; // Skip duplicate
        }
    }
    
#ifdef DEBUG
    printf("DEBUG: No duplicate found, adding new solution\n");
#endif
    
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->solutions = realloc(list->solutions, sizeof(enhanced_solution_t) * list->capacity);
    }
    
#ifdef DEBUG
    printf("DEBUG: Adding solution %d with %d bindings\n", list->count + 1, subst->count);
    for (int i = 0; i < subst->count; i++) {
        printf("DEBUG:   %s = ", subst->bindings[i].var);
        print_term(subst->bindings[i].term);
        printf("\n");
    }
#endif
    
    enhanced_solution_t* solution = &list->solutions[list->count];
    solution->substitution = *subst;
    solution->binding_count = subst->count;
    solution->bindings = malloc(sizeof(variable_binding_t) * solution->binding_count);
    
    for (int i = 0; i < subst->count; i++) {
        solution->bindings[i].var_name = strdup(subst->bindings[i].var);
        solution->bindings[i].value = copy_term(subst->bindings[i].term);
    }
    
    list->count++;
}

void print_enhanced_solution(enhanced_solution_t* solution) {
    if (solution->binding_count == 0) {
        printf("true");
        return;
    }
    
    for (int i = 0; i < solution->binding_count; i++) {
        if (i > 0) printf(", ");
        printf("%s = ", solution->bindings[i].var_name);
        print_term(solution->bindings[i].value);
    }
}

void free_enhanced_solution_list(enhanced_solution_list_t* list) {
    if (!list) return;
    
    for (int i = 0; i < list->count; i++) {
        enhanced_solution_t* solution = &list->solutions[i];
        for (int j = 0; j < solution->binding_count; j++) {
            free(solution->bindings[j].var_name);
            free_term(solution->bindings[j].value);
        }
        free(solution->bindings);
    }
    
    free(list->solutions);
    free(list);
}

int linear_resolve_query_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    goal_stack_t stack;
    init_goal_stack(&stack);
    return linear_resolve_query_enhanced_with_stack(kb, goals, goal_count, solutions, &stack);
}

int linear_resolve_query_enhanced_with_stack(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions, goal_stack_t* stack) {
    substitution_t initial_subst = {0};
    return linear_resolve_query_with_substitution_enhanced_internal(kb, goals, goal_count, goals, goal_count, &initial_subst, solutions, 1, stack);
}

// Enhanced disjunctive resolution: try to satisfy any one of the goals (OR logic)
int linear_resolve_query_enhanced_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    int found_any = 0;
    
    // Try each goal independently
    for (int i = 0; i < goal_count; i++) {
        // Create a single-goal array
        term_t* single_goal[1] = { goals[i] };
        
        // Save the consumed state before trying this goal
        consumed_state_t* saved_state = save_consumed_state(kb);
        
        // Try to resolve this single goal with enhanced resolution
        enhanced_solution_list_t* goal_solutions = create_enhanced_solution_list();
        int result = linear_resolve_query_enhanced(kb, single_goal, 1, goal_solutions);
        
        if (result && goal_solutions->count > 0) {
            // Success! Add all solutions from this goal
            for (int j = 0; j < goal_solutions->count; j++) {
                add_enhanced_solution(solutions, &goal_solutions->solutions[j].substitution);
            }
            found_any = 1;
        }
        
        // Restore state for next goal attempt
        restore_consumed_state(saved_state);
        free_enhanced_solution_list(goal_solutions);
    }
    
    return found_any;
}

int linear_resolve_query_with_substitution_enhanced_internal(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                  term_t** original_goals, int original_goal_count, substitution_t* global_subst, 
                                                  enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack) {
    static int total_iterations = 0;
    total_iterations++;
    
    // Prevent excessive recursion depth or total iterations
    if (rule_depth > 10 || total_iterations > 500) {
#ifdef DEBUG
        printf("DEBUG: Maximum recursion depth or iterations reached (depth=%d, iterations=%d), terminating\n", rule_depth, total_iterations);
#endif
        return 0;
    }
    if (goal_count == 0) {
        // Add solution when all original goals are satisfied (regardless of rule depth)
        char* original_vars[MAX_VARS];
        int original_var_count = 0;
        extract_variables_from_goals(original_goals, original_goal_count, original_vars, &original_var_count, MAX_VARS);
        
        if (original_var_count == 0 || all_variables_bound(original_vars, original_var_count, global_subst)) {
#ifdef DEBUG
            printf("DEBUG: Adding solution (rule_depth = %d)\n", rule_depth);
#endif
            // Create a filtered substitution with only original query variables
            substitution_t filtered_subst = {0};
            create_filtered_substitution(global_subst, original_vars, original_var_count, &filtered_subst);
            add_enhanced_solution(solutions, &filtered_subst);
            
            // Clean up filtered substitution
            for (int i = 0; i < filtered_subst.count; i++) {
                free(filtered_subst.bindings[i].var);
                free_term(filtered_subst.bindings[i].term);
            }
        }
        
        free_variable_list(original_vars, original_var_count);
        return 1;
    }

    term_t* current_goal = goals[0];
    term_t** remaining_goals = goals + 1;
    int remaining_count = goal_count - 1;
    int found_any = 0;

    // Try matching against facts (both linear and persistent)
    for (linear_resource_t* resource = kb->resources; resource; resource = resource->next) {
        if (resource->consumed) continue; // Skip consumed resources

#ifdef DEBUG
        printf("DEBUG: Checking resource: ");
        print_term(resource->fact);
        printf(" (persistent: %d)\n", resource->persistent);
        printf("DEBUG: Against goal: ");
        print_term(current_goal);
        printf("\n");
#endif

        substitution_t local_subst = *global_subst;
        int unified = unify(current_goal, resource->fact, &local_subst);
        if (!unified) {
            // Try type-aware matching only if regular unification failed
            unified = can_unify_with_type(kb, current_goal, resource->fact);
            if (unified) {
                // Reset substitution since type matching doesn't update it
                local_subst = *global_subst;
            }
        }
        
        if (unified) {
#ifdef DEBUG
            printf("DEBUG: Unification successful!\n");
#endif
            // Consume the resource only if it's linear (not persistent)
            if (!resource->persistent) {
                resource->consumed = 1;
            }
            
            if (linear_resolve_query_with_substitution_enhanced_internal(kb, remaining_goals, remaining_count, original_goals, original_goal_count, &local_subst, solutions, 0, stack)) {
                found_any = 1;
            }
            
            // Restore the resource state only for linear resources
            if (!resource->persistent) {
                resource->consumed = 0;
            }
        }
#ifdef DEBUG
        else {
            printf("DEBUG: Unification failed\n");
        }
#endif
    }



    // Try applying rules
    for (int i = 0; i < kb->rule_count; i++) {
        clause_t* rule = &kb->rules[i];
        if (try_rule_with_backtracking_enhanced(kb, rule, goals, goal_count, original_goals, original_goal_count, global_subst, solutions, rule_depth, stack)) {
            found_any = 1;
        }
    }

    return found_any;
}

int try_rule_with_backtracking_enhanced(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                       term_t** original_goals, int original_goal_count, substitution_t* global_subst, enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack) {
    term_t* current_goal = goals[0];
    substitution_t rule_subst = *global_subst;
    
    // Check for recursion if this is a recursive rule
    // Use improved pattern matching for similar goals with same ground arguments
    if (rule->is_recursive && is_goal_pattern_in_stack(stack, current_goal)) {
#ifdef DEBUG
        printf("DEBUG: Recursion detected for goal: ");
        print_term(current_goal);
        printf(", skipping recursive rule\n");
#endif
        return 0;
    }
    
#ifdef DEBUG
    printf("DEBUG: Trying rule for goal: ");
    print_term(current_goal);
    printf(" (rule depth: %d)\n", rule_depth);
#endif

    // Rename variables in the rule to avoid conflicts with existing variables
    static int rule_instance_counter = 0;
    rule_instance_counter++;
    
    term_t* renamed_head = rename_variables_in_term(rule->head, rule_instance_counter);
    term_t* renamed_production = rule->production ? rename_variables_in_term(rule->production, rule_instance_counter) : NULL;
    
#ifdef DEBUG
    printf("DEBUG: Original rule head: ");
    print_term(rule->head);
    printf("\n");
    printf("DEBUG: Renamed rule head: ");
    print_term(renamed_head);
    printf("\n");
#endif

    // For production rules, unify with the production; for regular rules, unify with the head
    term_t* unify_target = renamed_production ? renamed_production : renamed_head;
    
    // Try to unify current goal with rule production/head
    if (!unify(current_goal, unify_target, &rule_subst)) {
#ifdef DEBUG
        printf("DEBUG: Rule unification failed\n");
#endif
        free_term(renamed_head);
        if (renamed_production) free_term(renamed_production);
        return 0;
    }

#ifdef DEBUG
    printf("DEBUG: Rule unified successfully with ");
    print_term(unify_target);
    printf("\n");
#endif

    // Push goal onto stack to detect recursion
    if (rule->is_recursive) {
        if (!push_goal(stack, current_goal)) {
#ifdef DEBUG
            printf("DEBUG: Goal stack overflow\n");
#endif
            free_term(renamed_head);
            if (renamed_production) free_term(renamed_production);
            return 0;
        }
#ifdef DEBUG
        printf("DEBUG: Pushed goal onto recursion stack (depth now: %d)\n", stack->depth);
#endif
    } else {
#ifdef DEBUG
        printf("DEBUG: Rule is not recursive, skipping goal stack push\n");
#endif
    }

#ifdef DEBUG
    printf("DEBUG: About to apply substitution to rule body (body_size: %d)\n", rule->body_size);
#endif

    // Apply substitution to rule body (also rename variables)
    term_t** instantiated_body = malloc(sizeof(term_t*) * rule->body_size);
    for (int i = 0; i < rule->body_size; i++) {
        term_t* renamed_body_term = rename_variables_in_term(rule->body[i], rule_instance_counter);
#ifdef DEBUG
        printf("DEBUG: Applying substitution to body term %d: ", i);
        print_term(renamed_body_term);
        printf("\n");
        printf("DEBUG: Substitution has %d bindings:\n", rule_subst.count);
        for (int j = 0; j < rule_subst.count; j++) {
            printf("DEBUG:   %s = ", rule_subst.bindings[j].var);
            print_term(rule_subst.bindings[j].term);
            printf("\n");
        }
#endif
        instantiated_body[i] = apply_substitution(renamed_body_term, &rule_subst);
        free_term(renamed_body_term);
#ifdef DEBUG
        printf("DEBUG: Result: ");
        print_term(instantiated_body[i]);
        printf("\n");
#endif
    }

#ifdef DEBUG
    printf("DEBUG: Successfully instantiated rule body\n");
#endif

    // Try to satisfy the rule body with depth-first backtracking
    // We need to find each way the body can be satisfied and for each way:
    // 1. Apply the rule production 
    // 2. Continue with remaining goals
    // 3. Backtrack and try the next way
    
    int found = 0;
    if (rule->body_size == 0) {
        // Empty body - proceed directly to rule completion
        linear_resource_t* produced_resource = NULL;
        if (renamed_production) {
            term_t* produced_term = apply_substitution(renamed_production, &rule_subst);
            produced_resource = malloc(sizeof(linear_resource_t));
            produced_resource->fact = produced_term;
            produced_resource->consumed = 0;
            produced_resource->persistent = 0;
            produced_resource->next = kb->resources;
            kb->resources = produced_resource;
#ifdef DEBUG
            printf("DEBUG: Produced resource: ");
            print_term(produced_term);
            printf("\n");
#endif
        }

        // Continue with remaining goals
        term_t** remaining_goals = goals + 1;
        int remaining_count = goal_count - 1;
#ifdef DEBUG
        printf("DEBUG: Continuing with %d remaining goals\n", remaining_count);
#endif
        found = linear_resolve_query_with_substitution_enhanced_internal(kb, remaining_goals, remaining_count, original_goals, original_goal_count, &rule_subst, solutions, rule_depth, stack);

        // Clean up produced resource (backtrack)
        if (produced_resource) {
            if (kb->resources == produced_resource) {
                kb->resources = produced_resource->next;
            } else {
                linear_resource_t* prev = kb->resources;
                while (prev && prev->next != produced_resource) {
                    prev = prev->next;
                }
                if (prev) {
                    prev->next = produced_resource->next;
                }
            }
            free_term(produced_resource->fact);
            free(produced_resource);
        }
    } else {
        // Non-empty body - use depth-first backtracking to try each way to satisfy the body
        found = try_rule_body_depth_first(kb, rule, goals, goal_count, original_goals, original_goal_count, &rule_subst, solutions, rule_depth, instantiated_body, stack);
    }

    // Clean up
    for (int i = 0; i < rule->body_size; i++) {
        free_term(instantiated_body[i]);
    }
    free(instantiated_body);
    free_term(renamed_head);
    if (renamed_production) free_term(renamed_production);

    // Pop goal from stack
    if (rule->is_recursive) {
        pop_goal(stack);
#ifdef DEBUG
        printf("DEBUG: Popped goal from recursion stack (depth now: %d)\n", stack->depth);
#endif
    }

    return found;
}

// Depth-first backtracking for rule body resolution
int try_rule_body_depth_first(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t** original_goals, int original_goal_count, substitution_t* rule_subst,
                              enhanced_solution_list_t* solutions, int rule_depth, term_t** instantiated_body, goal_stack_t* stack) {
    
    // This function should find each way to satisfy the rule body using depth-first backtracking
    // For each way the body is satisfied:
    // 1. Apply the substitution from body satisfaction  
    // 2. Produce the rule production
    // 3. Continue with remaining goals
    // 4. Backtrack to try the next way
    
    int found_any = 0;
    
    // Use a recursive helper to resolve the rule body with depth-first backtracking
    // We need to resolve the body goals one by one, not all at once
    if (resolve_rule_body_recursive(kb, instantiated_body, rule->body_size, 0, rule_subst, 
                                   rule, goals + 1, goal_count - 1, original_goals, original_goal_count, 
                                   solutions, rule_depth, stack)) {
        found_any = 1;
    }
    
    return found_any;
}

// Recursive helper for depth-first rule body resolution
int resolve_rule_body_recursive(linear_kb_t* kb, term_t** body_goals, int body_count, int body_index,
                                substitution_t* current_subst, clause_t* rule, term_t** remaining_goals, int remaining_count,
                                term_t** original_goals, int original_goal_count, enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack) {
    
    if (body_index >= body_count) {
        // All body goals satisfied - now apply rule production and continue with remaining goals
#ifdef DEBUG
        printf("DEBUG: All rule body goals satisfied, applying production\n");
#endif
        
        linear_resource_t* produced_resource = NULL;
        if (rule->production) {
            term_t* produced_term = apply_substitution(rule->production, current_subst);
            produced_resource = malloc(sizeof(linear_resource_t));
            produced_resource->fact = produced_term;
            produced_resource->consumed = 0;
            produced_resource->persistent = 0;
            produced_resource->next = kb->resources;
            kb->resources = produced_resource;
#ifdef DEBUG
            printf("DEBUG: Produced resource: ");
            print_term(produced_term);
            printf("\n");
#endif
        }

        // Continue with remaining goals after rule application
#ifdef DEBUG
        printf("DEBUG: Continuing with %d remaining goals after rule completion\n", remaining_count);
#endif
        int found = linear_resolve_query_with_substitution_enhanced_internal(kb, remaining_goals, remaining_count, 
                                                                           original_goals, original_goal_count, 
                                                                           current_subst, solutions, rule_depth, stack);

        // Clean up produced resource (backtrack)
        if (produced_resource) {
            if (kb->resources == produced_resource) {
                kb->resources = produced_resource->next;
            } else {
                linear_resource_t* prev = kb->resources;
                while (prev && prev->next != produced_resource) {
                    prev = prev->next;
                }
                if (prev) {
                    prev->next = produced_resource->next;
                }
            }
            free_term(produced_resource->fact);
            free(produced_resource);
        }
        
        return found;
    }
    
    // Resolve current body goal using depth-first backtracking
    term_t* current_body_goal = body_goals[body_index];
    int found_any = 0;
    
#ifdef DEBUG
    printf("DEBUG: Resolving rule body goal %d: ", body_index);
    print_term(current_body_goal);
    printf("\n");
#endif
    
    // Try to match current body goal against available resources
    for (linear_resource_t* resource = kb->resources; resource; resource = resource->next) {
        if (resource->consumed) continue;
        
        substitution_t local_subst = *current_subst;
        int unified = unify(current_body_goal, resource->fact, &local_subst);
        if (!unified) {
            unified = can_unify_with_type(kb, current_body_goal, resource->fact);
            if (unified) {
                local_subst = *current_subst;
            }
        }
        
        if (unified) {
#ifdef DEBUG
            printf("DEBUG: Rule body goal unified with resource: ");
            print_term(resource->fact);
            printf("\n");
#endif
            // Consume resource and recurse to next body goal
            if (!resource->persistent) {
                resource->consumed = 1;
            }
            
            if (resolve_rule_body_recursive(kb, body_goals, body_count, body_index + 1, &local_subst,
                                           rule, remaining_goals, remaining_count, original_goals, original_goal_count,
                                           solutions, rule_depth, stack)) {
                found_any = 1;
            }
            
            // Restore resource (backtrack)
            if (!resource->persistent) {
                resource->consumed = 0;
            }
        }
    }
    
    // If no direct resource matching worked, try applying rules to resolve the body goal
    if (!found_any) {
#ifdef DEBUG
        printf("DEBUG: No direct resource match for body goal, trying rules\n");
#endif
        // Apply current substitution to the body goal before trying to resolve it
        term_t* instantiated_goal = apply_substitution(current_body_goal, current_subst);
        
#ifdef DEBUG
        printf("DEBUG: Instantiated body goal for rule resolution: ");
        print_term(instantiated_goal);
        printf("\n");
#endif
        
        // Try to resolve the instantiated body goal using rules
        term_t* body_goal_array[1] = { instantiated_goal };
        
        // Try to resolve this single goal - if it succeeds, continue with next body goal
        if (linear_resolve_query_with_substitution_enhanced_internal(kb, body_goal_array, 1, 
                                                                    original_goals, original_goal_count, current_subst, 
                                                                    solutions, rule_depth + 1, stack)) {
#ifdef DEBUG
            printf("DEBUG: Body goal resolved successfully via rules, continuing with next body goal\n");
#endif                                                           
            if (resolve_rule_body_recursive(kb, body_goals, body_count, body_index + 1, current_subst,
                                           rule, remaining_goals, remaining_count, original_goals, original_goal_count,
                                           solutions, rule_depth, stack)) {
                found_any = 1;
            }
        }
        
        free_term(instantiated_goal);
    }

    return found_any;
}

// Extract all variables from a term
void extract_variables_from_term(term_t* term, char** vars, int* var_count, int max_vars) {
    if (!term || *var_count >= max_vars) return;
    
    switch (term->type) {
        case TERM_VAR:
            // Check if this variable is already in the list
            for (int i = 0; i < *var_count; i++) {
                if (strcmp(vars[i], term->data.var) == 0) {
                    return; // Already have this variable
                }
            }
            // Add new variable
            vars[*var_count] = strdup(term->data.var);
            (*var_count)++;
            break;
            
        case TERM_COMPOUND:
            for (int i = 0; i < term->data.compound.arity; i++) {
                extract_variables_from_term(term->data.compound.args[i], vars, var_count, max_vars);
            }
            break;
            
        case TERM_CLONE:
            extract_variables_from_term(term->data.cloned, vars, var_count, max_vars);
            break;
            
        case TERM_ATOM:
        case TERM_INTEGER:
            // No variables in atoms or integers
            break;
    }
}

// Extract all variables from multiple goals
void extract_variables_from_goals(term_t** goals, int goal_count, char** vars, int* var_count, int max_vars) {
    for (int i = 0; i < goal_count && *var_count < max_vars; i++) {
        extract_variables_from_term(goals[i], vars, var_count, max_vars);
    }
}

// Check if all variables are bound to concrete values (not just other variables)
int all_variables_bound(char** vars, int var_count, substitution_t* subst) {
    for (int i = 0; i < var_count; i++) {
        term_t* final_value = resolve_variable_chain(subst, vars[i]);
        if (!final_value || final_value->type == TERM_VAR) {
            // Variable is not bound or is bound to another variable
            if (final_value) free_term(final_value);
            return 0;
        }
        free_term(final_value);
    }
    return 1; // All variables bound to concrete values
}

// Free variable list
void free_variable_list(char** vars, int var_count) {
    for (int i = 0; i < var_count; i++) {
        free(vars[i]);
    }
}

// Create a filtered substitution containing only the specified variables
// Also resolves chains of variables to their final values
void create_filtered_substitution(substitution_t* full_subst, char** target_vars, int target_count, substitution_t* filtered_subst) {
    filtered_subst->count = 0;
    
    for (int i = 0; i < target_count; i++) {
        char* var = target_vars[i];
        term_t* final_value = resolve_variable_chain(full_subst, var);
        
        if (final_value) {
            // Add this binding to the filtered substitution
            if (filtered_subst->count < MAX_VARS) {
                filtered_subst->bindings[filtered_subst->count].var = malloc(strlen(var) + 1);
                strcpy(filtered_subst->bindings[filtered_subst->count].var, var);
                filtered_subst->bindings[filtered_subst->count].term = copy_term(final_value);
                filtered_subst->count++;
            }
            free_term(final_value);
        }
    }
}

// Resolve a variable through a chain of substitutions to get its final value
term_t* resolve_variable_chain(substitution_t* subst, const char* var) {
    for (int i = 0; i < subst->count; i++) {
        if (string_equal(subst->bindings[i].var, var)) {
            term_t* value = subst->bindings[i].term;
            
            // If the value is a variable, recursively resolve it
            if (value->type == TERM_VAR) {
                return resolve_variable_chain(subst, value->data.var);
            } else {
                return copy_term(value);
            }
        }
    }
    return NULL; // Variable not found
}
