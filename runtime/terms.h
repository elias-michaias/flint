#ifndef TERMS_H
#define TERMS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "symbol_table.h"

// Forward declaration for substitution struct
struct substitution;

// Bit-packed type system for efficient subtyping
typedef uint16_t type_id_t;

// Bit layout: [distinct:1][reserved:3][type_index:12]
#define TYPE_DISTINCT_MASK    0x8000  // High bit = distinct
#define TYPE_RESERVED_MASK    0x7000  // Reserved for future use  
#define TYPE_INDEX_MASK       0x0FFF  // 4096 possible types

#define TYPE_IS_DISTINCT(t)   ((t) & TYPE_DISTINCT_MASK)
#define TYPE_INDEX(t)         ((t) & TYPE_INDEX_MASK)
#define MAKE_TYPE_ID(idx, distinct) ((idx) | ((distinct) ? TYPE_DISTINCT_MASK : 0))

// Base type indices (always present)
typedef enum {
    TYPE_IDX_ATOM = 0,
    TYPE_IDX_INTEGER = 1,
    TYPE_IDX_VARIABLE = 2,
    TYPE_IDX_USER_START = 3  // User-defined types start here
} base_type_index_t;

// Base type IDs
#define TYPE_ATOM     MAKE_TYPE_ID(TYPE_IDX_ATOM, false)
#define TYPE_INTEGER  MAKE_TYPE_ID(TYPE_IDX_INTEGER, false)
#define TYPE_VARIABLE MAKE_TYPE_ID(TYPE_IDX_VARIABLE, false)

// Base symbol IDs for built-in types
#define SYM_ATOM      0
#define SYM_INTEGER   1  
#define SYM_VARIABLE  2

// Type metadata for inheritance and compatibility
typedef struct {
    type_id_t parent_type;       // 2 bytes - inheritance parent
    symbol_id_t name_symbol;     // 2 bytes - name for debugging
    uint32_t compatible_mask;    // 4 bytes - quick compatibility check
} type_metadata_t;

// Term types (structural, not logical types)
typedef enum {
    TERM_ATOM,      // atom with type_id
    TERM_VAR,       // variable with type_id
    TERM_COMPOUND,  // compound term with functor type_id
    TERM_INTEGER,   // integer literal
    TERM_CLONE      // reference to another term
} term_type_t;

// Enhanced term structure with type information
typedef struct term {
    uint8_t type;               // 1 byte - term_type_t
    uint8_t arity;              // 1 byte - for compounds
    type_id_t type_id;          // 2 bytes - packed type info
    union {
        symbol_id_t atom_id;    // 2 bytes - for atoms
        var_id_t var_id;        // 2 bytes - for variables  
        struct {
            symbol_id_t functor_id;  // 2 bytes
            struct term** args;      // 8 bytes
        } compound;
        int64_t integer;        // 8 bytes
        struct term* cloned;    // 8 bytes
    } data;                     // 8 bytes
} term_t;                       // Total: 12 bytes

// Type system functions
bool types_compatible(type_id_t t1, type_id_t t2);
const char* type_name(symbol_table_t* symbols, type_id_t type_id);
void init_type_system();

// Enhanced term creation functions with type support
term_t* create_typed_atom(symbol_table_t* table, const char* name, type_id_t type_id);
term_t* create_typed_var(var_id_t var_id, type_id_t type_id);
term_t* create_typed_compound(symbol_table_t* table, const char* functor, 
                             term_t** args, int arity, type_id_t type_id);

// Legacy compatibility functions (use default types)
term_t* create_atom(symbol_table_t* table, const char* name);
term_t* create_var(var_id_t var_id);
term_t* create_var_named(symbol_table_t* table, const char* name);
term_t* create_integer(int64_t value);
term_t* create_compound(symbol_table_t* table, const char* functor, term_t** args, int arity);
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
