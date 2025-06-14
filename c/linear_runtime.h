#ifndef LINEAR_RUNTIME_H
#define LINEAR_RUNTIME_H

#include <stdint.h>
#include <stdlib.h>

// Linear pointer type for memory management
typedef struct {
    void* ptr;
    size_t size;
} linear_ptr_t;

// Pair type for tensor products
typedef struct {
    int64_t first;
    int64_t second;
} pair_t;

// Logical programming types
#define MAX_TERMS 1000
#define MAX_CLAUSES 100
#define MAX_VARS 50
#define MAX_SOLUTIONS 100

typedef enum {
    TERM_ATOM,
    TERM_VAR,
    TERM_COMPOUND,
    TERM_INTEGER
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
    } data;
} term_t;

// Linear resource - facts that can be consumed
typedef struct linear_resource {
    term_t* fact;
    int consumed;  // 0 = available, 1 = consumed
    struct linear_resource* next;
} linear_resource_t;

typedef struct {
    term_t* head;
    term_t** body;
    int body_size;
} clause_t;

typedef struct {
    char* var;
    term_t* term;
} binding_t;

typedef struct {
    binding_t bindings[MAX_VARS];
    int count;
} substitution_t;

// Linear knowledge base
typedef struct {
    linear_resource_t* resources;  // Linear facts
    clause_t* rules;               // Rules (can be reused)
    int rule_count;
} linear_kb_t;

// Runtime function declarations
linear_ptr_t linear_alloc(size_t size);
void linear_free(linear_ptr_t lptr);
int64_t linear_load(linear_ptr_t lptr);
void linear_store(linear_ptr_t lptr, int64_t value);

// String handling (simplified)
typedef struct {
    char* data;
    size_t length;
} linear_string_t;

linear_string_t linear_string_create(const char* str);
void linear_string_free(linear_string_t str);
linear_string_t linear_string_concat(linear_string_t a, linear_string_t b);

// Logical programming functions
term_t* create_atom(const char* name);
term_t* create_var(const char* name);
term_t* create_integer(int64_t value);
term_t* create_compound(const char* functor, term_t** args, int arity);
int unify(term_t* t1, term_t* t2, substitution_t* subst);
term_t* apply_substitution(term_t* term, substitution_t* subst);
void print_term(term_t* term);
void print_substitution(substitution_t* subst);
int resolve_query(clause_t* clauses, int clause_count, term_t** goals, int goal_count);
int string_equal(const char* s1, const char* s2);
term_t* copy_term(term_t* term);
void free_term(term_t* term);

// Linear logic functions
linear_kb_t* create_linear_kb();
void add_linear_fact(linear_kb_t* kb, term_t* fact);
void add_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size);
int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count);
void free_linear_kb(linear_kb_t* kb);
void reset_consumed_resources(linear_kb_t* kb);

#endif // LINEAR_RUNTIME_H
