#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "query_resolution.h"

// TODO: Extract query resolution functions from runtime.c
// This is a placeholder - implementations will be moved from runtime.c

int resolve_query(clause_t* clauses, int clause_count, term_t** goals, int goal_count) {
    (void)clauses;
    (void)clause_count;
    (void)goals;
    (void)goal_count;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_query(linear_kb_t* kb, term_t** goals, int goal_count) {
    substitution_t global_subst = {0};
    return linear_resolve_query_with_substitution(kb, goals, goal_count, goals[0], &global_subst);
}

int linear_resolve_query_with_type(linear_kb_t* kb, term_t** goals, int goal_count, int is_disjunctive) {
    if (is_disjunctive) {
        linear_path_t* path = create_linear_path();
        int result = linear_resolve_disjunctive(kb, goals, goal_count, path);
        free_linear_path(path);
        return result;
    } else {
        return linear_resolve_query(kb, goals, goal_count);
    }
}

// Linear resolve query with substitution - FIXED for forward chaining
int linear_resolve_query_with_substitution(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst) {
    // This is the core query resolution function
    // It tries to satisfy all goals using available resources
    // For conjunctive queries, KB state changes persist between goals (forward chaining)
    
    #ifdef DEBUG
    printf("DEBUG: Resolving query with %d goals\n", goal_count);
    for (int i = 0; i < goal_count; i++) {
        printf("DEBUG: Goal %d: ", i);
        print_term(goals[i], kb->symbols);
        printf("\n");
    }
    printf("DEBUG: Knowledge base has %d rules and resources:\n", kb->rule_count);
    for (linear_resource_t* r = kb->resources; r != NULL; r = r->next) {
        printf("DEBUG: Resource: ");
        print_term(r->fact, kb->symbols);
        printf(" (consumed: %d)\n", r->consumed);
    }
    for (int i = 0; i < kb->rule_count; i++) {
        printf("DEBUG: Rule %d head: ", i);
        print_term(kb->rules[i].head, kb->symbols);
        printf(" body_size: %d\n", kb->rules[i].body_size);
    }
    #endif
    
    if (goal_count == 0) {
        return 1; // All goals satisfied
    }
    
    // Try to satisfy the first goal
    term_t* current_goal = goals[0];
    
    // Try rules to derive the goal (works for both atoms and compounds)
    for (int rule_idx = 0; rule_idx < kb->rule_count; rule_idx++) {
        clause_t* rule = &kb->rules[rule_idx];
        
        #ifdef DEBUG
        printf("DEBUG: Checking rule %d with head: ", rule_idx);
        print_term(rule->head, kb->symbols);
        printf(" and production: ");
        if (rule->production) {
            print_term(rule->production, kb->symbols);
        } else {
            printf("NULL");
        }
        printf("\n");
        #endif
        
        // Check if this rule can produce the goal we're looking for
        int rule_matches = 0;
        substitution_t rule_subst = {0};  // Move substitution to broader scope
        init_substitution(&rule_subst);
        
        if (rule->head && rule->head->type == TERM_ATOM && current_goal->type == TERM_ATOM &&
            rule->head->data.atom_id == current_goal->data.atom_id) {
            // Direct head match (traditional Prolog-style rule for atoms)
            rule_matches = 1;
            #ifdef DEBUG
            printf("DEBUG: Rule head matches goal directly\n");
            #endif
        } else if (rule->production) {
            // Check if the rule's production matches the goal (linear logic production rule)
            if (unify_terms(rule->production, current_goal, &rule_subst)) {
                rule_matches = 1;
                #ifdef DEBUG
                printf("DEBUG: Rule production matches goal: ");
                print_term(rule->production, kb->symbols);
                printf(" with substitution: ");
                print_substitution(&rule_subst, kb->symbols);
                printf("\n");
                #endif
            } else {
                #ifdef DEBUG
                printf("DEBUG: Rule production does not match goal\n");
                #endif
            }
        }
        
        if (rule_matches) {
                
                #ifdef DEBUG
                printf("DEBUG: Attempting to apply rule '%s'\n", 
                       symbol_table_get_string(kb->symbols, current_goal->data.atom_id));
                #endif
                
                // Check if we can consume all the rule's body requirements
                int can_apply = 1;
                linear_resource_t* consumed_resources[10] = {0};
                int consumed_count = 0;
                
                for (int body_idx = 0; body_idx < rule->body_size; body_idx++) {
                    term_t* body_term = rule->body[body_idx];
                    
                    // Apply the substitution from rule-goal unification to the body term
                    term_t* substituted_body_term = apply_substitution(body_term, &rule_subst);
                    
                    int found = 0;
                    
                    #ifdef DEBUG
                    printf("DEBUG: Looking for resource matching: ");
                    print_term(substituted_body_term, kb->symbols);
                    printf("\n");
                    #endif
                    
                    // Try to find a resource that matches this substituted body term
                    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
                        if (!resource->consumed && !resource->deallocated) {
                            substitution_t match_subst = {0};
                            init_substitution(&match_subst);
                            
                            if (unify_terms(substituted_body_term, resource->fact, &match_subst)) {
                                // Found a matching resource
                                if (resource->persistent == 0) {
                                    // Linear resource: consume and deallocate it
                                    resource->consumed = 1;
                                    consumed_resources[consumed_count++] = resource;
                                    
                                    #ifdef DEBUG
                                    printf("DEBUG: Consuming linear resource: ");
                                    print_term(resource->fact, kb->symbols);
                                    printf("\n");
                                    #endif
                                    
                                    // Deallocate for memory efficiency
                                    auto_deallocate_resource(kb, resource);
                                    
                                } else {
                                    // Persistent resource: use but don't consume
                                    #ifdef DEBUG
                                    printf("DEBUG: Using persistent resource (not consumed): ");
                                    print_term(resource->fact, kb->symbols);
                                    printf("\n");
                                    #endif
                                }
                                
                                found = 1;
                                free_substitution(&match_subst);
                                break;
                            }
                            free_substitution(&match_subst);
                        }
                    }
                    
                    free_term(substituted_body_term);
                    
                    if (!found) {
                        #ifdef DEBUG
                        printf("DEBUG: Cannot find resource for: ");
                        print_term(body_term, kb->symbols);
                        printf("\n");
                        #endif
                        can_apply = 0;
                        break;
                    }
                }
                
                if (can_apply && rule->production) {
                    // Rule can be applied, add its production as a new fact
                    // Apply the substitution to the production before adding it
                    term_t* substituted_production = apply_substitution(rule->production, &rule_subst);
                    linear_resource_t* new_resource = malloc(sizeof(linear_resource_t));
                    new_resource->fact = substituted_production;
                    new_resource->consumed = 0;
                    new_resource->next = kb->resources;
                    kb->resources = new_resource;
                    
                    #ifdef DEBUG
                    printf("DEBUG: Rule applied successfully, produced: ");
                    print_term(new_resource->fact, kb->symbols);
                    printf("\n");
                    #endif
                    
                    // Try to satisfy remaining goals WITHOUT backtracking
                    // This allows forward chaining to work properly
                    if (goal_count > 1) {
                        int result = linear_resolve_query_with_substitution(kb, goals + 1, goal_count - 1, original_query, global_subst);
                        if (result) {
                            return 1; // Success - keep all state changes
                        }
                        // If remaining goals fail, we still don't backtrack individual rule applications
                        // This is the key change for forward chaining
                    } else {
                        return 1; // This was the last goal and rule applied successfully
                    }
                } else if (!can_apply) {
                    // Restore consumed resources for this failed attempt
                    for (int i = 0; i < consumed_count; i++) {
                        consumed_resources[i]->consumed = 0;
                    }
                }
            }
            // Clean up rule substitution before trying next rule
            free_substitution(&rule_subst);
        }
    
    // Check if we have a direct fact that matches this goal
    for (linear_resource_t* resource = kb->resources; resource != NULL; resource = resource->next) {
        if (!resource->consumed) {
            substitution_t temp_subst = {0};
            if (unify(current_goal, resource->fact, &temp_subst)) {
                #ifdef DEBUG
                printf("DEBUG: Found direct fact match: ");
                print_term(resource->fact, kb->symbols);
                printf("\n");
                #endif
                
                // Found a matching fact, consume it and continue with remaining goals
                resource->consumed = 1;
                
                // Compose the substitution
                if (global_subst) {
                    compose_substitutions(global_subst, &temp_subst);
                }
                
                // Try to satisfy remaining goals
                if (goal_count > 1) {
                    int result = linear_resolve_query_with_substitution(kb, goals + 1, goal_count - 1, original_query, global_subst);
                    if (result) {
                        return 1; // Success - keep the consumed state
                    }
                    // Don't backtrack here for forward chaining
                } else {
                    return 1; // This was the last goal and it matched
                }
                
                // Only restore if no remaining goals or they all failed
                resource->consumed = 0;
            }
        }
    }
    
    return 0; // Failed to satisfy all goals
}

int linear_resolve_query_with_path(linear_kb_t* kb, term_t** goals, int goal_count, term_t* original_query, substitution_t* global_subst, linear_path_t* path) {
    (void)path; // Suppress unused parameter warning
    // This is a more advanced version that tracks the path of resource consumption
    // For now, delegate to the simpler version
    return linear_resolve_query_with_substitution(kb, goals, goal_count, original_query, global_subst);
}

int linear_resolve_query_all_solutions(linear_kb_t* kb, term_t** goals, int goal_count, solution_list_t* solutions) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

// Enhanced query resolution with solutions tracking
int linear_resolve_query_enhanced(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    if (goal_count == 0) return 1; // Success
    
    // For now, delegate to the basic resolution and just track success/failure
    substitution_t global_subst = {0};
    init_substitution(&global_subst);
    
    int result = linear_resolve_query_with_substitution(kb, goals, goal_count, goals[0], &global_subst);
    
    if (result && solutions) {
        // Add the solution to the list if resolution succeeded
        if (solutions->count < solutions->capacity) {
            enhanced_solution_t* solution = &solutions->solutions[solutions->count];
            solution->binding_count = global_subst.count;
            solution->bindings = malloc(sizeof(variable_binding_t) * global_subst.count);
            
            for (int i = 0; i < global_subst.count; i++) {
                const char* var_name = symbol_table_get_var_name(kb->symbols, global_subst.bindings[i].var_id);
                solution->bindings[i].var_name = malloc(strlen(var_name) + 1);
                strcpy(solution->bindings[i].var_name, var_name);
                solution->bindings[i].value = copy_term(global_subst.bindings[i].term);
            }
            solutions->count++;
        }
    }
    
    free_substitution(&global_subst);
    return result;
}

int linear_resolve_query_enhanced_with_stack(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions, goal_stack_t* stack) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)solutions;
    (void)stack;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_query_enhanced_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

// Disjunctive resolution (placeholder)
int linear_resolve_disjunctive(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path) {
    (void)path; // Unused parameter
    // For now, just try each goal independently and return success if any succeeds
    for (int i = 0; i < goal_count; i++) {
        term_t* single_goal[1] = { goals[i] };
        if (linear_resolve_query(kb, single_goal, 1)) {
            return 1;
        }
    }
    return 0;
}

int build_solution_path_single_goal(linear_kb_t* kb, linear_resource_t* start_resource, term_t** goals, int goal_count, linear_path_t* path) {
    (void)kb;
    (void)start_resource;
    (void)goals;
    (void)goal_count;
    (void)path;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_forward_chaining(linear_kb_t* kb, term_t** goals, int goal_count, linear_path_t* path) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)path;
    // TODO: Move implementation from runtime.c
    return 0;
}

solution_plan_collection_t* create_solution_plan_collection() {
    // TODO: Move implementation from runtime.c
    return NULL;
}

void free_solution_plan_collection(solution_plan_collection_t* collection) {
    (void)collection;
    // TODO: Move implementation from runtime.c
}

solution_plan_collection_t* generate_all_solution_plans(linear_kb_t* kb, term_t** goals, int goal_count) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    // TODO: Move implementation from runtime.c
    return NULL;
}

int execute_solution_plan(linear_kb_t* kb, solution_plan_t* plan) {
    (void)kb;
    (void)plan;
    // TODO: Move implementation from runtime.c
    return 0;
}

resource_allocation_t* find_all_resource_allocations(linear_kb_t* kb, term_t** goals, int goal_count, int* allocation_count) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)allocation_count;
    // TODO: Move implementation from runtime.c
    return NULL;
}

rule_application_plan_t* plan_all_rule_applications(linear_kb_t* kb, term_t** goals, int goal_count, int* plan_count) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)plan_count;
    // TODO: Move implementation from runtime.c
    return NULL;
}

int is_solution_plan_valid(linear_kb_t* kb, solution_plan_t* plan) {
    (void)kb;
    (void)plan;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_query_forward(linear_kb_t* kb, term_t** goals, int goal_count) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_query_enhanced_forward(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_query_enhanced_disjunctive_forward(linear_kb_t* kb, term_t** goals, int goal_count, enhanced_solution_list_t* solutions) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

int linear_resolve_query_with_substitution_backtrack(linear_kb_t* kb, term_t** goals, int goal_count, 
                                                   term_t* original_query, substitution_t* global_subst, 
                                                   solution_list_t* solutions) {
    (void)kb;
    (void)goals;
    (void)goal_count;
    (void)original_query;
    (void)global_subst;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

int try_rule_with_backtracking(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t* original_query, substitution_t* global_subst, solution_list_t* solutions) {
    (void)kb;
    (void)rule;
    (void)goals;
    (void)goal_count;
    (void)original_query;
    (void)global_subst;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

int try_rule_with_backtracking_simple(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                     term_t* original_query, substitution_t* global_subst, solution_list_t* solutions) {
    (void)kb;
    (void)rule;
    (void)goals;
    (void)goal_count;
    (void)original_query;
    (void)global_subst;
    (void)solutions;
    // TODO: Move implementation from runtime.c
    return 0;
}

int try_rule_with_backtracking_enhanced(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                                       term_t** original_goals, int original_goal_count, substitution_t* global_subst, 
                                       enhanced_solution_list_t* solutions, int rule_depth, goal_stack_t* stack) {
    (void)kb;
    (void)rule;
    (void)goals;
    (void)goal_count;
    (void)original_goals;
    (void)original_goal_count;
    (void)global_subst;
    (void)solutions;
    (void)rule_depth;
    (void)stack;
    // TODO: Move implementation from runtime.c
    return 0;
}

int try_rule_body_depth_first(linear_kb_t* kb, clause_t* rule, term_t** goals, int goal_count,
                              term_t** original_goals, int original_goal_count, substitution_t* rule_subst,
                              enhanced_solution_list_t* solutions, int rule_depth, term_t** instantiated_body, goal_stack_t* stack) {
    (void)kb;
    (void)rule;
    (void)goals;
    (void)goal_count;
    (void)original_goals;
    (void)original_goal_count;
    (void)rule_subst;
    (void)solutions;
    (void)rule_depth;
    (void)instantiated_body;
    (void)stack;
    // TODO: Move implementation from runtime.c
    return 0;
}

int resolve_rule_body_recursive(linear_kb_t* kb, term_t** body_goals, int body_count, int body_index,
                                substitution_t* current_subst, clause_t* rule, term_t** remaining_goals, int remaining_count,
                                term_t** original_goals, int original_goal_count, enhanced_solution_list_t* solutions, 
                                int rule_depth, goal_stack_t* stack) {
    (void)kb;
    (void)body_goals;
    (void)body_count;
    (void)body_index;
    (void)current_subst;
    (void)rule;
    (void)remaining_goals;
    (void)remaining_count;
    (void)original_goals;
    (void)original_goal_count;
    (void)solutions;
    (void)rule_depth;
    (void)stack;
    // TODO: Move implementation from runtime.c
    return 0;
}

int consume_resource_and_apply_rules_with_subst(linear_kb_t* kb, linear_resource_t* resource, term_t** goals, int goal_count, 
                                               int* remaining_goals, int* unsatisfied_count, linear_path_t* path, substitution_t* global_subst) {
    (void)kb;
    (void)resource;
    (void)goals;
    (void)goal_count;
    (void)remaining_goals;
    (void)unsatisfied_count;
    (void)path;
    (void)global_subst;
    // TODO: Move implementation from runtime.c
    return 0;
}
