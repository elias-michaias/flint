#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "terms.h"
#include "unification.h"

int string_equal(const char* s1, const char* s2) {
    return strcmp(s1, s2) == 0;
}

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

term_t* rename_variables_in_term(term_t* term, int instance_id) {
    (void)term;
    (void)instance_id;
    // TODO: Move implementation from runtime.c
    return NULL;
}

term_t* resolve_variable_chain(substitution_t* subst, const char* var) {
    (void)subst;
    (void)var;
    // TODO: Move implementation from runtime.c
    return NULL;
}

int has_variables(term_t* term) {
    (void)term;
    // TODO: Move implementation from runtime.c
    return 0;
}

term_t* get_inner_term(term_t* term) {
    if (term->type == TERM_CLONE) {
        return term->data.cloned;
    }
    return term;
}

void term_to_string_buffer(term_t* term, char* buffer, size_t buffer_size) {
    (void)term;
    (void)buffer;
    (void)buffer_size;
    // TODO: Move implementation from runtime.c
}

void extract_variables_from_term(term_t* term, char** vars, int* var_count, int max_vars) {
    (void)term;
    (void)vars;
    (void)var_count;
    (void)max_vars;
    // TODO: Move implementation from runtime.c
}

void extract_variables_from_goals(term_t** goals, int goal_count, char** vars, int* var_count, int max_vars) {
    (void)goals;
    (void)goal_count;
    (void)vars;
    (void)var_count;
    (void)max_vars;
    // TODO: Move implementation from runtime.c
}

void free_variable_list(char** vars, int var_count) {
    (void)vars;
    (void)var_count;
    // TODO: Move implementation from runtime.c
}
