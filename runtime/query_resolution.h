#ifndef QUERY_RESOLUTION_H
#define QUERY_RESOLUTION_H

#include "knowledge_base.h"
#include "memory.h"
#include "solutions.h"
#include "path_tracking.h"

// Forward collection solution planning structures
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

// Basic query resolution
int resolve_query(clause_t* clauses, int clause_count, term_t** goals, int goal_count);

// Linear query resolution functions
int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count);
int linear_resolve_query_with_type(linear_kb_t* kb, term_t** goals, int goal_count, int is_disjunctive);
int linear_resolve_query_with_substitution(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst);
int linear_resolve_query_with_path(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst, linear_path_t* path);
int linear_resolve_query_all_solutions(linear_kb_t* kb, term_t** goals, int goal_count, solution_list_t* solutions);

// Enhanced query resolution
int linear_resolve_query_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
int linear_resolve_query_enhanced_with_stack(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions, goal_stack_t* stack);
int linear_resolve_query_enhanced_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);

// Disjunctive resolution
int linear_resolve_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path);
int build_solution_path_single_goal(linear_kb_t* kb, linear_resource_t* start_resource, term_t** goals, int goal_count, linear_path_t* path);

// Forward chaining resolution
int linear_resolve_forward_chaining(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path);

// Forward collection functions
solution_plan_collection_t* create_solution_plan_collection();
void free_solution_plan_collection(solution_plan_collection_t* collection);
solution_plan_collection_t* generate_all_solution_plans(linear_kb_t* kb, term_t** goals, int goal_count);
int execute_solution_plan(linear_kb_t* kb, solution_plan_t* plan);
resource_allocation_t* find_all_resource_allocations(linear_kb_t* kb, term_t** goals, int goal_count, int* allocation_count);
rule_application_plan_t* plan_all_rule_applications(linear_kb_t* kb, term_t** goals, int goal_count, int* plan_count);
int is_solution_plan_valid(linear_kb_t* kb, solution_plan_t* plan);

// Forward collection query resolution
int linear_resolve_query_forward(linear_kb_t* kb, term_t** goals, int goal_count);
int linear_resolve_query_enhanced_forward(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);
int linear_resolve_query_enhanced_disjunctive_forward(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions);

// Backtracking functions
int linear_resolve_query_with_substitution_backtrack(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                   term_t* original_query, substitution_t* global_subst, 
                                                   solution_list_t* solutions);
int try_rule_with_backtracking(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t* original_query, substitution_t* global_subst, solution_list_t* solutions);
int try_rule_with_backtracking_simple(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                     term_t* original_query, substitution_t* global_subst, solution_list_t* solutions);
int try_rule_with_backtracking_enhanced(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                       term_t** original_goals, int original_goal_count, substitution_t* global_subst, 
                                       enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack);

// Rule application functions  
int try_rule_body_depth_first(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t** original_goals, int original_goal_count, substitution_t* rule_subst,
                              enhanced_solution_list_t* solutions, int rule_depth, term_t** instantiated_body, goal_stack_t* stack);
int resolve_rule_body_recursive(linear_kb_t* kb, term_t** body_goals, int body_count, int body_index,
                                substitution_t* current_subst, clause_t* rule, term_t** remaining_goals, int remaining_count,
                                term_t** original_goals, int original_goal_count, enhanced_solution_list_t* solutions, 
                                int rule_depth, goal_stack_t* stack);
int consume_resource_and_apply_rules_with_subst(linear_kb_t* kb, linear_resource_t* resource, term_t** goals, int goal_count, 
                                               int* remaining_goals, int* unsatisfied_count, linear_path_t* path, substitution_t* global_subst);

// Utility functions
void copy_substitution(substitution_t* dest, substitution_t* src);
int unify_terms(term_t* term1, term_t* term2, substitution_t* subst);

#endif // QUERY_RESOLUTION_H
