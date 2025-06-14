#include "linear_runtime.h"
#include <string.h>
#include <stdio.h>

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
    
    printf("Trying to resolve: ");
    print_term(goal);
    printf("\n");
    
    for (int i = 0; i < clause_count; i++) {
        if (clauses[i].body_size == 0) { // Only facts for now
            substitution_t subst = {0};
            
            if (unify(goal, clauses[i].head, &subst)) {
                printf("Unified with fact: ");
                print_term(clauses[i].head);
                printf(" with substitution: ");
                print_substitution(&subst);
                printf("\n");
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
void add_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size) {
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
        kb->rule_count++;
    }
}

// Reset all consumed resources (for new queries)
void reset_consumed_resources(linear_kb_t* kb) {
    linear_resource_t* resource = kb->resources;
    while (resource) {
        resource->consumed = 0;
        resource = resource->next;
    }
}

// Linear resolution - resources are consumed when used
int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count) {
    if (goal_count == 0) return 1; // Success - all goals resolved
    
    term_t* goal = goals[0];
    
    printf("Linear resolving: ");
    print_term(goal);
    printf("\n");
    
    // Try to match against available (unconsumed) linear facts
    linear_resource_t* resource = kb->resources;
    while (resource) {
        if (!resource->consumed) {
            substitution_t subst = {0};
            
            if (unify(goal, resource->fact, &subst)) {
                printf("Unified with linear fact: ");
                print_term(resource->fact);
                printf(" (consuming resource)\n");
                
                // CONSUME the resource (key difference from classical logic)
                resource->consumed = 1;
                
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
                int success = linear_resolve_query(kb, remaining_goals, remaining_count);
                
                if (success) {
                    printf("SUCCESS: Query resolved with linear consumption\n");
                    if (remaining_goals) {
                        for (int i = 0; i < remaining_count; i++) {
                            free_term(remaining_goals[i]);
                        }
                        free(remaining_goals);
                    }
                    return 1;
                } else {
                    // BACKTRACK: restore the resource since this path failed
                    resource->consumed = 0;
                    printf("Backtracking: restored resource\n");
                }
                
                if (remaining_goals) {
                    for (int i = 0; i < remaining_count; i++) {
                        free_term(remaining_goals[i]);
                    }
                    free(remaining_goals);
                }
            }
        }
        resource = resource->next;
    }
    
    // Try to match against rules
    for (int i = 0; i < kb->rule_count; i++) {
        substitution_t subst = {0};
        
        if (unify(goal, kb->rules[i].head, &subst)) {
            printf("Unified with rule head: ");
            print_term(kb->rules[i].head);
            printf("\n");
            
            // Create new goal list: rule body + remaining original goals
            int new_goal_count = kb->rules[i].body_size + (goal_count - 1);
            term_t** new_goals = malloc(sizeof(term_t*) * new_goal_count);
            
            // Add rule body goals (with substitution applied)
            for (int j = 0; j < kb->rules[i].body_size; j++) {
                new_goals[j] = apply_substitution(kb->rules[i].body[j], &subst);
            }
            
            // Add remaining original goals (with substitution applied)
            for (int j = 1; j < goal_count; j++) {
                new_goals[kb->rules[i].body_size + j - 1] = apply_substitution(goals[j], &subst);
            }
            
            // Recursively resolve new goals
            int success = linear_resolve_query(kb, new_goals, new_goal_count);
            
            // Clean up
            for (int j = 0; j < new_goal_count; j++) {
                free_term(new_goals[j]);
            }
            free(new_goals);
            
            if (success) {
                printf("SUCCESS: Query resolved via rule\n");
                return 1;
            }
        }
    }
    
    printf("FAILED: No more options for goal ");
    print_term(goal);
    printf("\n");
    return 0; // No more options
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
    free(kb);
}
