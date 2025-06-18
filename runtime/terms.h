#ifndef TERMS_H
#define TERMS_H

#include <stdint.h>
#include <stdlib.h>
#include "symbol_table.h"

// Forward declaration for substitution struct (typedef comes from unification.h)
struct substitution;

// Compact term representation using IDs instead of strings
typedef enum {
    TERM_ATOM,      // atom represented by symbol_id_t (2 bytes)
    TERM_VAR,       // variable represented by var_id_t (2 bytes)  
    TERM_COMPOUND,  // compound term with functor ID and args
    TERM_INTEGER,   // 64-bit integer
    TERM_CLONE      // reference to another term
} term_type_t;

// Compact term structure - aggressively optimized for memory
typedef struct term {
    uint8_t type;           // 1 byte
    uint8_t arity;          // 1 byte - moved out of compound union for better packing
    uint16_t padding;       // 2 bytes explicit padding for alignment
    union {
        symbol_id_t atom_id;   // 2 bytes
        var_id_t var_id;       // 2 bytes
        int64_t integer;       // 8 bytes
        symbol_id_t functor_id; // 2 bytes - for compound terms
        struct term* cloned;   // 8 bytes
    } data;
    struct term** args;     // 8 bytes - args array pointer (NULL if not compound)
} term_t;

// Memory usage with better packing:
// - ATOM/VAR: 16 bytes total (due to pointer padding) vs previous 32 bytes
// - COMPOUND: 16 bytes + args vs previous 32+ bytes
// - INTEGER: 16 bytes vs previous 32 bytes

// Pair type for tensor products
typedef struct {
    int64_t first;
    int64_t second;
} pair_t;

// Term creation functions (now take symbol table for efficient storage)
term_t* create_atom(symbol_table_t* table, const char* name);
term_t* create_atom_id(symbol_id_t atom_id);
term_t* create_var(var_id_t var_id);
term_t* create_var_named(symbol_table_t* table, const char* name);
term_t* create_integer(int64_t value);
term_t* create_compound(symbol_table_t* table, const char* functor, term_t** args, int arity);
term_t* create_compound_id(symbol_id_t functor_id, term_t** args, uint8_t arity);
term_t* create_clone(term_t* inner);

// Term manipulation functions
term_t* copy_term(term_t* term);
void free_term(term_t* term);
void print_term(term_t* term, symbol_table_t* symbols);
int terms_equal(term_t* t1, term_t* t2);
int occurs_in_term(var_id_t var_id, term_t* term);
term_t* rename_variables_in_term(term_t* term, int instance_id);
term_t* apply_substitution(term_t* term, struct substitution* subst);
term_t* resolve_variable_chain(struct substitution* subst, var_id_t var_id);
int has_variables(term_t* term);
term_t* get_inner_term(term_t* term);
void term_to_string_buffer(term_t* term, symbol_table_t* symbols, char* buffer, size_t buffer_size);

// Variable extraction functions
void extract_variables_from_term(term_t* term, var_id_t* vars, int* var_count, int max_vars);
void extract_variables_from_goals(term_t** goals, int goal_count, var_id_t* vars, int* var_count, int max_vars);
void free_variable_list(var_id_t* vars, int var_count);

#endif // TERMS_H
