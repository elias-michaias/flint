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

// Apply a substitution to a term, returning a new term with variables replaced
term_t* apply_substitution(term_t* term, substitution_t* subst) {
    if (!term || !subst) return copy_term(term);
    
    switch (term->type) {
        case TERM_VAR:
            // Look for this variable in the substitution
            for (int i = 0; i < subst->count; i++) {
                if (strcmp(term->data.var, subst->bindings[i].var) == 0) {
                    return copy_term(subst->bindings[i].term);
                }
            }
            // Variable not found in substitution, return as-is
            return copy_term(term);
            
        case TERM_COMPOUND:
            // Apply substitution recursively to all arguments
            if (term->data.compound.arity == 0) {
                return copy_term(term);
            }
            
            term_t** new_args = malloc(sizeof(term_t*) * term->data.compound.arity);
            for (int i = 0; i < term->data.compound.arity; i++) {
                new_args[i] = apply_substitution(term->data.compound.args[i], subst);
            }
            
            return create_compound(term->data.compound.functor, new_args, term->data.compound.arity);
            
        case TERM_CLONE:
            return create_clone(apply_substitution(term->data.cloned, subst));
            
        case TERM_ATOM:
        case TERM_INTEGER:
            // These don't contain variables, return as-is
            return copy_term(term);
    }
    
    return copy_term(term);
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
    kb->resource_count = 0;
    kb->type_mappings = NULL;
    kb->union_mappings = NULL;
    kb->persistent_facts = NULL;
    kb->auto_deallocate = 0; // Default: auto deallocation disabled
    kb->total_memory_allocated = 0;
    kb->peak_memory_usage = 0;
    kb->checkpoint_count = 0;
    kb->checkpoints = NULL;
    kb->consumption_metadata = NULL; // Initialize consumption metadata
    return kb;
}

// Add a linear fact (resource that can be consumed)
void add_linear_fact(linear_kb_t* kb, term_t* fact) {
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = copy_term(fact);
    resource->consumed = 0;
    resource->persistent = 0; // Mark as linear (consumable)
    resource->deallocated = 0;
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = "fact";
    resource->next = kb->resources;
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: MEMORY ALLOCATED - Added resource: ");
    print_term(fact);
    printf(" (allocated %zu bytes, total resources: %d)\n", resource->memory_size, kb->resource_count);
    #endif
}

// Set automatic deallocation mode
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
    resource->consumed = 0;
    resource->deallocated = 0;
    resource->persistent = 0;  // Linear but optional
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = "optional_fact";
    resource->next = kb->resources;
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: Added optional linear fact: ");
    print_term(fact);
    printf(" (estimated size: %zu bytes)\n", resource->memory_size);
    #endif
}

// Enhanced fact addition with exponential support
void add_exponential_linear_fact(linear_kb_t* kb, term_t* fact) {
    // Exponential facts are essentially persistent but with different semantics
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = fact;
    resource->consumed = 0;
    resource->deallocated = 0;
    resource->persistent = 2;  // 2 = exponential (can be used multiple times)
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = "exponential_fact";
    resource->next = kb->resources;
    kb->resources = resource;
    kb->resource_count++;
    
    #ifdef DEBUG
    printf("DEBUG: Added exponential linear fact: ");
    print_term(fact);
    printf(" (estimated size: %zu bytes)\n", resource->memory_size);
    #endif
}

// Memory size estimation for automatic management
size_t estimate_term_memory_size(term_t* term) {
    if (!term) return 0;
    
    size_t size = sizeof(term_t);
    
    switch (term->type) {
        case TERM_ATOM:
            if (term->data.atom) {
                size += strlen(term->data.atom) + 1;
            }
            break;
        case TERM_VAR:
            if (term->data.var) {
                size += strlen(term->data.var) + 1;
            }
            break;
        case TERM_COMPOUND:
            if (term->data.compound.functor) {
                size += strlen(term->data.compound.functor) + 1;
            }
            size += term->data.compound.arity * sizeof(term_t*);
            for (int i = 0; i < term->data.compound.arity; i++) {
                size += estimate_term_memory_size(term->data.compound.args[i]);
            }
            break;
        case TERM_CLONE:
            size += estimate_term_memory_size(term->data.cloned);
            break;
        case TERM_INTEGER:
            // Just the union size, already counted in sizeof(term_t)
            break;
    }
    
    return size;
}

// Automatic deallocation when resource is consumed
void auto_deallocate_resource(linear_kb_t* kb, linear_resource_t* resource) {
    if (!kb->auto_deallocate || resource->persistent) {
        return;  // Don't deallocate persistent or when auto-deallocation is disabled
    }
    
    if (!resource->deallocated) {
        #ifdef DEBUG
        printf("DEBUG: Auto-deallocating consumed resource: ");
        print_term(resource->fact);
        printf(" (freed %zu bytes)\n", resource->memory_size);
        #endif
        
        // Mark as deallocated but don't actually free yet (for debugging)
        resource->deallocated = 1;
        
        // In a production system, you might actually free the memory here:
        // free_term(resource->fact);
        // resource->fact = NULL;
    }
}

// Enhanced resource consumption with automatic deallocation
int consume_linear_resource_enhanced(linear_kb_t* kb, term_t* goal, substitution_t* subst) {
    linear_resource_t* current = kb->resources;
    
    while (current != NULL) {
        if (!current->consumed && !current->deallocated) {
            // Try to unify with the goal
            substitution_t temp_subst = {0};
            init_substitution(&temp_subst);
            copy_substitution(&temp_subst, subst);
            
            if (unify_terms(current->fact, goal, &temp_subst)) {
                // Resource matches - for persistent resources, don't mark as consumed
                if (current->persistent == 0) {
                    // Linear resource: consume it  
                    current->consumed = 1;
                    #ifdef DEBUG
                    printf("DEBUG: Consumed linear resource: ");
                    print_term(current->fact);
                    printf("\n");
                    #endif
                } else {
                    // Persistent resource: use but don't consume
                    #ifdef DEBUG
                    printf("DEBUG: Used persistent resource (not consumed): ");
                    print_term(current->fact);
                    printf("\n");
                    #endif
                }
                
                copy_substitution(subst, &temp_subst);
                
                // Auto-deallocate if enabled and not persistent/exponential
                if (current->persistent == 0) {  // Only auto-deallocate truly linear resources
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
    
    // Try rules to derive the goal (works for both atoms and compounds)
    for (int rule_idx = 0; rule_idx < kb->rule_count; rule_idx++) {
        clause_t* rule = &kb->rules[rule_idx];
        
        #ifdef DEBUG
        printf("DEBUG: Checking rule %d with head: ", rule_idx);
        print_term(rule->head);
        printf(" and production: ");
        if (rule->production) {
            print_term(rule->production);
        } else {
            printf("NULL");
        }
        printf("\n");
        #endif
        
        // Check if this rule can produce the goal we're looking for
        int rule_matches = 0;
        substitution_t rule_subst = {0};  // Move substitution to broader scope
        init_substitution(&rule_subst);
        
        if (rule->head && rule->head->type == TERM_ATOM && current_goal->type == TERM_ATOM &&
            strcmp(rule->head->data.atom, current_goal->data.atom) == 0) {
            // Direct head match (traditional Prolog-style rule for atoms)
            rule_matches = 1;
            #ifdef DEBUG
            printf("DEBUG: Rule head matches goal directly\n");
            #endif
        } else if (rule->production) {
            // Check if the rule's production matches the goal (linear logic production rule)
            if (unify_terms(rule->production, current_goal, &rule_subst)) {
                rule_matches = 1;
                #ifdef DEBUG
                printf("DEBUG: Rule production matches goal: ");
                print_term(rule->production);
                printf(" with substitution: ");
                print_substitution(&rule_subst);
                printf("\n");
                #endif
            } else {
                #ifdef DEBUG
                printf("DEBUG: Rule production does not match goal\n");
                #endif
            }
        }
        
        if (rule_matches) {
                
                #ifdef DEBUG
                printf("DEBUG: Attempting to apply rule '%s'\n", current_goal->data.atom);
                #endif
                
                // Check if we can consume all the rule's body requirements
                int can_apply = 1;
                linear_resource_t* consumed_resources[10] = {0};
                int consumed_count = 0;
                
                for (int body_idx = 0; body_idx < rule->body_size; body_idx++) {
                    term_t* body_term = rule->body[body_idx];
                    
                    // Apply the substitution from rule-goal unification to the body term
                    term_t* substituted_body_term = apply_substitution(body_term, &rule_subst);
                    
                    int found = 0;
                    
                    #ifdef DEBUG
                    printf("DEBUG: Looking for resource matching: ");
                    print_term(substituted_body_term);
                    printf("\n");
                    #endif
                    
                    // Try to find a resource that matches this substituted body term
                    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
                        if (!resource->consumed && !resource->deallocated) {
                            substitution_t match_subst = {0};
                            init_substitution(&match_subst);
                            
                            if (unify_terms(substituted_body_term, resource->fact, &match_subst)) {
                                // Found a matching resource
                                if (resource->persistent == 0) {
                                    // Linear resource: consume and deallocate it
                                    resource->consumed = 1;
                                    consumed_resources[consumed_count++] = resource;
                                    
                                    #ifdef DEBUG
                                    printf("DEBUG: Consuming linear resource: ");
                                    print_term(resource->fact);
                                    printf("\n");
                                    #endif
                                    
                                    // Deallocate for memory efficiency
                                    auto_deallocate_resource(kb, resource);
                                    
                                } else {
                                    // Persistent resource: use but don't consume
                                    #ifdef DEBUG
                                    printf("DEBUG: Using persistent resource (not consumed): ");
                                    print_term(resource->fact);
                                    printf("\n");
                                    #endif
                                }
                                
                                found = 1;
                                free_substitution(&match_subst);
                                break;
                            }
                            free_substitution(&match_subst);
                        }
                    }
                    
                    free_term(substituted_body_term);
                    
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
            // Clean up rule substitution before trying next rule
            free_substitution(&rule_subst);
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

// Helper function to copy substitutions
void copy_substitution(substitution_t* dest, substitution_t* src) {
    if (!dest || !src) return;
    dest->count = src->count;
    for (int i = 0; i < src->count && i < MAX_VARS; i++) {
        dest->bindings[i] = src->bindings[i];
    }
}

// Basic unification for terms (simplified)
int unify_terms(term_t* term1, term_t* term2, substitution_t* subst) {
    if (!term1 || !term2) return 0;
    
    // Extract inner terms from persistent resources (cloned terms)
    term_t* actual_term1 = get_inner_term(term1);
    term_t* actual_term2 = get_inner_term(term2);
    
    if (actual_term1->type == TERM_VAR) {
        // Variable unifies with anything
        return add_binding(subst, actual_term1->data.var, actual_term2);
    }
    
    if (actual_term2->type == TERM_VAR) {
        // Variable unifies with anything  
        return add_binding(subst, actual_term2->data.var, actual_term1);
    }
    
    if (actual_term1->type == TERM_ATOM && actual_term2->type == TERM_ATOM) {
        return strcmp(actual_term1->data.atom, actual_term2->data.atom) == 0;
    }
    
    if (actual_term1->type == TERM_INTEGER && actual_term2->type == TERM_INTEGER) {
        return actual_term1->data.integer == actual_term2->data.integer;
    }
    
    if (actual_term1->type == TERM_COMPOUND && actual_term2->type == TERM_COMPOUND) {
        if (strcmp(actual_term1->data.compound.functor, actual_term2->data.compound.functor) != 0) {
            return 0;
        }
        if (actual_term1->data.compound.arity != actual_term2->data.compound.arity) {
            return 0;
        }
        for (int i = 0; i < actual_term1->data.compound.arity; i++) {
            if (!unify_terms(actual_term1->data.compound.args[i], actual_term2->data.compound.args[i], subst)) {
                return 0;
            }
        }
        return 1;
    }
    
    return 0; // Different types don't unify
}

// COMPILER-DIRECTED MEMORY MANAGEMENT FUNCTIONS
// Register consumption metadata from compiler  
void register_consumption_metadata(linear_kb_t* kb, const char* resource_name, const char* consumption_point,
                                   int is_optional, int is_persistent, size_t estimated_size) {
    consumption_metadata_t* metadata = malloc(sizeof(consumption_metadata_t));
    metadata->resource_name = malloc(strlen(resource_name) + 1);
    strcpy(metadata->resource_name, resource_name);
    metadata->consumption_point = malloc(strlen(consumption_point) + 1);
    strcpy(metadata->consumption_point, consumption_point);
    metadata->is_optional = is_optional;
    metadata->is_persistent = is_persistent;
    metadata->estimated_size = estimated_size;
    
    // Add to linked list
    metadata->next = kb->consumption_metadata;
    kb->consumption_metadata = metadata;
    
    #ifdef DEBUG
    printf("DEBUG: Registered consumption metadata for '%s' at '%s' (optional=%d, persistent=%d, size=%zu)\n",
           resource_name, consumption_point, is_optional, is_persistent, estimated_size);
    #endif
}

// Find consumption metadata for a resource
consumption_metadata_t* find_consumption_metadata(linear_kb_t* kb, const char* resource_name) {
    consumption_metadata_t* current = kb->consumption_metadata;
    while (current != NULL) {
        if (strcmp(current->resource_name, resource_name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Compiler-directed resource deallocation (immediate, precise)
void free_linear_resource(linear_kb_t* kb, linear_resource_t* resource) {
    if (resource && !resource->deallocated && !resource->persistent) {
        #ifdef DEBUG
        printf("DEBUG: Compiler-directed deallocation of resource: ");
        print_term(resource->fact);
        printf(" (freed %zu bytes)\n", resource->memory_size);
        #endif
        
        // Mark as deallocated
        resource->deallocated = 1;
        
        // In production, actually free the memory:
        // free_term(resource->fact);
        // resource->fact = NULL;
        
        // Update memory tracking
        if (kb->total_memory_allocated >= resource->memory_size) {
            kb->total_memory_allocated -= resource->memory_size;
        }
    }
}

// Check if resource should be deallocated at current execution point
int should_deallocate_resource(linear_kb_t* kb, const char* resource_name, const char* current_point) {
    consumption_metadata_t* metadata = find_consumption_metadata(kb, resource_name);
    if (!metadata) {
        return 0; // No metadata, don't deallocate
    }
    
    if (metadata->is_persistent) {
        return 0; // Persistent resources are never deallocated
    }
    
    // Check if we're at the right consumption point
    return strcmp(metadata->consumption_point, current_point) == 0;
}

// Enhanced resource consumption with compiler metadata integration
int consume_linear_resource_with_metadata(linear_kb_t* kb, term_t* goal, substitution_t* subst, const char* consumption_point) {
    linear_resource_t* current = kb->resources;
    
    while (current != NULL) {
        if (!current->consumed && !current->deallocated) {
            // Try to unify with the goal
            substitution_t temp_subst = {0};
            init_substitution(&temp_subst);
            copy_substitution(&temp_subst, subst);
            
            if (unify_terms(current->fact, goal, &temp_subst)) {
                // Resource matches - consume it
                current->consumed = 1;
                copy_substitution(subst, &temp_subst);
                
                #ifdef DEBUG
                printf("DEBUG: Consumed linear resource: ");
                print_term(current->fact);
                printf(" at point '%s'\n", consumption_point);
                #endif
                
                // Check if compiler metadata says to deallocate here
                char resource_name[256];
                term_to_string_buffer(current->fact, resource_name, sizeof(resource_name));
                
                if (should_deallocate_resource(kb, resource_name, consumption_point)) {
                    free_linear_resource(kb, current);
                }
                
                free_substitution(&temp_subst);
                return 1;  // Successfully consumed
            }
            
            free_substitution(&temp_subst);
        }
        current = current->next;
    }
    
    return 0; // No matching resource found
}

// Helper function to convert term to string (for resource naming)
void term_to_string_buffer(term_t* term, char* buffer, size_t buffer_size) {
    if (!term || !buffer || buffer_size < 2) {
        if (buffer && buffer_size > 0) buffer[0] = '\0';
        return;
    }
    
    switch (term->type) {
        case TERM_ATOM:
            snprintf(buffer, buffer_size, "%s", term->data.atom ? term->data.atom : "null");
            break;
        case TERM_VAR:
            snprintf(buffer, buffer_size, "%s", term->data.var ? term->data.var : "?");
            break;
        case TERM_INTEGER:
            snprintf(buffer, buffer_size, "%lld", term->data.integer);
            break;
        case TERM_COMPOUND:
            if (term->data.compound.arity == 0) {
                snprintf(buffer, buffer_size, "%s", term->data.compound.functor ? term->data.compound.functor : "compound");
            } else {
                snprintf(buffer, buffer_size, "%s/%d", 
                        term->data.compound.functor ? term->data.compound.functor : "compound",
                        term->data.compound.arity);
            }
            break;
        case TERM_CLONE:
            term_to_string_buffer(term->data.cloned, buffer, buffer_size);
            break;
        default:
            snprintf(buffer, buffer_size, "unknown");
            break;
    }
}

// Persistent fact functions
void add_persistent_fact(linear_kb_t* kb, term_t* fact) {
    // Add as a linear resource but mark it as persistent
    linear_resource_t* resource = malloc(sizeof(linear_resource_t));
    resource->fact = fact;
    resource->consumed = 0;
    resource->persistent = 1; // Mark as persistent (non-consumable)
    resource->deallocated = 0;
    resource->memory_size = estimate_term_memory_size(fact);
    resource->allocation_site = "persistent_fact";
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

// Substitution utility functions
void init_substitution(substitution_t* subst) {
    if (subst) {
        subst->count = 0;
    }
}

void free_substitution(substitution_t* subst) {
    if (!subst) return;
    
    for (int i = 0; i < subst->count; i++) {
        free(subst->bindings[i].var);
        free_term(subst->bindings[i].term);
    }
    subst->count = 0;
}

int add_binding(substitution_t* subst, const char* var, term_t* term) {
    if (!subst || subst->count >= MAX_VARS) return 0;
    
    subst->bindings[subst->count].var = malloc(strlen(var) + 1);
    strcpy(subst->bindings[subst->count].var, var);
    subst->bindings[subst->count].term = copy_term(term);
    subst->count++;
    return 1;
}

// Check if a resource is persistent (has persistent-use marker)
int is_persistent_resource(term_t* fact) {
    return fact->type == TERM_CLONE;
}

// Utility functions for type checking
term_t* get_inner_term(term_t* term) {
    if (term->type == TERM_CLONE) {
        return term->data.cloned;
    }
    return term;
}

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

// Free the linear knowledge base
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

// Enhanced query resolution with solutions tracking
int linear_resolve_query_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    if (goal_count == 0) return 1; // Success
    
    // For now, delegate to the basic resolution and just track success/failure
    substitution_t global_subst = {0};
    init_substitution(&global_subst);
    
    int result = linear_resolve_query_with_substitution(kb, goals, goal_count, goals[0], &global_subst);
    
    if (result && solutions) {
        // Add the solution to the list if resolution succeeded
        if (solutions->count < solutions->capacity) {
            enhanced_solution_t* solution = &solutions->solutions[solutions->count];
            solution->binding_count = global_subst.count;
            solution->bindings = malloc(sizeof(variable_binding_t) * global_subst.count);
            
            for (int i = 0; i < global_subst.count; i++) {
                solution->bindings[i].var_name = malloc(strlen(global_subst.bindings[i].var) + 1);
                strcpy(solution->bindings[i].var_name, global_subst.bindings[i].var);
                solution->bindings[i].value = copy_term(global_subst.bindings[i].term);
            }
            solutions->count++;
        }
    }
    
    free_substitution(&global_subst);
    return result;
}

// Disjunctive resolution (placeholder)
int linear_resolve_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path) {
    (void)path; // Unused parameter
    // For now, just try each goal independently and return success if any succeeds
    for (int i = 0; i < goal_count; i++) {
        term_t* single_goal[1] = { goals[i] };
        if (linear_resolve_query(kb, single_goal, 1)) {
            return 1;
        }
    }
    return 0;
}

// Add a rule to the knowledge base
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

// Add a type mapping to the knowledge base
void add_type_mapping(linear_kb_t* kb, const char* term_name, const char* type_name) {
    type_mapping_t* mapping = malloc(sizeof(type_mapping_t));
    mapping->term_name = malloc(strlen(term_name) + 1);
    strcpy(mapping->term_name, term_name);
    mapping->type_name = malloc(strlen(type_name) + 1);
    strcpy(mapping->type_name, type_name);
    mapping->next = kb->type_mappings;
    kb->type_mappings = mapping;
}

// Type-based unification check
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

// Print current memory state for debugging
void print_memory_state(linear_kb_t* kb, const char* context) {
    #ifndef DEBUG
    (void)kb;      // Suppress unused parameter warning
    (void)context; // Suppress unused parameter warning
    #endif
    #ifdef DEBUG
    printf("DEBUG: MEMORY STATE [%s]:\n", context);
    
    size_t total_allocated = 0;
    size_t total_active = 0;
    size_t total_deallocated = 0;
    int active_count = 0;
    int deallocated_count = 0;
    
    linear_resource_t* current = kb->resources;
    while (current != NULL) {
        total_allocated += current->memory_size;
        
        if (current->deallocated) {
            total_deallocated += current->memory_size;
            deallocated_count++;
            printf("  [FREED] ");
        } else {
            total_active += current->memory_size;
            active_count++;
            if (current->consumed) {
                printf("  [CONSUMED] ");
            } else {
                printf("  [ACTIVE] ");
            }
        }
        
        print_term(current->fact);
        printf(" (%zu bytes)\n", current->memory_size);
        
        current = current->next;
    }
    
    printf("  SUMMARY: %d active (%zu bytes), %d deallocated (%zu bytes), total allocated: %zu bytes\n",
           active_count, total_active, deallocated_count, total_deallocated, total_allocated);
    printf("DEBUG: END MEMORY STATE\n\n");
    #endif
}

// Print enhanced solution for debugging
void print_enhanced_solution(enhanced_solution_t* solution) {
    if (!solution) {
        printf("(null solution)");
        return;
    }
    
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
