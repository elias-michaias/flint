#define AM_IMPLEMENTATION  // Enable amoeba implementation
#include "amoeba.h"
#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// CONSTRAINT STORE MANAGEMENT
// =============================================================================

ConstraintStore* flint_create_constraint_store(void) {
    ConstraintStore* store = flint_alloc(sizeof(ConstraintStore));
    
    // Create amoeba solver
    store->solver = am_newsolver(NULL, NULL);
    if (!store->solver) {
        flint_free(store);
        return NULL;
    }
    
    // Initialize variable arrays
    store->variables = NULL;
    store->var_count = 0;
    store->var_capacity = 0;
    
    // Initialize constraint arrays
    store->constraints = NULL;
    store->constraint_count = 0;
    store->constraint_capacity = 0;
    
    // Enable auto-update for real-time constraint solving
    store->auto_update = true;
    am_autoupdate(store->solver, 1);
    
    return store;
}

void flint_free_constraint_store(ConstraintStore* store) {
    if (!store) return;
    
    // Free all constraint variables
    if (store->variables) {
        for (size_t i = 0; i < store->var_count; i++) {
            if (store->variables[i].name) {
                flint_free(store->variables[i].name);
            }
            if (store->variables[i].amoeba_var) {
                am_delvariable(store->variables[i].amoeba_var);
            }
        }
        flint_free(store->variables);
    }
    
    // Free all constraints
    if (store->constraints) {
        for (size_t i = 0; i < store->constraint_count; i++) {
            if (store->constraints[i].amoeba_constraint) {
                am_delconstraint(store->constraints[i].amoeba_constraint);
            }
            if (store->constraints[i].variables) {
                flint_free(store->constraints[i].variables);
            }
            if (store->constraints[i].description) {
                flint_free(store->constraints[i].description);
            }
        }
        flint_free(store->constraints);
    }
    
    // Free amoeba solver (this also frees all variables and constraints)
    if (store->solver) {
        am_delsolver(store->solver);
    }
    
    flint_free(store);
}

// =============================================================================
// CONSTRAINT VARIABLE MANAGEMENT
// =============================================================================

static FlintConstraintVar* find_constraint_var(ConstraintStore* store, VarId var_id) {
    for (size_t i = 0; i < store->var_count; i++) {
        if (store->variables[i].flint_id == var_id) {
            return &store->variables[i];
        }
    }
    return NULL;
}

FlintConstraintVar* flint_get_or_create_constraint_var(ConstraintStore* store, VarId var_id, const char* name) {
    if (!store) return NULL;
    
    // Check if variable already exists
    FlintConstraintVar* existing = find_constraint_var(store, var_id);
    if (existing) return existing;
    
    // Expand variables array if needed
    if (store->var_count >= store->var_capacity) {
        store->var_capacity = (store->var_capacity == 0) ? 8 : (store->var_capacity * 2);
        store->variables = realloc(store->variables, 
                                 sizeof(FlintConstraintVar) * store->var_capacity);
        if (!store->variables) return NULL;
    }
    
    // Create new amoeba variable
    am_Var* amoeba_var = am_newvariable(store->solver);
    if (!amoeba_var) return NULL;
    
    // Initialize Flint constraint variable
    FlintConstraintVar* var = &store->variables[store->var_count];
    var->flint_id = var_id;
    var->amoeba_var = amoeba_var;
    var->name = name ? strdup(name) : NULL;
    
    store->var_count++;
    
    return var;
}

void flint_suggest_constraint_value(ConstraintStore* store, VarId var_id, double value) {
    if (!store) return;
    
    FlintConstraintVar* var = find_constraint_var(store, var_id);
    if (!var) return;
    
    // Add edit constraint and suggest value
    am_addedit(var->amoeba_var, AM_MEDIUM);
    am_suggest(var->amoeba_var, (am_Num)value);
}

double flint_get_constraint_value(ConstraintStore* store, VarId var_id) {
    if (!store) return 0.0;
    
    FlintConstraintVar* var = find_constraint_var(store, var_id);
    if (!var) return 0.0;
    
    return (double)am_value(var->amoeba_var);
}

// =============================================================================
// CONSTRAINT CREATION
// =============================================================================

FlintConstraint* flint_add_arithmetic_constraint(ConstraintStore* store, 
                                                ArithmeticOp op,
                                                VarId* variables,
                                                size_t var_count,
                                                double constant,
                                                ConstraintStrength strength) {
    if (!store || !variables || var_count == 0) return NULL;
    
    // Expand constraints array if needed
    if (store->constraint_count >= store->constraint_capacity) {
        store->constraint_capacity = (store->constraint_capacity == 0) ? 8 : (store->constraint_capacity * 2);
        store->constraints = realloc(store->constraints,
                                   sizeof(FlintConstraint) * store->constraint_capacity);
        if (!store->constraints) return NULL;
    }
    
    // Create amoeba constraint
    am_Constraint* amoeba_constraint = am_newconstraint(store->solver, (am_Num)strength);
    if (!amoeba_constraint) return NULL;
    
    // Get or create constraint variables
    FlintConstraintVar* constraint_vars[var_count];
    for (size_t i = 0; i < var_count; i++) {
        constraint_vars[i] = flint_get_or_create_constraint_var(store, variables[i], NULL);
        if (!constraint_vars[i]) {
            am_delconstraint(amoeba_constraint);
            return NULL;
        }
    }
    
    // Build constraint based on operation
    int result = AM_OK;
    switch (op) {
        case ARITH_ADD:
            // X + Y = Z  ->  X + Y - Z = 0
            if (var_count >= 3) {
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, 1.0);
                am_setrelation(amoeba_constraint, AM_EQUAL);
                am_addterm(amoeba_constraint, constraint_vars[2]->amoeba_var, 1.0);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_SUB:
            // X - Y = Z  ->  X - Y - Z = 0
            if (var_count >= 3) {
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, -1.0);
                am_setrelation(amoeba_constraint, AM_EQUAL);
                am_addterm(amoeba_constraint, constraint_vars[2]->amoeba_var, 1.0);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_EQUAL:
            // X = Y  ->  X - Y = 0
            if (var_count >= 2) {
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_setrelation(amoeba_constraint, AM_EQUAL);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, 1.0);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_LEQ:
            // X <= Y  ->  X - Y <= 0
            if (var_count >= 2) {
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_setrelation(amoeba_constraint, AM_LESSEQUAL);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, 1.0);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_GEQ:
            // X >= Y  ->  X - Y >= 0
            if (var_count >= 2) {
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_setrelation(amoeba_constraint, AM_GREATEQUAL);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, 1.0);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_MUL:
        case ARITH_DIV:
            // Multiplication and division are non-linear and not directly supported by Cassowary
            // These would require linearization or specialized handling
            printf("Warning: Non-linear constraints (multiply/divide) not yet supported\n");
            am_delconstraint(amoeba_constraint);
            return NULL;
    }
    
    // Add constraint to solver
    result = am_add(amoeba_constraint);
    if (result != AM_OK) {
        printf("Failed to add constraint to solver: %d\n", result);
        am_delconstraint(amoeba_constraint);
        return NULL;
    }
    
    // Initialize Flint constraint
    FlintConstraint* constraint = &store->constraints[store->constraint_count];
    constraint->amoeba_constraint = amoeba_constraint;
    constraint->type = (op == ARITH_EQUAL) ? CONSTRAINT_EQUAL : 
                      (op == ARITH_LEQ) ? CONSTRAINT_LEQ : 
                      (op == ARITH_GEQ) ? CONSTRAINT_GEQ : CONSTRAINT_EQUAL;
    constraint->strength = strength;
    constraint->var_count = var_count;
    constraint->variables = flint_alloc(sizeof(VarId) * var_count);
    memcpy(constraint->variables, variables, sizeof(VarId) * var_count);
    constraint->description = NULL;
    
    store->constraint_count++;
    
    return constraint;
}

// =============================================================================
// UNIFIED CONSTRAINT SOLVING
// =============================================================================

bool flint_solve_constraints(ConstraintStore* store, VarId var_id, Environment* env) {
    if (!store || !store->solver) return true;
    
    // The amoeba solver automatically maintains consistency
    // We just need to handle unification constraints separately
    
    // Handle unification constraints (not managed by amoeba)
    LogicalVar* var = flint_lookup_variable(env, var_id);
    if (!var) return true;
    
    // If this variable is bound, try to suggest its value to the constraint solver
    if (var->binding && var->binding->type == VAL_INTEGER) {
        flint_suggest_constraint_value(store, var_id, (double)var->binding->data.integer);
    } else if (var->binding && var->binding->type == VAL_FLOAT) {
        flint_suggest_constraint_value(store, var_id, var->binding->data.float_val);
    }
    
    // Update all variables in the constraint system
    if (!store->auto_update) {
        am_updatevars(store->solver);
    }
    
    return true;
}

// Legacy functions for compatibility (now redirects to new constraint system)
void flint_add_constraint(ConstraintStore* store, VarId var1, VarId var2, int constraint_type, Value* data) {
    if (!store || constraint_type != CONSTRAINT_EQUAL) return;
    
    // Convert legacy equal constraint to new arithmetic constraint
    VarId variables[] = {var1, var2};
    flint_add_arithmetic_constraint(store, ARITH_EQUAL, variables, 2, 0.0, STRENGTH_REQUIRED);
}

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

FlintConstraint* flint_add_equals_constraint(ConstraintStore* store, VarId var1, VarId var2, ConstraintStrength strength) {
    VarId variables[] = {var1, var2};
    return flint_add_arithmetic_constraint(store, ARITH_EQUAL, variables, 2, 0.0, strength);
}

FlintConstraint* flint_add_addition_constraint(ConstraintStore* store, VarId x, VarId y, VarId sum, ConstraintStrength strength) {
    VarId variables[] = {x, y, sum};
    return flint_add_arithmetic_constraint(store, ARITH_ADD, variables, 3, 0.0, strength);
}

FlintConstraint* flint_add_subtraction_constraint(ConstraintStore* store, VarId x, VarId y, VarId diff, ConstraintStrength strength) {
    VarId variables[] = {x, y, diff};
    return flint_add_arithmetic_constraint(store, ARITH_SUB, variables, 3, 0.0, strength);
}

FlintConstraint* flint_add_inequality_constraint(ConstraintStore* store, VarId var1, VarId var2, bool less_than, ConstraintStrength strength) {
    VarId variables[] = {var1, var2};
    ArithmeticOp op = less_than ? ARITH_LEQ : ARITH_GEQ;
    return flint_add_arithmetic_constraint(store, op, variables, 2, 0.0, strength);
}

void flint_remove_constraint(ConstraintStore* store, FlintConstraint* constraint) {
    if (!store || !constraint || !constraint->amoeba_constraint) return;
    
    // Remove from amoeba solver
    am_remove(constraint->amoeba_constraint);
    
    // Mark as removed (but don't delete from array to maintain indices)
    constraint->amoeba_constraint = NULL;
}

void flint_print_constraint_values(ConstraintStore* store) {
    if (!store) return;
    
    printf("=== Constraint Variable Values ===\n");
    for (size_t i = 0; i < store->var_count; i++) {
        FlintConstraintVar* var = &store->variables[i];
        double value = am_value(var->amoeba_var);
        printf("Var %llu", (unsigned long long)var->flint_id);
        if (var->name) {
            printf(" (%s)", var->name);
        }
        printf(": %.6f\n", value);
    }
    printf("=================================\n");
}
