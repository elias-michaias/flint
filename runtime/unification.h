#ifndef UNIFICATION_H
#define UNIFICATION_H

#include "terms.h"

#define MAX_VARS 50

// Binding structure for variable bindings
typedef struct {
    char* var;
    term_t* term;
} binding_t;

// Substitution structure
typedef struct substitution {
    binding_t bindings[MAX_VARS];
    int count;
} substitution_t;

// Unification functions
int unify(term_t* t1, term_t* t2, substitution_t* subst);
int unify_terms(term_t* term1, term_t* term2, substitution_t* subst);
int add_binding(substitution_t* subst, const char* var, term_t* term);

// Substitution management
void init_substitution(substitution_t* subst);
void free_substitution(substitution_t* subst);
void copy_substitution(substitution_t* dest, substitution_t* src);
void compose_substitutions(substitution_t* dest, substitution_t* src);
void print_substitution(substitution_t* subst);
int substitutions_equal(substitution_t* s1, substitution_t* s2);
int solutions_are_equivalent(substitution_t* s1, substitution_t* s2);
void create_filtered_substitution(substitution_t* full_subst, char** target_vars, int target_count, substitution_t* filtered_subst);
int all_variables_bound(char** vars, int var_count, substitution_t* subst);

#endif // UNIFICATION_H
