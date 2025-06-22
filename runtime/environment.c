#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// ENVIRONMENT MANAGEMENT
// =============================================================================

Environment* flint_create_environment(Environment* parent) {
    Environment* env = (Environment*)flint_alloc(sizeof(Environment));
    env->variables = NULL;
    env->var_count = 0;
    env->capacity = 0;
    env->parent = parent;
    env->constraint_store = NULL;  // Initialize constraint store to NULL
    env->linear_trail = flint_create_linear_trail();  // Initialize linear trail
    return env;
}

void flint_free_environment(Environment* env) {
    if (!env) return;
    
    // Free the variables array (the variables themselves are managed separately)
    if (env->variables) {
        flint_free(env->variables);
    }
    
    // Free the linear trail
    if (env->linear_trail) {
        flint_free_linear_trail(env->linear_trail);
    }
    
    flint_free(env);
}

void flint_bind_variable(Environment* env, VarId var_id, Value* val) {
    // First, try to find an existing variable with this ID
    for (size_t i = 0; i < env->var_count; i++) {
        if (env->variables[i]->id == var_id) {
            // Update existing binding
            env->variables[i]->binding = val;
            
            // Resume any suspensions waiting on this variable
            flint_resume_suspensions(var_id, env);
            return;
        }
    }
    
    // Variable not found, create a new one
    LogicalVar* var = flint_alloc(sizeof(LogicalVar));
    var->id = var_id;
    var->binding = val;
    var->waiters = NULL;
    var->use_count = 0;
    var->is_consumed = false;
    var->allow_reuse = true;  // Default to allowing reuse
    
    // Expand the variables array if needed
    if (env->var_count >= env->capacity) {
        env->capacity = (env->capacity == 0) ? 8 : (env->capacity * 2);
        env->variables = realloc(env->variables, sizeof(LogicalVar*) * env->capacity);
    }
    
    env->variables[env->var_count] = var;
    env->var_count++;
    
    // Resume suspensions
    flint_resume_suspensions(var_id, env);
}

LogicalVar* flint_lookup_variable(Environment* env, VarId var_id) {
    // Search in current environment
    for (size_t i = 0; i < env->var_count; i++) {
        if (env->variables[i]->id == var_id) {
            return env->variables[i];
        }
    }
    
    // Search in parent environments
    if (env->parent) {
        return flint_lookup_variable(env->parent, var_id);
    }
    
    return NULL;
}

// =============================================================================
// CHOICE POINTS AND BACKTRACKING
// =============================================================================

static Environment* clone_environment(Environment* env) {
    if (!env) return NULL;
    
    Environment* clone = flint_create_environment(clone_environment(env->parent));
    
    // Copy all variables
    if (env->var_count > 0) {
        clone->capacity = env->var_count;
        clone->variables = flint_alloc(sizeof(LogicalVar*) * clone->capacity);
        
        for (size_t i = 0; i < env->var_count; i++) {
            LogicalVar* orig_var = env->variables[i];
            LogicalVar* cloned_var = flint_alloc(sizeof(LogicalVar));
            
            cloned_var->id = orig_var->id;
            cloned_var->binding = orig_var->binding;  // Shallow copy for now
            cloned_var->waiters = orig_var->waiters;  // Shallow copy
            cloned_var->use_count = orig_var->use_count;
            cloned_var->is_consumed = orig_var->is_consumed;
            cloned_var->allow_reuse = orig_var->allow_reuse;
            
            clone->variables[i] = cloned_var;
        }
        
        clone->var_count = env->var_count;
    }
    
    return clone;
}

ChoicePoint* flint_create_choice_point(Environment* env, Value** alternatives, size_t alt_count) {
    ChoicePoint* choice = flint_alloc(sizeof(ChoicePoint));
    
    // Take a snapshot of the current environment
    choice->env_snapshot = clone_environment(env);
    
    // TODO: Clone constraint store
    choice->constraint_snapshot = NULL;
    
    // Copy alternatives
    if (alt_count > 0 && alternatives) {
        choice->alternatives = flint_alloc(sizeof(Value) * alt_count);
        for (size_t i = 0; i < alt_count; i++) {
            choice->alternatives[i] = *alternatives[i];
        }
    } else {
        choice->alternatives = NULL;
    }
    
    choice->alt_count = alt_count;
    choice->current_alt = 0;
    choice->parent = NULL;  // Will be set by caller
    
    return choice;
}

bool flint_backtrack(ChoicePoint** current_choice) {
    if (!current_choice || !*current_choice) {
        return false;  // No choice points to backtrack to
    }
    
    ChoicePoint* choice = *current_choice;
    
    // Try the next alternative
    choice->current_alt++;
    
    if (choice->current_alt < choice->alt_count) {
        // We have another alternative to try
        // Restore the environment state
        // TODO: Implement proper environment restoration
        return true;
    } else {
        // No more alternatives, backtrack to parent choice point
        *current_choice = choice->parent;
        
        // Free this choice point
        flint_free_environment(choice->env_snapshot);
        if (choice->alternatives) {
            flint_free(choice->alternatives);
        }
        flint_free(choice);
        
        // Recursively try backtracking to parent
        return flint_backtrack(current_choice);
    }
}

void flint_commit_choice(ChoicePoint* choice) {
    if (!choice) return;
    
    // Free the choice point (commit to current choice)
    flint_free_environment(choice->env_snapshot);
    if (choice->alternatives) {
        flint_free(choice->alternatives);
    }
    flint_free(choice);
}

// =============================================================================
// CONSTRAINT STORE
// =============================================================================

ConstraintStore* flint_create_constraint_store(void) {
    ConstraintStore* store = flint_alloc(sizeof(ConstraintStore));
    store->constraints = NULL;
    store->constraint_count = 0;
    store->capacity = 0;
    return store;
}

void flint_add_constraint(ConstraintStore* store, VarId var1, VarId var2, int constraint_type, Value* data) {
    if (!store) return;
    
    // Expand constraints array if needed
    if (store->constraint_count >= store->capacity) {
        store->capacity = (store->capacity == 0) ? 8 : (store->capacity * 2);
        store->constraints = realloc(store->constraints, 
                                   sizeof(store->constraints[0]) * store->capacity);
    }
    
    // Add the new constraint
    size_t idx = store->constraint_count;
    store->constraints[idx].var1 = var1;
    store->constraints[idx].var2 = var2;
    store->constraints[idx].type = constraint_type;
    store->constraints[idx].constraint_data = data;
    
    store->constraint_count++;
}

bool flint_solve_constraints(ConstraintStore* store, VarId var_id, Environment* env) {
    if (!store) return true;
    
    // Track which constraints we've already processed to avoid infinite loops
    bool* processed = calloc(store->constraint_count, sizeof(bool));
    bool progress = true;
    int iterations = 0;
    const int MAX_ITERATIONS = 100; // Prevent infinite loops
    
    while (progress && iterations < MAX_ITERATIONS) {
        progress = false;
        iterations++;
        
        for (size_t i = 0; i < store->constraint_count; i++) {
            // Skip already processed constraints
            if (processed[i]) continue;
            
            // Check constraints involving the given variable
            if (store->constraints[i].var1 == var_id || store->constraints[i].var2 == var_id) {
                
                LogicalVar* var1 = flint_lookup_variable(env, store->constraints[i].var1);
                LogicalVar* var2 = flint_lookup_variable(env, store->constraints[i].var2);
                
                if (!var1 || !var2) continue; // Skip if variables don't exist
                
                if (var1->binding && var2->binding) {
                    // Both variables are bound, check the constraint
                    switch (store->constraints[i].type) {
                        case CONSTRAINT_EQUAL:
                        case CONSTRAINT_UNIFY:
                            if (!flint_unify(var1->binding, var2->binding, env)) {
                                free(processed);
                                return false;  // Constraint violation
                            }
                            processed[i] = true; // Mark as processed
                            break;
                            
                        case CONSTRAINT_TYPE:
                            // Type constraint checking would go here
                            processed[i] = true;
                            break;
                    }
                } else if (var1->binding && !var2->binding) {
                    // var1 is bound, var2 is free - try to constrain var2
                    switch (store->constraints[i].type) {
                        case CONSTRAINT_EQUAL:
                        case CONSTRAINT_UNIFY:
                            var2->binding = var1->binding;
                            flint_resume_suspensions(var2->id, env);
                            processed[i] = true;
                            progress = true;
                            break;
                    }
                } else if (var2->binding && !var1->binding) {
                    // var2 is bound, var1 is free - try to constrain var1
                    switch (store->constraints[i].type) {
                        case CONSTRAINT_EQUAL:
                        case CONSTRAINT_UNIFY:
                            var1->binding = var2->binding;
                            flint_resume_suspensions(var1->id, env);
                            processed[i] = true;
                            progress = true;
                            break;
                    }
                }
            }
        }
    }
    
    free(processed);
    
    if (iterations >= MAX_ITERATIONS) {
        printf("Warning: Constraint solving hit iteration limit, possible infinite loop\n");
    }
    
    return true;
}
