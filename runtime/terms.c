#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "terms.h"
#include "unification.h"
#include "symbol_table.h"

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
            return t1->data.atom_id == t2->data.atom_id;
        case TERM_VAR:
            return t1->data.var_id == t2->data.var_id;
        case TERM_COMPOUND:
            if (t1->data.functor_id != t2->data.functor_id) return 0;
            if (t1->arity != t2->arity) return 0;
            for (int i = 0; i < t1->arity; i++) {
                if (!terms_equal(t1->args[i], t2->args[i])) {
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

term_t* create_atom(symbol_table_t* symbols, const char* name) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_ATOM;
    term->data.atom_id = symbol_table_intern(symbols, name);
    return term;
}

term_t* create_var_named(symbol_table_t* symbols, const char* name) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_VAR;
    term->data.var_id = symbol_table_intern_var(symbols, name);
    return term;
}

term_t* create_var(var_id_t var_id) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_VAR;
    term->data.var_id = var_id;
    return term;
}

term_t* create_integer(int64_t value) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_INTEGER;
    term->data.integer = value;
    return term;
}

term_t* create_compound(symbol_table_t* symbols, const char* functor, term_t** args, int arity) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_COMPOUND;
    term->data.functor_id = symbol_table_intern(symbols, functor);
    term->arity = arity;
    
    if (arity > 0) {
        term->args = malloc(sizeof(term_t*) * arity);
        for (int i = 0; i < arity; i++) {
            term->args[i] = args[i];
        }
    } else {
        term->args = NULL;
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
            // No string to free - just the ID
            break;
        case TERM_VAR:
            // No string to free - just the ID
            break;
        case TERM_COMPOUND:
            // No functor string to free - just the ID
            if (term->args) {
                for (int i = 0; i < term->arity; i++) {
                    free_term(term->args[i]);
                }
                free(term->args);
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
        case TERM_ATOM: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_ATOM;
            new_term->data.atom_id = term->data.atom_id;
            return new_term;
        }
        case TERM_VAR: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_VAR;
            new_term->data.var_id = term->data.var_id;
            return new_term;
        }
        case TERM_INTEGER: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_INTEGER;
            new_term->data.integer = term->data.integer;
            return new_term;
        }
        case TERM_COMPOUND: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_COMPOUND;
            new_term->data.functor_id = term->data.functor_id;
            new_term->arity = term->arity;
            
            if (term->arity > 0) {
                new_term->args = malloc(sizeof(term_t*) * term->arity);
                for (int i = 0; i < term->arity; i++) {
                    new_term->args[i] = copy_term(term->args[i]);
                }
            } else {
                new_term->args = NULL;
            }
            return new_term;
        }
        case TERM_CLONE: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_CLONE;
            new_term->data.cloned = copy_term(term->data.cloned);
            return new_term;
        }
    }
    return NULL;
}

term_t* apply_substitution(term_t* term, substitution_t* subst) {
    if (!term || !subst) return copy_term(term);
    
    switch (term->type) {
        case TERM_VAR:
            // Look for this variable in the substitution
            for (int i = 0; i < subst->count; i++) {
                if (term->data.var_id == subst->bindings[subst->count].var_id) {
                    return copy_term(subst->bindings[i].term);
                }
            }
            // Variable not found in substitution, return as-is
            return copy_term(term);
            
        case TERM_COMPOUND:
            // Apply substitution recursively to all arguments
            if (term->arity == 0) {
                return copy_term(term);
            }
            
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_COMPOUND;
            new_term->data.functor_id = term->data.functor_id;
            new_term->arity = term->arity;
            new_term->args = malloc(sizeof(term_t*) * term->arity);
            
            for (int i = 0; i < term->arity; i++) {
                new_term->args[i] = apply_substitution(term->args[i], subst);
            }
            
            return new_term;
            
        case TERM_CLONE: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_CLONE;
            new_term->data.cloned = apply_substitution(term->data.cloned, subst);
            return new_term;
        }
            
        case TERM_ATOM:
        case TERM_INTEGER:
            // These don't contain variables, return as-is
            return copy_term(term);
    }
    
    return copy_term(term);
}

void print_term(term_t* term, symbol_table_t* symbols) {
    if (!term) {
        printf("NULL");
        return;
    }
    
    switch (term->type) {
        case TERM_ATOM:
            printf("%s", symbol_table_get_string(symbols, term->data.atom_id));
            break;
        case TERM_VAR:
            printf("%s", symbol_table_get_var_name(symbols, term->data.var_id));
            break;
        case TERM_INTEGER:
            printf("%lld", term->data.integer);
            break;
        case TERM_COMPOUND:
            printf("%s", symbol_table_get_string(symbols, term->data.functor_id));
            if (term->arity > 0) {
                printf("(");
                for (int i = 0; i < term->arity; i++) {
                    if (i > 0) printf(", ");
                    print_term(term->args[i], symbols);
                }
                printf(")");
            }
            break;
        case TERM_CLONE:
            printf("!");
            print_term(term->data.cloned, symbols);
            break;
    }
}

int occurs_in_term(var_id_t var_id, term_t* term) {
    if (!term) return 0;
    
    switch (term->type) {
        case TERM_VAR:
            return var_id == term->data.var_id;
            
        case TERM_COMPOUND:
            for (int i = 0; i < term->arity; i++) {
                if (occurs_in_term(var_id, term->args[i])) {
                    return 1;
                }
            }
            return 0;
            
        case TERM_CLONE:
            return occurs_in_term(var_id, term->data.cloned);
            
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

term_t* resolve_variable_chain(substitution_t* subst, var_id_t var_id) {
    (void)subst;
    (void)var_id;
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

void term_to_string_buffer(term_t* term, symbol_table_t* symbols, char* buffer, size_t buffer_size) {
    (void)term;
    (void)symbols;
    (void)buffer;
    (void)buffer_size;
    // TODO: Move implementation from runtime.c
}

void extract_variables_from_term(term_t* term, var_id_t* vars, int* var_count, int max_vars) {
    (void)term;
    (void)vars;
    (void)var_count;
    (void)max_vars;
    // TODO: Move implementation from runtime.c
}

void extract_variables_from_goals(term_t** goals, int goal_count, var_id_t* vars, int* var_count, int max_vars) {
    (void)goals;
    (void)goal_count;
    (void)vars;
    (void)var_count;
    (void)max_vars;
    // TODO: Move implementation from runtime.c
}

void free_variable_list(var_id_t* vars, int var_count) {
    (void)vars;
    (void)var_count;
    // TODO: Move implementation from runtime.c
}

term_t* create_atom_id(symbol_id_t atom_id) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_ATOM;
    term->data.atom_id = atom_id;
    return term;
}

term_t* create_compound_id(symbol_id_t functor_id, term_t** args, uint8_t arity) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_COMPOUND;
    term->data.functor_id = functor_id;
    term->arity = arity;
    
    if (arity > 0) {
        term->args = malloc(sizeof(term_t*) * arity);
        for (int i = 0; i < arity; i++) {
            term->args[i] = args[i];
        }
    } else {
        term->args = NULL;
    }
    
    return term;
}
