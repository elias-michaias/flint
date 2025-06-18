#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unification.h"
#include "terms.h"
#include "symbol_table.h"

// TODO: Extract unification functions from runtime.c
// This is a placeholder - implementations will be moved from runtime.c

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
        if (!occurs_in_term(term1->data.var_id, term2)) {
            if (subst->count < MAX_VARS) {
                subst->bindings[subst->count].var_id = term1->data.var_id;
                subst->bindings[subst->count].term = copy_term(term2);
                subst->count++;
                result = 1;
            }
        }
    } else if (term2->type == TERM_VAR) {
        // Occurs check: don't bind a variable to a term containing itself
        if (!occurs_in_term(term2->data.var_id, term1)) {
            if (subst->count < MAX_VARS) {
                subst->bindings[subst->count].var_id = term2->data.var_id;
                subst->bindings[subst->count].term = copy_term(term1);
                subst->count++;
                result = 1;
            }
        }
    }
    // Atom unification
    else if (term1->type == TERM_ATOM && term2->type == TERM_ATOM) {
        result = term1->data.atom_id == term2->data.atom_id;
    }
    // Integer unification
    else if (term1->type == TERM_INTEGER && term2->type == TERM_INTEGER) {
        result = term1->data.integer == term2->data.integer;
    }
    // Compound term unification
    else if (term1->type == TERM_COMPOUND && term2->type == TERM_COMPOUND) {
        if (term1->data.functor_id == term2->data.functor_id &&
            term1->arity == term2->arity) {
            result = 1;
            for (int i = 0; i < term1->arity && result; i++) {
                result = unify(term1->args[i], term2->args[i], subst);
            }
        }
    }
    
    free_term(term1);
    free_term(term2);
    return result;
}

int unify_terms(term_t* term1, term_t* term2, substitution_t* subst) {
    if (!term1 || !term2) return 0;
    
    // Extract inner terms from persistent resources (cloned terms)
    term_t* actual_term1 = get_inner_term(term1);
    term_t* actual_term2 = get_inner_term(term2);
    
    if (actual_term1->type == TERM_VAR) {
        // Variable unifies with anything
        return add_binding(subst, actual_term1->data.var_id, actual_term2);
    }
    
    if (actual_term2->type == TERM_VAR) {
        // Variable unifies with anything  
        return add_binding(subst, actual_term2->data.var_id, actual_term1);
    }
    
    if (actual_term1->type == TERM_ATOM && actual_term2->type == TERM_ATOM) {
        return actual_term1->data.atom_id == actual_term2->data.atom_id;
    }
    
    if (actual_term1->type == TERM_INTEGER && actual_term2->type == TERM_INTEGER) {
        return actual_term1->data.integer == actual_term2->data.integer;
    }
    
    if (actual_term1->type == TERM_COMPOUND && actual_term2->type == TERM_COMPOUND) {
        if (actual_term1->data.functor_id != actual_term2->data.functor_id) {
            return 0;
        }
        if (actual_term1->arity != actual_term2->arity) {
            return 0;
        }
        for (int i = 0; i < actual_term1->arity; i++) {
            if (!unify_terms(actual_term1->args[i], actual_term2->args[i], subst)) {
                return 0;
            }
        }
        return 1;
    }
    
    return 0; // Different types don't unify
}

int add_binding(substitution_t* subst, var_id_t var_id, term_t* term) {
    if (!subst || subst->count >= MAX_VARS) return 0;
    
    subst->bindings[subst->count].var_id = var_id;
    subst->bindings[subst->count].term = copy_term(term);
    subst->count++;
    return 1;
}

void copy_substitution(substitution_t* dest, substitution_t* src) {
    if (!dest || !src) return;
    dest->count = src->count;
    for (int i = 0; i < src->count && i < MAX_VARS; i++) {
        dest->bindings[i] = src->bindings[i];
    }
}

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
            if (src->bindings[i].var_id == dest->bindings[j].var_id) {
                found = 1;
                break;
            }
        }
        if (!found && dest->count < MAX_VARS) {
            dest->bindings[dest->count].var_id = src->bindings[i].var_id;
            dest->bindings[dest->count].term = copy_term(src->bindings[i].term);
            dest->count++;
        }
    }
}

void print_substitution(substitution_t* subst, symbol_table_t* symbols) {
    if (!subst || subst->count == 0) {
        printf("{}");
        return;
    }
    
    printf("{");
    for (int i = 0; i < subst->count; i++) {
        if (i > 0) printf(", ");
        printf("%s/", symbol_table_get_var_name(symbols, subst->bindings[subst->count].var_id));
        print_term(subst->bindings[i].term, symbols);
    }
    printf("}");
}

int substitutions_equal(substitution_t* s1, substitution_t* s2) {
    (void)s1;
    (void)s2;
    // TODO: Move implementation from runtime.c
    return 0;
}

int solutions_are_equivalent(substitution_t* s1, substitution_t* s2) {
    (void)s1;
    (void)s2;
    // TODO: Move implementation from runtime.c
    return 0;
}

void create_filtered_substitution(substitution_t* full_subst, var_id_t* target_vars, int target_count, substitution_t* filtered_subst) {
    (void)full_subst;
    (void)target_vars;
    (void)target_count;
    (void)filtered_subst;
    // TODO: Move implementation from runtime.c
}

int all_variables_bound(var_id_t* vars, int var_count, substitution_t* subst) {
    (void)vars;
    (void)var_count;
    (void)subst;
    // TODO: Move implementation from runtime.c
    return 0;
}

void init_substitution(substitution_t* subst) {
    if (subst) {
        subst->count = 0;
    }
}

void free_substitution(substitution_t* subst) {
    if (!subst) return;
    
    for (int i = 0; i < subst->count; i++) {
        // No need to free var_id - it's just an integer
        free_term(subst->bindings[i].term);
    }
    subst->count = 0;
}
