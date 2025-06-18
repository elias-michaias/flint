#ifndef UNIFICATION_H
#define UNIFICATION_H

#include "terms.h"

#define MAX_VARS 50

// Compact binding structure using var IDs instead of strings
typedef struct {
    var_id_t var_id;      // 2 bytes instead of 8-byte pointer + string
    term_t* term;      // 8 bytes (unchanged)
} binding_t;

// Substitution structure - much more compact
typedef struct substitution {
    binding_t bindings[MAX_VARS];  // Now 10 bytes per binding vs ~16+ bytes
    uint8_t count;                 // 1 byte instead of 4
} substitution_t;

// Unification functions
int unify(term_t* t1, term_t* t2, substitution_t* subst);
int unify_terms(term_t* term1, term_t* term2, substitution_t* subst);
int add_binding(substitution_t* subst, var_id_t var, term_t* term);

// Substitution management
void init_substitution(substitution_t* subst);
void free_substitution(substitution_t* subst);
void copy_substitution(substitution_t* dest, substitution_t* src);
void compose_substitutions(substitution_t* dest, substitution_t* src);
void print_substitution(substitution_t* subst, symbol_table_t* symbols);
int substitutions_equal(substitution_t* s1, substitution_t* s2);
int solutions_are_equivalent(substitution_t* s1, substitution_t* s2);
void create_filtered_substitution(substitution_t* full_subst, var_id_t* target_vars, int target_count, substitution_t* filtered_subst);
int all_variables_bound(var_id_t* vars, int var_count, substitution_t* subst);

#endif // UNIFICATION_H
