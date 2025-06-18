#ifndef TERMS_NEW_H
#define TERMS_NEW_H

#include <stdint.h>
#include <stdlib.h>
#include "symbol_table.h"

// Forward declaration for substitution struct
struct substitution;

// Distinction between structured terms and simple IDs
// Simple terms are just identifiers (atoms, unbound variables)
// Complex terms need storage for structure (compounds, bound variables)

typedef enum {
    TERM_SIMPLE_ATOM,    // Just a symbol_id_t, no structure needed
    TERM_SIMPLE_VAR,     // Just a var_id_t, no structure needed  
    TERM_COMPOUND,       // Needs structure: functor + args
    TERM_INTEGER,        // Needs structure: 64-bit value
    TERM_CLONE          // Needs structure: reference to another term
} term_type_t;

// For compound terms, args can be either:
// - Simple atoms/vars (stored as IDs directly)
// - Complex terms (stored as term_t* pointers)

typedef enum {
    ARG_ATOM,           // symbol_id_t
    ARG_VAR,            // var_id_t  
    ARG_TERM            // term_t*
} arg_type_t;

// Argument can be either an ID or a pointer to a complex term
typedef struct {
    arg_type_t type;
    union {
        symbol_id_t atom_id;
        var_id_t var_id;
        struct term* term_ptr;
    } data;
} term_arg_t;

// Only complex terms need the full structure
typedef struct term {
    term_type_t type;   // 1 byte
    uint8_t arity;      // 1 byte (for compounds only)
    uint16_t _padding;  // 2 bytes
    union {
        // For compounds: functor + args
        struct {
            symbol_id_t functor_id;  // 2 bytes
            term_arg_t* args;        // 8 bytes -> array of mixed args
        } compound;
        
        // For integers: direct value
        int64_t integer;             // 8 bytes
        
        // For clones: reference
        struct term* cloned;         // 8 bytes
    } data;
} term_t;

// Creation functions
// Simple terms - return IDs directly, no allocation
static inline symbol_id_t create_atom_id(symbol_table_t* table, const char* name) {
    return intern_symbol(table, name);
}

static inline var_id_t create_var_simple(symbol_table_t* table, const char* name) {
    return symbol_table_intern_var(table, name);
}

// Complex terms - return allocated structures
term_t* create_compound_mixed(symbol_table_t* table, const char* functor, 
                             term_arg_t* args, uint8_t arity);
term_t* create_integer(int64_t value);
term_t* create_clone(term_t* inner);

// Utility functions
void free_term(term_t* term);
void print_term_arg(term_arg_t* arg, symbol_table_t* symbols);
void print_term(term_t* term, symbol_table_t* symbols);

// Memory savings:
// - Atoms: 2 bytes (was 24 bytes)  -> 92% reduction  
// - Variables: 2 bytes (was 24 bytes) -> 92% reduction
// - Compounds with atom args: much more efficient

#endif // TERMS_NEW_H
