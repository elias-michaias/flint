#include "symbol_table.h"
#include <string.h>
#include <stdio.h>

static var_id_t var_counter = 1;

symbol_table_t* create_symbol_table() {
    symbol_table_t* table = malloc(sizeof(symbol_table_t));
    table->capacity = 256;
    table->count = SYMBOL_FIRST_USER;
    table->symbols = malloc(sizeof(char*) * table->capacity);
    
    // Initialize built-in symbols
    table->symbols[SYMBOL_NULL] = strdup("");
    table->symbols[SYMBOL_TRUE] = strdup("true");
    table->symbols[SYMBOL_FALSE] = strdup("false");
    table->symbols[SYMBOL_NIL] = strdup("nil");
    
    return table;
}

void free_symbol_table(symbol_table_t* table) {
    if (!table) return;
    
    for (symbol_id_t i = 0; i < table->count; i++) {
        free(table->symbols[i]);
    }
    free(table->symbols);
    free(table);
}

symbol_id_t intern_symbol(symbol_table_t* table, const char* str) {
    if (!str) return SYMBOL_NULL;
    
    // Check if symbol already exists
    for (symbol_id_t i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i], str) == 0) {
            return i;
        }
    }
    
    // Add new symbol
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->symbols = realloc(table->symbols, sizeof(char*) * table->capacity);
    }
    
    table->symbols[table->count] = strdup(str);
    return table->count++;
}

const char* symbol_to_string(symbol_table_t* table, symbol_id_t id) {
    if (id >= table->count) return "INVALID_SYMBOL";
    return table->symbols[id];
}

var_id_t create_var_id() {
    return var_counter++;
}

void reset_var_counter() {
    var_counter = 1;
}

// Compatibility functions for the expected API
symbol_id_t symbol_table_intern(symbol_table_t* table, const char* str) {
    return intern_symbol(table, str);
}

var_id_t symbol_table_intern_var(symbol_table_t* table, const char* name) {
    // For simplicity, just return a new var ID
    // In a more sophisticated implementation, we might want to map var names to IDs
    (void)table; // unused parameter
    (void)name;  // unused parameter
    return create_var_id();
}

const char* symbol_table_get_var_name(symbol_table_t* table, var_id_t var_id) {
    // For now, just return a generic variable name
    // In a proper implementation, we'd maintain a mapping
    (void)table;
    static char var_name[32];
    snprintf(var_name, sizeof(var_name), "$var_%d", var_id);
    return var_name;
}

const char* symbol_table_get_string(symbol_table_t* table, symbol_id_t symbol_id) {
    return symbol_to_string(table, symbol_id);
}
