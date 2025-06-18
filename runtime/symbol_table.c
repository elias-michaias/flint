#include "symbol_table.h"
#include <string.h>
#include <stdio.h>

static var_id_t var_counter = 1;

symbol_table_t* create_symbol_table() {
    symbol_table_t* table = malloc(sizeof(symbol_table_t));
    table->capacity = 256;
    table->count = SYMBOL_FIRST_USER;
    table->symbols = malloc(sizeof(char*) * table->capacity);
    
    // Initialize variable interning
    table->var_names = malloc(sizeof(char*) * 256);
    table->var_ids = malloc(sizeof(var_id_t) * 256);
    table->var_count = 0;
    table->var_capacity = 256;
    table->next_var_id = 1; // Start from 1 (0 can be reserved)
    
    // Initialize built-in symbols
    table->symbols[SYMBOL_NULL] = strdup("");
    table->symbols[SYMBOL_TRUE] = strdup("true");
    table->symbols[SYMBOL_FALSE] = strdup("false");
    table->symbols[SYMBOL_NIL] = strdup("nil");
    
    return table;
}

void free_symbol_table(symbol_table_t* table) {
    if (!table) return;
    
    // Free symbol strings
    for (symbol_id_t i = 0; i < table->count; i++) {
        free(table->symbols[i]);
    }
    free(table->symbols);
    
    // Free variable names
    for (var_id_t i = 0; i < table->var_count; i++) {
        free(table->var_names[i]);
    }
    free(table->var_names);
    free(table->var_ids);
    
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
    if (!table || !name) return 0;
    
    // Check if variable name is already interned
    for (var_id_t i = 0; i < table->var_count; i++) {
        if (strcmp(table->var_names[i], name) == 0) {
            return table->var_ids[i];
        }
    }
    
    // Add new variable
    if (table->var_count >= table->var_capacity) {
        return 0; // Table full
    }
    
    var_id_t new_var_id = table->next_var_id++;
    table->var_names[table->var_count] = strdup(name);
    table->var_ids[table->var_count] = new_var_id;
    table->var_count++;
    
    return new_var_id;
}

const char* symbol_table_get_var_name(symbol_table_t* table, var_id_t var_id) {
    if (!table) return "unknown";
    
    // Find the variable name by var_id
    for (var_id_t i = 0; i < table->var_count; i++) {
        if (table->var_ids[i] == var_id) {
            return table->var_names[i];
        }
    }
    
    // Fallback to generic name
    static char var_name[32];
    snprintf(var_name, sizeof(var_name), "$var_%d", var_id);
    return var_name;
}

const char* symbol_table_get_string(symbol_table_t* table, symbol_id_t symbol_id) {
    return symbol_to_string(table, symbol_id);
}
