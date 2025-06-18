#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdint.h>
#include <stdlib.h>

// Symbol ID type - much more memory efficient than strings
typedef uint16_t symbol_id_t;
typedef uint16_t var_id_t;

// Special symbol IDs
#define SYMBOL_NULL 0
#define SYMBOL_TRUE 1
#define SYMBOL_FALSE 2
#define SYMBOL_NIL 3
#define SYMBOL_FIRST_USER 4

// Maximum symbols (can be increased if needed)
#define MAX_SYMBOLS 65535
#define MAX_SYMBOL_LENGTH 64

// Symbol table for interning strings to compact IDs
typedef struct {
    char** symbols;           // Array of symbol strings
    symbol_id_t count;        // Number of symbols
    symbol_id_t capacity;     // Capacity of symbols array
} symbol_table_t;

// Global symbol table functions
symbol_table_t* create_symbol_table();
void free_symbol_table(symbol_table_t* table);

// Intern a string to get its symbol ID (creates if doesn't exist)
symbol_id_t intern_symbol(symbol_table_t* table, const char* str);

// Get string from symbol ID
const char* symbol_to_string(symbol_table_t* table, symbol_id_t id);

// Variable ID management (separate from symbols for efficiency)
var_id_t create_var_id();
void reset_var_counter();

// Compatibility functions
symbol_id_t symbol_table_intern(symbol_table_t* table, const char* str);
var_id_t symbol_table_intern_var(symbol_table_t* table, const char* name);
const char* symbol_table_get_var_name(symbol_table_t* table, var_id_t var_id);
const char* symbol_table_get_string(symbol_table_t* table, symbol_id_t symbol_id);

// Predicate signature for efficient compound term storage
typedef struct {
    symbol_id_t functor;
    uint8_t arity;
} predicate_sig_t;

#endif // SYMBOL_TABLE_H
