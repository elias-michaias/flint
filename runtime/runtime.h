#ifndef RUNTIME_H
#define RUNTIME_H

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

// Linear resource - facts that can be consumed
typedef struct linear_resource {
    term_t* fact;
    int consumed;  // 0 = available, 1 = consumed
    int persistent; // 0 = linear (consumable), 1 = persistent (non-consumable)
    struct linear_resource* next;
} linear_resource_t;

typedef struct {
    term_t* head;
    term_t** body;
    int body_size;
    term_t* production;  // Optional production term (NULL if no production)
} clause_t;

typedef struct {
    char* var;
    term_t* term;
} binding_t;

// Type mapping for terms
typedef struct type_mapping {
    char* term_name;      // Name of the term (e.g., "c1")
    char* type_name;      // Type of the term (e.g., "coin")
    struct type_mapping* next;
} type_mapping_t;

// Union hierarchy mapping (variant -> parent type)
typedef struct union_mapping {
    char* variant_type;   // Name of the variant type (e.g., "apple")
    char* parent_type;    // Name of the parent type (e.g., "fruit")
    struct union_mapping* next;
} union_mapping_t;

// Linear path tracking for showing resource consumption chains
typedef struct path_step {
    enum {
        PATH_CONSUME,    // Consumed a resource: "turkey1"
        PATH_RULE_APPLY, // Applied a rule: "eat"
        PATH_PRODUCE     // Produced a resource: "=> satisfied"
    } type;
    char* item_name;           // Name of resource or rule
    char* produced_name;       // For PATH_PRODUCE, what was produced
    struct path_step* next;
} path_step_t;

typedef struct linear_path {
    path_step_t* steps;        // Linked list of path steps
    path_step_t* last_step;    // For easy appending
} linear_path_t;

// Consumed state for backtracking
typedef struct consumed_state {
    linear_resource_t* resource;
    int was_consumed;
    struct consumed_state* next;
} consumed_state_t;

typedef struct {
    binding_t bindings[MAX_VARS];
    int count;
} substitution_t;

// Forward declaration for persistent facts
typedef struct persistent_fact {
    term_t* fact;
    struct persistent_fact* next;
} persistent_fact_t;

// Linear knowledge base
typedef struct {
    linear_resource_t* resources;  // Linear facts
    clause_t* rules;               // Rules (can be reused)
    int rule_count;
    type_mapping_t* type_mappings; // Maps terms to their types
    union_mapping_t* union_mappings; // Maps variant types to parent types
    persistent_fact_t* persistent_facts; // Persistent facts (not consumed)
    int* applied_rules;            // Bitmap tracking which rules have been applied
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
term_t* create_clone(term_t* inner);
int unify(term_t* t1, term_t* t2, substitution_t* subst);
term_t* apply_substitution(term_t* term, substitution_t* subst);
void print_term(term_t* term);
void print_substitution(substitution_t* subst);
int resolve_query(clause_t* clauses, int clause_count, term_t** goals, int goal_count);
int string_equal(const char* s1, const char* s2);
term_t* copy_term(term_t* term);
void free_term(term_t* term);

// Linear path tracking functions
linear_path_t* create_linear_path();
void add_path_consume(linear_path_t* path, const char* resource_name);
void add_path_rule_apply(linear_path_t* path, const char* rule_name);
void add_path_produce(linear_path_t* path, const char* rule_name, const char* produced_name);
void print_linear_path(linear_path_t* path);
void free_linear_path(linear_path_t* path);
linear_path_t* copy_linear_path(linear_path_t* path);

// Linear logic functions
linear_kb_t* create_linear_kb();
void add_linear_fact(linear_kb_t* kb, term_t* fact);
void add_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production);
void add_type_mapping(linear_kb_t* kb, const char* term_name, const char* type_name);
void add_union_mapping(linear_kb_t* kb, const char* variant_type, const char* parent_type);
int is_variant_of(linear_kb_t* kb, const char* variant_type, const char* parent_type);
const char* get_term_type(linear_kb_t* kb, const char* term_name);
int can_unify_with_type(linear_kb_t* kb, term_t* goal, term_t* fact);
int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count);
int linear_resolve_query_with_type(linear_kb_t* kb, term_t** goals, int goal_count, int is_disjunctive);
int linear_resolve_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path);
int build_solution_path_single_goal(linear_kb_t* kb, linear_resource_t* start_resource, term_t** goals, int goal_count, linear_path_t* path);
int linear_resolve_query_with_substitution(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst);
int linear_resolve_query_with_path(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst, linear_path_t* path);
int linear_resolve_forward_chaining(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path);
void compose_substitutions(substitution_t* dest, substitution_t* src);
void free_linear_kb(linear_kb_t* kb);
void reset_consumed_resources(linear_kb_t* kb);
consumed_state_t* save_consumed_state(linear_kb_t* kb);
void restore_consumed_state(consumed_state_t* state);
int consume_resource_and_apply_rules_with_subst(linear_kb_t* kb, linear_resource_t* resource, term_t** goals, int goal_count, int* remaining_goals, int* unsatisfied_count, linear_path_t* path, substitution_t* global_subst);

// Solution list for backtracking
typedef struct solution_list {
    int count;
    int capacity;
    substitution_t* solutions;
} solution_list_t;

// Function declarations for backtracking
solution_list_t* create_solution_list();
void add_solution(solution_list_t* list, substitution_t* solution);
void free_solution_list(solution_list_t* list);
int linear_resolve_query_all_solutions(linear_kb_t* kb, term_t** goals, int goal_count, solution_list_t* solutions);
int linear_resolve_query_with_substitution_backtrack(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                   term_t* original_query, substitution_t* global_subst, 
                                                   solution_list_t* solutions);
int try_rule_with_backtracking(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t* original_query, substitution_t* global_subst, solution_list_t* solutions);
int try_rule_with_backtracking_simple(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                     term_t* original_query, substitution_t* global_subst, solution_list_t* solutions);

// Enhanced solution structures for variable binding support
typedef struct variable_binding {
    char* var_name;
    term_t* value;
} variable_binding_t;

typedef struct enhanced_solution {
    substitution_t substitution;
    variable_binding_t* bindings;
    int binding_count;
} enhanced_solution_t;

typedef struct enhanced_solution_list {
    int count;
    int capacity;
    enhanced_solution_t* solutions;
} enhanced_solution_list_t;

// Enhanced solution functions
enhanced_solution_list_t* create_enhanced_solution_list();
void add_enhanced_solution(enhanced_solution_list_t* list, substitution_t* subst);
void print_enhanced_solution(enhanced_solution_t* solution);
void free_enhanced_solution_list(enhanced_solution_list_t* list);
void add_persistent_fact(linear_kb_t* kb, term_t* fact);
int match_persistent_facts(linear_kb_t* kb, term_t* goal, substitution_t* subst);
int linear_resolve_query_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
int linear_resolve_query_with_substitution_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                  term_t* original_query, substitution_t* global_subst, 
                                                  enhanced_solution_list_t* solutions);
int try_rule_with_backtracking_enhanced(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                       term_t* original_query, substitution_t* global_subst, enhanced_solution_list_t* solutions);

// Helper functions
int has_variables(term_t* term);
int is_persistent_resource(term_t* fact);
term_t* get_inner_term(term_t* term);
int terms_equal(term_t* t1, term_t* t2);
int substitutions_equal(substitution_t* s1, substitution_t* s2);
int fact_exists(linear_kb_t* kb, term_t* fact);
void apply_rule_exhaustively(linear_kb_t* kb, clause_t* rule, int* made_progress);
void apply_rule_combinations(linear_kb_t* kb, clause_t* rule, int body_index, 
                           linear_resource_t** used_resources, 
                           substitution_t* current_subst, int* made_progress);
int can_apply_rule(linear_kb_t* kb, clause_t* rule);
int can_satisfy_body_conditions(linear_kb_t* kb, clause_t* rule, int body_index, 
                               linear_resource_t** used_resources);

#endif // RUNTIME_H
