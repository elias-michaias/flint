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

// Linear resource - facts that can be consumed with enhanced memory management
typedef struct linear_resource {
    term_t* fact;
    int consumed;  // 0 = available, 1 = consumed
    int deallocated; // 0 = in memory, 1 = deallocated (for true linear management)
    int persistent; // 0 = linear (consumable), 1 = persistent (non-consumable)
    size_t memory_size; // Estimated memory usage for this resource
    char* allocation_site; // Debug info: where this was allocated
    struct linear_resource* next;
} linear_resource_t;

typedef struct {
    term_t* head;
    term_t** body;
    int body_size;
    term_t* production;  // Optional production term (NULL if no production)
    int is_recursive;    // 1 if this rule is recursive, 0 otherwise
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

// Define substitution_t before forward collection structures
typedef struct {
    binding_t bindings[MAX_VARS];
    int count;
} substitution_t;

// NEW: Forward collection solution planning structures
#define MAX_RESOURCE_ALLOCATIONS 50
#define MAX_RULE_APPLICATIONS 20

// Represents a specific resource allocation for a goal
typedef struct resource_allocation {
    term_t* goal;                    // The goal this allocation satisfies
    linear_resource_t* resource;     // The resource being allocated
    substitution_t bindings;         // Variable bindings from unification
    int is_valid;                    // Whether this allocation is viable
} resource_allocation_t;

// Represents a rule application plan
typedef struct rule_application_plan {
    clause_t* rule;                  // The rule to apply
    substitution_t bindings;         // Variable bindings for this application
    linear_resource_t** consumed_resources; // Resources this rule will consume
    int consumed_count;              // Number of resources consumed
    term_t* produced_fact;           // Fact this rule will produce
    int execution_order;             // When to execute this rule (0 = first)
    int is_valid;                    // Whether this rule application is viable
} rule_application_plan_t;

// Complete solution plan for a query
typedef struct solution_plan {
    substitution_t final_bindings;   // Final variable bindings
    resource_allocation_t* allocations; // Resource allocations
    int allocation_count;            // Number of allocations
    rule_application_plan_t* rule_applications; // Rule applications
    int rule_application_count;      // Number of rule applications
    int is_valid;                    // Whether this plan is executable
    int estimated_cost;              // Cost estimate for optimization
} solution_plan_t;

// Collection of all possible solution plans
typedef struct solution_plan_collection {
    solution_plan_t* plans;          // Array of solution plans
    int count;                       // Number of plans
    int capacity;                    // Allocated capacity
} solution_plan_collection_t;

// Forward declaration for persistent facts
typedef struct persistent_fact {
    term_t* fact;
    struct persistent_fact* next;
} persistent_fact_t;

// Goal stack for recursion detection
#define MAX_GOAL_STACK_DEPTH 100
#define MAX_RECURSIVE_DEPTH 10
#define MAX_GOAL_CACHE 50
typedef struct goal_stack {
    term_t* goals[MAX_GOAL_STACK_DEPTH];
    int depth;
} goal_stack_t;

// Goal cache for memoization
typedef struct goal_cache {
    term_t* goals[MAX_GOAL_CACHE];
    int results[MAX_GOAL_CACHE];  // 0 = not resolved, 1 = success, -1 = failure
    int count;
} goal_cache_t;

// Linear knowledge base with enhanced memory management
typedef struct {
    linear_resource_t* resources;  // Linear facts
    clause_t* rules;               // Rules (can be reused)
    int rule_count;
    type_mapping_t* type_mappings; // Maps terms to their types
    union_mapping_t* union_mappings; // Maps variant types to parent types
    persistent_fact_t* persistent_facts; // Persistent facts (not consumed)
    int* applied_rules;            // Bitmap tracking which rules have been applied
    
    // Enhanced memory management
    int auto_deallocate;           // Enable automatic deallocation on consumption
    size_t total_memory_allocated; // Track total memory usage
    size_t peak_memory_usage;      // Track peak memory usage
    int checkpoint_count;          // Number of active checkpoints
    void** checkpoints;            // Stack of resource state checkpoints
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
int occurs_in_term(const char* var, term_t* term);
term_t* rename_variables_in_term(term_t* term, int instance_id);
void create_filtered_substitution(substitution_t* full_subst, char** target_vars, int target_count, substitution_t* filtered_subst);
term_t* resolve_variable_chain(substitution_t* subst, const char* var);
int solutions_are_equivalent(substitution_t* s1, substitution_t* s2);
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
void add_recursive_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production);
void add_type_mapping(linear_kb_t* kb, const char* term_name, const char* type_name);
void add_union_mapping(linear_kb_t* kb, const char* variant_type, const char* parent_type);

// Function for adding persistent facts
void add_persistent_fact(linear_kb_t* kb, term_t* fact);

// Goal stack functions for recursion detection
void init_goal_stack(goal_stack_t* stack);
int push_goal(goal_stack_t* stack, term_t* goal);
void pop_goal(goal_stack_t* stack);
int is_goal_in_stack(goal_stack_t* stack, term_t* goal);
int is_goal_pattern_in_stack(goal_stack_t* stack, term_t* goal);
int goals_have_same_pattern(term_t* goal1, term_t* goal2);

// Goal cache functions for memoization
void init_goal_cache(goal_cache_t* cache);
int check_goal_cache(goal_cache_t* cache, term_t* goal);
void add_goal_cache(goal_cache_t* cache, term_t* goal, int result);

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

// NEW: Forward collection function declarations
solution_plan_collection_t* create_solution_plan_collection();
void free_solution_plan_collection(solution_plan_collection_t* collection);
solution_plan_collection_t* generate_all_solution_plans(linear_kb_t* kb, term_t** goals, int goal_count);
int execute_solution_plan(linear_kb_t* kb, solution_plan_t* plan);
resource_allocation_t* find_all_resource_allocations(linear_kb_t* kb, term_t** goals, int goal_count, int* allocation_count);
rule_application_plan_t* plan_all_rule_applications(linear_kb_t* kb, term_t** goals, int goal_count, int* plan_count);
int is_solution_plan_valid(linear_kb_t* kb, solution_plan_t* plan);
linear_kb_t* create_kb_copy(linear_kb_t* kb);

// NEW: Forward collection query resolution (replaces backtracking versions)
int linear_resolve_query_forward(linear_kb_t* kb, term_t** goals, int goal_count);
int linear_resolve_query_enhanced_forward(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
int linear_resolve_query_enhanced_disjunctive_forward(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
void free_enhanced_solution_list(enhanced_solution_list_t* list);
void add_persistent_fact(linear_kb_t* kb, term_t* fact);
int match_persistent_facts(linear_kb_t* kb, term_t* goal, substitution_t* subst);
int linear_resolve_query_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
int linear_resolve_query_enhanced_with_stack(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions, goal_stack_t* stack);
int linear_resolve_query_enhanced_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
int try_rule_with_backtracking_enhanced(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                       term_t** original_goals, int original_goal_count, substitution_t* global_subst, enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack);
int try_rule_body_depth_first(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t** original_goals, int original_goal_count, substitution_t* rule_subst,
                              enhanced_solution_list_t* solutions, int rule_depth, term_t** instantiated_body, goal_stack_t* stack);
int resolve_rule_body_recursive(linear_kb_t* kb, term_t** body_goals, int body_count, int body_index,
                                substitution_t* current_subst, clause_t* rule, term_t** remaining_goals, int remaining_count,
                                term_t** original_goals, int original_goal_count, enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack);

// Enhanced solution comparison
int enhanced_solutions_are_equivalent(enhanced_solution_t* solution, substitution_t* subst);

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

// Variable extraction and binding checking functions
void extract_variables_from_term(term_t* term, char** vars, int* var_count, int max_vars);
void extract_variables_from_goals(term_t** goals, int goal_count, char** vars, int* var_count, int max_vars);
int all_variables_bound(char** vars, int var_count, substitution_t* subst);
void free_variable_list(char** vars, int var_count);

// Enhanced linear memory management functions
void set_auto_deallocation(linear_kb_t* kb, int enabled);
int get_memory_usage_estimate(linear_kb_t* kb);
int create_resource_checkpoint(linear_kb_t* kb);
int rollback_to_checkpoint(linear_kb_t* kb, int checkpoint_id);
void cleanup_consumed_resources(linear_kb_t* kb);
void print_linear_memory_status(linear_kb_t* kb);

// Linear resource lifecycle functions  
linear_resource_t* create_linear_resource_with_tracking(term_t* fact, const char* allocation_site);
void deallocate_linear_resource(linear_resource_t* resource);
int is_resource_deallocated(linear_resource_t* resource);

// Compile-time checking support (for integration with Rust)
typedef struct linearity_violation {
    char* resource_name;
    char* violation_type;  // "unconsumed", "double_use", "use_after_free"
    char* location;
} linearity_violation_t;

int check_resource_linearity(linear_kb_t* kb, linearity_violation_t** violations, int* violation_count);
void free_linearity_violations(linearity_violation_t* violations, int count);

#endif // RUNTIME_H
