#ifndef TERMS_H
#define TERMS_H

#include <stdint.h>
#include <stdlib.h>

// Forward declarations
typedef struct substitution substitution_t;

// Term types
typedef enum {
    TERM_ATOM,
    TERM_VAR,
    TERM_COMPOUND,
    TERM_INTEGER,
    TERM_CLONE
} term_type_t;

typedef struct term {
    term_type_t type;
    union {
        char* atom;
        char* var;
        int64_t integer;
        struct {
            char* functor;
            struct term** args;
            int arity;
        } compound;
        struct term* cloned;  // For TERM_CLONE
    } data;
} term_t;

// Pair type for tensor products
typedef struct {
    int64_t first;
    int64_t second;
} pair_t;

// Term creation functions
term_t* create_atom(const char* name);
term_t* create_var(const char* name);
term_t* create_integer(int64_t value);
term_t* create_compound(const char* functor, term_t** args, int arity);
term_t* create_clone(term_t* inner);

// Term manipulation functions
term_t* copy_term(term_t* term);
void free_term(term_t* term);
void print_term(term_t* term);
int terms_equal(term_t* t1, term_t* t2);
int string_equal(const char* s1, const char* s2);
int occurs_in_term(const char* var, term_t* term);
term_t* rename_variables_in_term(term_t* term, int instance_id);
term_t* apply_substitution(term_t* term, substitution_t* subst);
term_t* resolve_variable_chain(substitution_t* subst, const char* var);
int has_variables(term_t* term);
term_t* get_inner_term(term_t* term);
void term_to_string_buffer(term_t* term, char* buffer, size_t buffer_size);

// Variable extraction functions
void extract_variables_from_term(term_t* term, char** vars, int* var_count, int max_vars);
void extract_variables_from_goals(term_t** goals, int goal_count, char** vars, int* var_count, int max_vars);
void free_variable_list(char** vars, int var_count);

#endif // TERMS_H
