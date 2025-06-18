#ifndef KNOWLEDGE_BASE_H
#define KNOWLEDGE_BASE_H

#include "terms.h"
#include "unification.h"
#include "symbol_table.h"

// Constants
#define MAX_TERMS 1000
#define MAX_CLAUSES 100
#define MAX_GOAL_STACK_DEPTH 100
#define MAX_RECURSIVE_DEPTH 10
#define MAX_GOAL_CACHE 50

// More compact linear resource representation
typedef struct linear_resource {
    term_t* fact;
    uint8_t consumed;      // 1 bit needed, but 1 byte for alignment
    uint8_t deallocated;   // 1 bit needed, but 1 byte for alignment  
    uint8_t persistent;    // 1 bit needed, but 1 byte for alignment
    uint16_t memory_size;  // 2 bytes instead of 8 (sufficient for most resources)
    symbol_id_t allocation_site; // 2 bytes for interned allocation site string
    struct linear_resource* next; // 8 bytes (unchanged)
} linear_resource_t;

// Total: ~17 bytes vs previous ~32+ bytes

typedef struct {
    term_t* head;
    term_t** body;
    uint8_t body_size;     // 1 byte instead of 4
    term_t* production;    // Optional production term (NULL if no production)
    uint8_t is_recursive;  // 1 byte instead of 4
} clause_t;

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

// Forward declaration for persistent facts
typedef struct persistent_fact {
    term_t* fact;
    struct persistent_fact* next;
} persistent_fact_t;

// Goal stack for recursion detection
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

// Consumption metadata for compiler-directed memory management
typedef struct consumption_metadata {
    char* resource_name;          // Name of resource that gets consumed
    char* consumption_point;      // Point in execution where consumption happens  
    int is_optional;              // 1 if optional resource, 0 if required
    int is_persistent;            // 1 if persistent (no deallocation), 0 if linear
    size_t estimated_size;        // Estimated memory size for this resource
    struct consumption_metadata* next;
} consumption_metadata_t;

// Linear knowledge base with enhanced memory management and symbol table
typedef struct linear_kb {
    symbol_table_t* symbols;       // Central symbol table for efficient string storage
    linear_resource_t* resources;  // Linear facts
    clause_t* rules;               // Rules (can be reused)
    uint16_t rule_count;           // 2 bytes instead of 4
    uint16_t resource_count;       // 2 bytes instead of 4
    type_mapping_t* type_mappings; // Maps terms to their types
    union_mapping_t* union_mappings; // Maps variant types to parent types
    persistent_fact_t* persistent_facts; // Persistent facts (not consumed)
    uint8_t* applied_rules;        // Compact bitmap tracking which rules have been applied
    
    // Enhanced memory management (more compact)
    uint8_t auto_deallocate;       // 1 byte instead of 4
    uint32_t total_memory_allocated; // 4 bytes instead of 8 (sufficient for most uses)
    uint32_t peak_memory_usage;    // 4 bytes instead of 8
    int checkpoint_count;          // Number of active checkpoints
    void** checkpoints;            // Stack of resource state checkpoints
    
    // Compiler-directed memory management metadata
    struct consumption_metadata* consumption_metadata; // Linked list of consumption metadata
} linear_kb_t;

// Consumed state for backtracking
typedef struct consumed_state {
    linear_resource_t* resource;
    int was_consumed;
    struct consumed_state* next;
} consumed_state_t;

// Knowledge base functions
linear_kb_t* create_linear_kb(symbol_table_t* symbols);
void free_linear_kb(linear_kb_t* kb);
linear_kb_t* create_kb_copy(linear_kb_t* kb);

// Fact and rule management
void add_linear_fact(linear_kb_t* kb, term_t* fact);
void add_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production);
void add_recursive_rule(linear_kb_t* kb, term_t* head, term_t** body, int body_size, term_t* production);
void add_persistent_fact(linear_kb_t* kb, term_t* fact);

// Type system functions
void add_type_mapping(linear_kb_t* kb, const char* term_name, const char* type_name);
void add_union_mapping(linear_kb_t* kb, const char* variant_type, const char* parent_type);
int is_variant_of(linear_kb_t* kb, const char* variant_type, const char* parent_type);
const char* get_term_type(linear_kb_t* kb, const char* term_name);
int can_unify_with_type(linear_kb_t* kb, term_t* goal, term_t* fact);

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

// Resource management
void reset_consumed_resources(linear_kb_t* kb);
consumed_state_t* save_consumed_state(linear_kb_t* kb);
void restore_consumed_state(consumed_state_t* state);
int consume_linear_resource_enhanced(linear_kb_t* kb, term_t* goal, substitution_t* subst);
int consume_linear_resource_with_metadata(linear_kb_t* kb, term_t* goal, substitution_t* subst, const char* consumption_point);

// Helper functions
int fact_exists(linear_kb_t* kb, term_t* fact);
int is_persistent_resource(term_t* fact);
int match_persistent_facts(linear_kb_t* kb, term_t* goal, substitution_t* subst);
void apply_rule_exhaustively(linear_kb_t* kb, clause_t* rule, int* made_progress);
void apply_rule_combinations(linear_kb_t* kb, clause_t* rule, int body_index, 
                           linear_resource_t** used_resources, 
                           substitution_t* current_subst, int* made_progress);
int can_apply_rule(linear_kb_t* kb, clause_t* rule);
int can_satisfy_body_conditions(linear_kb_t* kb, clause_t* rule, int body_index, 
                               linear_resource_t** used_resources);

#endif // KNOWLEDGE_BASE_H
