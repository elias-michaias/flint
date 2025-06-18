#include "terms.h"
#include "unification.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Global type metadata (will be populated by generated code)
static const type_metadata_t* TYPE_METADATA = NULL;
static size_t TYPE_COUNT = 0;

// Initialize type system with generated metadata
void init_type_system() {
    // Will be called by generated code to set TYPE_METADATA
}

// Fast type compatibility check
bool types_compatible(type_id_t t1, type_id_t t2) {
    // Exact match - always compatible
    if (t1 == t2) return true;
    
    // If either is distinct, no cross-compatibility
    if (TYPE_IS_DISTINCT(t1) || TYPE_IS_DISTINCT(t2)) return false;
    
    // For now, all non-distinct types of the same base are compatible
    // TODO: Implement full inheritance checking with TYPE_METADATA
    uint32_t idx1 = TYPE_INDEX(t1);
    uint32_t idx2 = TYPE_INDEX(t2);
    
    // Basic compatibility: same base type family
    return (idx1 < TYPE_IDX_USER_START) == (idx2 < TYPE_IDX_USER_START);
}

// Get type name for debugging
const char* type_name(symbol_table_t* symbols, type_id_t type_id) {
    if (!TYPE_METADATA || !symbols) return "unknown";
    
    uint32_t idx = TYPE_INDEX(type_id);
    if (idx >= TYPE_COUNT) return "invalid";
    
    return symbol_to_string(symbols, TYPE_METADATA[idx].name_symbol);
}

// Enhanced term creation functions with type support
term_t* create_typed_atom(symbol_table_t* table, const char* name, type_id_t type_id) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_ATOM;
    term->arity = 0;
    term->type_id = type_id;
    term->data.atom_id = intern_symbol(table, name);
    return term;
}

term_t* create_typed_var(var_id_t var_id, type_id_t type_id) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_VAR;
    term->arity = 0;
    term->type_id = type_id;
    term->data.var_id = var_id;
    return term;
}

term_t* create_typed_compound(symbol_table_t* table, const char* functor, 
                             term_t** args, int arity, type_id_t type_id) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_COMPOUND;
    term->arity = arity;
    term->type_id = type_id;
    term->data.compound.functor_id = intern_symbol(table, functor);
    
    if (arity > 0) {
        term->data.compound.args = malloc(sizeof(term_t*) * arity);
        for (int i = 0; i < arity; i++) {
            term->data.compound.args[i] = args[i];
        }
    } else {
        term->data.compound.args = NULL;
    }
    
    return term;
}

// Legacy compatibility functions (use default types)
term_t* create_atom(symbol_table_t* table, const char* name) {
    return create_typed_atom(table, name, TYPE_ATOM);
}

term_t* create_var(var_id_t var_id) {
    return create_typed_var(var_id, TYPE_VARIABLE);
}

term_t* create_var_named(symbol_table_t* table, const char* name) {
    var_id_t var_id = symbol_table_intern_var(table, name);
    return create_var(var_id);
}

term_t* create_integer(int64_t value) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_INTEGER;
    term->arity = 0;
    term->type_id = TYPE_INTEGER;
    term->data.integer = value;
    return term;
}

term_t* create_compound(symbol_table_t* table, const char* functor, term_t** args, int arity) {
    return create_typed_compound(table, functor, args, arity, TYPE_ATOM);
}

term_t* create_clone(term_t* inner) {
    term_t* term = malloc(sizeof(term_t));
    term->type = TERM_CLONE;
    term->arity = 0;
    term->type_id = inner ? inner->type_id : TYPE_ATOM;
    term->data.cloned = inner;
    return term;
}

// Utility functions
void free_term(term_t* term) {
    if (!term) return;
    
    switch (term->type) {
        case TERM_ATOM:
        case TERM_VAR:
            // These are simple types, just free the structure
            break;
        case TERM_COMPOUND:
            if (term->data.compound.args) {
                for (int i = 0; i < term->arity; i++) {
                    free_term(term->data.compound.args[i]);
                }
                free(term->data.compound.args);
            }
            break;
        case TERM_INTEGER:
            break;
        case TERM_CLONE:
            free_term(term->data.cloned);
            break;
    }
    free(term);
}

void print_term(term_t* term, symbol_table_t* symbols) {
    if (!term) {
        printf("NULL");
        return;
    }
    
    switch (term->type) {
        case TERM_ATOM:
            printf("%s", symbol_to_string(symbols, term->data.atom_id));
            break;
        case TERM_VAR:
            printf("%s", symbol_table_get_var_name(symbols, term->data.var_id));
            break;
        case TERM_COMPOUND:
            printf("%s", symbol_to_string(symbols, term->data.compound.functor_id));
            if (term->arity > 0) {
                printf("(");
                for (int i = 0; i < term->arity; i++) {
                    if (i > 0) printf(", ");
                    print_term(term->data.compound.args[i], symbols);
                }
                printf(")");
            }
            break;
        case TERM_INTEGER:
            printf("%lld", term->data.integer);
            break;
        case TERM_CLONE:
            printf("clone(");
            print_term(term->data.cloned, symbols);
            printf(")");
            break;
    }
}

// Term manipulation functions
term_t* copy_term(term_t* term) {
    if (!term) return NULL;
    
    switch (term->type) {
        case TERM_ATOM: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_ATOM;
            new_term->arity = 0;
            new_term->type_id = term->type_id;
            new_term->data.atom_id = term->data.atom_id;
            return new_term;
        }
        case TERM_VAR: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_VAR;
            new_term->arity = 0;
            new_term->type_id = term->type_id;
            new_term->data.var_id = term->data.var_id;
            return new_term;
        }
        case TERM_INTEGER: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_INTEGER;
            new_term->arity = 0;
            new_term->type_id = term->type_id;
            new_term->data.integer = term->data.integer;
            return new_term;
        }
        case TERM_COMPOUND: {
            term_t* new_term = malloc(sizeof(term_t));
            new_term->type = TERM_COMPOUND;
            new_term->arity = term->arity;
            new_term->type_id = term->type_id;
            new_term->data.compound.functor_id = term->data.compound.functor_id;
            
            if (term->arity > 0 && term->data.compound.args) {
                new_term->data.compound.args = malloc(sizeof(term_t*) * term->arity);
                for (int i = 0; i < term->arity; i++) {
                    new_term->data.compound.args[i] = copy_term(term->data.compound.args[i]);
                }
            } else {
                new_term->data.compound.args = NULL;
            }
            return new_term;
        }
        case TERM_CLONE: {
            term_t* new_term = malloc(sizeof(term_t));  
            new_term->type = TERM_CLONE;
            new_term->arity = 0;
            new_term->type_id = term->type_id;
            new_term->data.cloned = copy_term(term->data.cloned);
            return new_term;
        }
    }
    return NULL;
}

int terms_equal(term_t* t1, term_t* t2) {
    if (!t1 && !t2) return 1;
    if (!t1 || !t2) return 0;
    if (t1->type != t2->type) return 0;
    
    // Check type compatibility for atoms and variables
    if ((t1->type == TERM_ATOM || t1->type == TERM_VAR) && 
        !types_compatible(t1->type_id, t2->type_id)) {
        return 0;
    }
    
    switch (t1->type) {
        case TERM_ATOM:
            return t1->data.atom_id == t2->data.atom_id;
        case TERM_VAR:
            return t1->data.var_id == t2->data.var_id;
        case TERM_INTEGER:
            return t1->data.integer == t2->data.integer;
        case TERM_COMPOUND:
            if (t1->arity != t2->arity) return 0;
            if (t1->data.compound.functor_id != t2->data.compound.functor_id) return 0;
            if (!types_compatible(t1->type_id, t2->type_id)) return 0;
            for (int i = 0; i < t1->arity; i++) {
                if (!terms_equal(t1->data.compound.args[i], t2->data.compound.args[i])) {
                    return 0;
                }
            }
            return 1;
        case TERM_CLONE:
            return terms_equal(t1->data.cloned, t2->data.cloned);
    }
    return 0;
}

int occurs_in_term(var_id_t var_id, term_t* term) {
    // TODO: Implement
    (void)var_id; (void)term;
    return 0;
}

term_t* apply_substitution(term_t* term, struct substitution* subst) {
    if (!term || !subst) return term;
    
    // For now, just return the original term if no substitution is needed
    // This is a simplified implementation
    if (term->type == TERM_VAR) {
        // Look for this variable in the substitution
        for (int i = 0; i < subst->count; i++) {
            if (subst->bindings[i].var_id == term->data.var_id) {
                return copy_term(subst->bindings[i].term);
            }
        }
    }
    
    // For compound terms, apply substitution to arguments
    if (term->type == TERM_COMPOUND && term->arity > 0) {
        term_t* result = malloc(sizeof(term_t));
        result->type = TERM_COMPOUND;
        result->arity = term->arity;
        result->type_id = term->type_id;
        result->data.compound.functor_id = term->data.compound.functor_id;
        result->data.compound.args = malloc(sizeof(term_t*) * term->arity);
        
        for (int i = 0; i < term->arity; i++) {
            result->data.compound.args[i] = apply_substitution(term->data.compound.args[i], subst);
        }
        return result;
    }
    
    // For atoms, integers, etc., just return a copy
    return copy_term(term);
}

term_t* rename_variables_in_term(term_t* term, int instance_id) {
    // TODO: Implement
    (void)term; (void)instance_id;
    return NULL;
}

term_t* resolve_variable_chain(struct substitution* subst, var_id_t var_id) {
    // TODO: Implement
    (void)subst; (void)var_id;
    return NULL;
}

int has_variables(term_t* term) {
    // TODO: Implement
    (void)term;
    return 0;
}

term_t* get_inner_term(term_t* term) {
    if (!term) return NULL;
    
    // For cloned terms, return the inner term
    if (term->type == TERM_CLONE) {
        return term->data.cloned;
    }
    
    // For all other terms, return the term itself
    return term;
}

void term_to_string_buffer(term_t* term, symbol_table_t* symbols, char* buffer, size_t buffer_size) {
    // TODO: Implement
    (void)term; (void)symbols; (void)buffer; (void)buffer_size;
    if (buffer_size > 0) buffer[0] = '\0';
}

void extract_variables_from_term(term_t* term, var_id_t* vars, int* var_count, int max_vars) {
    if (!term || !vars || !var_count || *var_count >= max_vars) return;
    
    switch (term->type) {
        case TERM_VAR:
            // Check if this variable is already in the list
            for (int i = 0; i < *var_count; i++) {
                if (vars[i] == term->data.var_id) {
                    return; // Already added
                }
            }
            // Add new variable
            vars[*var_count] = term->data.var_id;
            (*var_count)++;
            break;
            
        case TERM_COMPOUND:
            if (term->data.compound.args) {
                for (int i = 0; i < term->arity; i++) {
                    extract_variables_from_term(term->data.compound.args[i], vars, var_count, max_vars);
                }
            }
            break;
            
        case TERM_CLONE:
            extract_variables_from_term(term->data.cloned, vars, var_count, max_vars);
            break;
            
        case TERM_ATOM:
        case TERM_INTEGER:
            // These don't contain variables
            break;
    }
}

void extract_variables_from_goals(term_t** goals, int goal_count, var_id_t* vars, int* var_count, int max_vars) {
    // TODO: Implement
    (void)goals; (void)goal_count; (void)vars; (void)var_count; (void)max_vars;
}

void free_variable_list(var_id_t* vars, int var_count) {
    // TODO: Implement
    (void)vars; (void)var_count;
}