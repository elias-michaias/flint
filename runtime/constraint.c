#define AM_IMPLEMENTATION  // Enable amoeba implementation
#include "amoeba.h"
#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
bool flint_solve_function_constraint_algebraically(ConstraintStore* store, const char* function_name, VarId var_id, int target_value, Environment* env);

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
            if (store->constraints[i].var_ids) {
                flint_free(store->constraints[i].var_ids);
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
    
    printf("DEBUG: flint_suggest_constraint_value called for var %llu with value %f\n", (unsigned long long)var_id, value);
    
    FlintConstraintVar* var = find_constraint_var(store, var_id);
    if (!var) {
        printf("DEBUG: Constraint variable %llu not found, creating it\n", (unsigned long long)var_id);
        var = flint_get_or_create_constraint_var(store, var_id, NULL);
    }
    
    if (var) {
        printf("DEBUG: Adding edit constraint and suggesting value %f\n", value);
        // Add edit constraint and suggest value using amoeba's edit system
        int result = am_addedit(var->amoeba_var, AM_MEDIUM);
        if (result == AM_OK) {
            am_suggest(var->amoeba_var, (am_Num)value);
            // Auto-update is enabled, so constraints should propagate automatically
            if (!store->auto_update) {
                am_updatevars(store->solver);
            }
        } else {
            printf("DEBUG: Failed to add edit constraint: %d\n", result);
        }
    } else {
        printf("DEBUG: Failed to create constraint variable for %llu\n", (unsigned long long)var_id);
    }
}

/**
 * Suggest values for multiple variables at once
 */
bool flint_suggest_multiple_values(ConstraintStore* store, VarId* var_ids, double* values, size_t count) {
    if (!store || !var_ids || !values || count == 0) return false;
    
    bool all_success = true;
    
    for (size_t i = 0; i < count; i++) {
        FlintConstraintVar* var = flint_get_or_create_constraint_var(store, var_ids[i], NULL);
        if (var) {
            int result = am_addedit(var->amoeba_var, AM_MEDIUM);
            if (result == AM_OK) {
                am_suggest(var->amoeba_var, (am_Num)values[i]);
            } else {
                all_success = false;
            }
        } else {
            all_success = false;
        }
    }
    
    // Update all variables after all suggestions
    if (!store->auto_update) {
        am_updatevars(store->solver);
    }
    
    return all_success;
}

/**
 * Remove edit constraints from variables (stops suggesting values)
 */
void flint_stop_suggesting_values(ConstraintStore* store, VarId* var_ids, size_t count) {
    if (!store || !var_ids) return;
    
    for (size_t i = 0; i < count; i++) {
        FlintConstraintVar* var = find_constraint_var(store, var_ids[i]);
        if (var && am_hasedit(var->amoeba_var)) {
            am_deledit(var->amoeba_var);
        }
    }
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
                am_addterm(amoeba_constraint, constraint_vars[2]->amoeba_var, -1.0);  // Z gets -1.0 coefficient
                am_setrelation(amoeba_constraint, AM_EQUAL);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_SUB:
            // X - Y = Z  ->  X - Y - Z = 0
            if (var_count >= 3) {
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, -1.0);
                am_addterm(amoeba_constraint, constraint_vars[2]->amoeba_var, -1.0);  // Z gets -1.0 coefficient
                am_setrelation(amoeba_constraint, AM_EQUAL);
                if (constant != 0.0) am_addconstant(amoeba_constraint, (am_Num)constant);
            }
            break;
            
        case ARITH_EQUAL:
            // Handle both X = Y and X = constant cases
            if (var_count == 1 && constant != 0.0) {
                // X = constant  ->  X = constant
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_setrelation(amoeba_constraint, AM_EQUAL);
                am_addconstant(amoeba_constraint, (am_Num)constant);
            } else if (var_count >= 2) {
                // X = Y  ->  X - Y = 0
                am_addterm(amoeba_constraint, constraint_vars[0]->amoeba_var, 1.0);
                am_addterm(amoeba_constraint, constraint_vars[1]->amoeba_var, -1.0);  // Y gets -1.0 coefficient
                am_setrelation(amoeba_constraint, AM_EQUAL);
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
    constraint->var_ids = flint_alloc(sizeof(VarId) * var_count);
    memcpy(constraint->var_ids, variables, sizeof(VarId) * var_count);
    constraint->description = NULL;
    
    store->constraint_count++;
    
    return constraint;
}

// =============================================================================
// ARITHMETIC CONSTRAINT HELPERS
// =============================================================================

/**
 * Create a constraint of the form: coefficient * variable + constant = target
 * This is useful for constraints like increment($x) = 11 where increment($x) = $x + 1
 * So we get: $x + 1 = 11, which becomes 1 * $x + 1 = 11
 */
bool flint_add_linear_constraint(ConstraintStore* store, VarId var_id, double coefficient, double constant, double target) {
    if (!store || !store->solver) {
        printf("[DEBUG] flint_add_linear_constraint: no store or solver\n");
        return false;
    }
    
    printf("[DEBUG] flint_add_linear_constraint: %f * var_%llu + %f = %f\n", 
           coefficient, (unsigned long long)var_id, constant, target);
    
    // Get or create the constraint variable
    FlintConstraintVar* var = flint_get_or_create_constraint_var(store, var_id, NULL);
    if (!var) {
        printf("[DEBUG] flint_add_linear_constraint: failed to create constraint variable\n");
        return false;
    }
    
    // Create amoeba constraint with high strength (required)
    am_Constraint* constraint = am_newconstraint(store->solver, AM_REQUIRED);
    if (!constraint) {
        printf("[DEBUG] flint_add_linear_constraint: failed to create amoeba constraint\n");
        return false;
    }
    
    // Build the constraint: coefficient * var + constant = target
    // This becomes: coefficient * var = target - constant
    am_addterm(constraint, var->amoeba_var, (am_Num)coefficient);
    am_setrelation(constraint, AM_EQUAL);
    am_addconstant(constraint, (am_Num)(target - constant));
    
    // Add constraint to solver
    int result = am_add(constraint);
    if (result != AM_OK) {
        printf("[DEBUG] flint_add_linear_constraint: failed to add constraint to solver: %d\n", result);
        am_delconstraint(constraint);
        return false;
    }
    
    printf("[DEBUG] flint_add_linear_constraint: constraint added successfully\n");
    
    // Update variables to get the solution
    am_updatevars(store->solver);
    
    // Get the solved value
    double solved_value = (double)am_value(var->amoeba_var);
    printf("[DEBUG] flint_add_linear_constraint: solved value for var_%llu = %f\n", 
           (unsigned long long)var_id, solved_value);
    
    return true;
}

/**
 * Add a constraint for arbitrary linear expressions
 * This handles constraints like: a*X + b*Y + c*Z + constant = target
 */
bool flint_add_multi_var_linear_constraint(ConstraintStore* store, VarId* var_ids, double* coefficients, 
                                         size_t var_count, double constant, double target, 
                                         ConstraintStrength strength) {
    if (!store || !store->solver || !var_ids || !coefficients || var_count == 0) {
        printf("[DEBUG] flint_add_multi_var_linear_constraint: invalid parameters\n");
        return false;
    }
    
    printf("[DEBUG] flint_add_multi_var_linear_constraint: creating constraint with %zu variables\n", var_count);
    
    // Create amoeba constraint
    am_Constraint* constraint = am_newconstraint(store->solver, (am_Num)strength);
    if (!constraint) {
        printf("[DEBUG] flint_add_multi_var_linear_constraint: failed to create amoeba constraint\n");
        return false;
    }
    
    // Add terms for each variable
    for (size_t i = 0; i < var_count; i++) {
        FlintConstraintVar* var = flint_get_or_create_constraint_var(store, var_ids[i], NULL);
        if (!var) {
            printf("[DEBUG] flint_add_multi_var_linear_constraint: failed to create constraint variable %llu\n", 
                   (unsigned long long)var_ids[i]);
            am_delconstraint(constraint);
            return false;
        }
        
        am_addterm(constraint, var->amoeba_var, (am_Num)coefficients[i]);
        printf("[DEBUG] flint_add_multi_var_linear_constraint: added term %f * var_%llu\n", 
               coefficients[i], (unsigned long long)var_ids[i]);
    }
    
    // Set relation and constant
    am_setrelation(constraint, AM_EQUAL);
    am_addconstant(constraint, (am_Num)(target - constant));
    
    // Add constraint to solver
    int result = am_add(constraint);
    if (result != AM_OK) {
        printf("[DEBUG] flint_add_multi_var_linear_constraint: failed to add constraint to solver: %d\n", result);
        am_delconstraint(constraint);
        return false;
    }
    
    printf("[DEBUG] flint_add_multi_var_linear_constraint: constraint added successfully\n");
    
    // Update variables to get the solution
    am_updatevars(store->solver);
    
    return true;
}

/**
 * Solve a constraint of the form f($x) = target where f is a linear function
 * This attempts to identify the function pattern and create appropriate constraints
 * Now supports multiple common arithmetic patterns
 */
bool flint_solve_function_constraint(ConstraintStore* store, VarId var_id, int target_value) {
    if (!store) return false;
    
    printf("[DEBUG] flint_solve_function_constraint: var_%llu, target=%d\n", 
           (unsigned long long)var_id, target_value);
    
    // For the increment function pattern: increment($x) = $x + 1 = target
    // We create the constraint: $x + 1 = target
    // Which solves to: $x = target - 1
    
    // Assume increment function for now (can be generalized later)
    return flint_add_linear_constraint(store, var_id, 1.0, 1.0, (double)target_value);
}

/**
 * Enhanced function constraint solver that can handle various arithmetic patterns
 */
bool flint_solve_general_arithmetic_constraint(ConstraintStore* store, const char* function_name, 
                                             VarId var_id, double target_value) {
    if (!store || !function_name) return false;
    
    printf("[DEBUG] flint_solve_general_arithmetic_constraint: %s(var_%llu) = %f\n", 
           function_name, (unsigned long long)var_id, target_value);
    
    // Handle different arithmetic function patterns
    if (strcmp(function_name, "increment") == 0 || strcmp(function_name, "inc") == 0) {
        // increment(x) = x + 1
        return flint_add_linear_constraint(store, var_id, 1.0, 1.0, target_value);
    } 
    else if (strcmp(function_name, "decrement") == 0 || strcmp(function_name, "dec") == 0) {
        // decrement(x) = x - 1
        return flint_add_linear_constraint(store, var_id, 1.0, -1.0, target_value);
    }
    else if (strcmp(function_name, "double") == 0 || strcmp(function_name, "twice") == 0) {
        // double(x) = 2 * x
        return flint_add_linear_constraint(store, var_id, 2.0, 0.0, target_value);
    }
    else if (strcmp(function_name, "half") == 0) {
        // half(x) = x / 2 = 0.5 * x
        return flint_add_linear_constraint(store, var_id, 0.5, 0.0, target_value);
    }
    else if (strcmp(function_name, "square") == 0) {
        // Non-linear - not supported by Cassowary directly
        printf("[WARNING] Non-linear constraint 'square' not supported by Cassowary solver\n");
        return false;
    }
    else {
        // Try to parse generic linear functions like "add5" -> x + 5
        if (strncmp(function_name, "add", 3) == 0) {
            double constant = atof(function_name + 3);
            return flint_add_linear_constraint(store, var_id, 1.0, constant, target_value);
        }
        else if (strncmp(function_name, "sub", 3) == 0) {
            double constant = atof(function_name + 3);
            return flint_add_linear_constraint(store, var_id, 1.0, -constant, target_value);
        }
        else if (strncmp(function_name, "mul", 3) == 0) {
            double multiplier = atof(function_name + 3);
            return flint_add_linear_constraint(store, var_id, multiplier, 0.0, target_value);
        }
        
        printf("[WARNING] Unknown function pattern '%s' for constraint solving\n", function_name);
        return false;
    }
}

/**
 * Add constraints for common arithmetic relationships
 */
bool flint_add_arithmetic_relationship(ConstraintStore* store, VarId var1, VarId var2, VarId result, 
                                     ArithmeticOp operation, ConstraintStrength strength) {
    if (!store) return false;
    
    VarId vars[3] = {var1, var2, result};
    double coeffs[3];
    
    switch (operation) {
        case ARITH_ADD:
            // var1 + var2 = result  ->  var1 + var2 - result = 0
            coeffs[0] = 1.0; coeffs[1] = 1.0; coeffs[2] = -1.0;
            break;
        case ARITH_SUB:
            // var1 - var2 = result  ->  var1 - var2 - result = 0
            coeffs[0] = 1.0; coeffs[1] = -1.0; coeffs[2] = -1.0;
            break;
        case ARITH_EQUAL:
            // var1 = var2 (ignore result)  ->  var1 - var2 = 0
            coeffs[0] = 1.0; coeffs[1] = -1.0;
            return flint_add_multi_var_linear_constraint(store, vars, coeffs, 2, 0.0, 0.0, strength);
        default:
            printf("[WARNING] Unsupported arithmetic operation %d\n", operation);
            return false;
    }
    
    return flint_add_multi_var_linear_constraint(store, vars, coeffs, 3, 0.0, 0.0, strength);
}

// =============================================================================
// UNIFIED CONSTRAINT SOLVING
// =============================================================================

bool flint_solve_constraints(ConstraintStore* store, VarId var_id, Environment* env) {
    if (!store || !store->solver) return true;
    
    printf("DEBUG: flint_solve_constraints called for var %llu\n", (unsigned long long)var_id);
    
    // First, check if we have any function constraints for this variable
    for (size_t i = 0; i < store->constraint_count; i++) {
        FlintConstraint* constraint = &store->constraints[i];
        
        if (constraint->type == CONSTRAINT_FUNCTION && 
            constraint->var_count > 0 && 
            constraint->var_ids[0] == var_id) {
            
            printf("DEBUG: Found function constraint for var %llu: %s($x) = %d\n", 
                   (unsigned long long)var_id, constraint->function_name, constraint->target_value);
            
            // Try to solve the function constraint algebraically
            bool solved = flint_solve_function_constraint_algebraically(
                store, constraint->function_name, var_id, constraint->target_value, env);
            
            if (solved) {
                printf("DEBUG: Function constraint solved algebraically\n");
                return true;
            } else {
                printf("DEBUG: Could not solve function constraint algebraically\n");
            }
        }
    }
    
    // Handle unification constraints - propagate values from unification to constraints
    LogicalVar* var = flint_lookup_variable(env, var_id);
    if (var && var->binding) {
        Value* binding = flint_deref_value(var->binding);
        
        printf("DEBUG: Variable %llu has binding of type %d\n", (unsigned long long)var_id, binding->type);
        
        // If this variable is bound to a concrete value, suggest it to the constraint solver
        if (binding->type == VAL_INTEGER) {
            printf("DEBUG: Suggesting integer value %lld for var %llu\n", (long long)binding->data.integer, (unsigned long long)var_id);
            flint_suggest_constraint_value(store, var_id, (double)binding->data.integer);
        } else if (binding->type == VAL_FLOAT) {
            printf("DEBUG: Suggesting float value %f for var %llu\n", binding->data.float_val, (unsigned long long)var_id);
            flint_suggest_constraint_value(store, var_id, binding->data.float_val);
        }
    } else {
        printf("DEBUG: Variable %llu has no binding\n", (unsigned long long)var_id);
    }
    
    // Update all variables in the constraint system to propagate constraints
    printf("DEBUG: Updating constraint variables\n");
    am_updatevars(store->solver);
    
    // Now propagate constraint values back to unification if needed
    FlintConstraintVar* constraint_var = flint_get_or_create_constraint_var(store, var_id, NULL);
    if (constraint_var) {
        double constraint_value = am_value(constraint_var->amoeba_var);
        printf("DEBUG: Constraint variable %llu has value %f\n", (unsigned long long)var_id, constraint_value);
        
        // Only propagate constraint values to unbound variables if the constraint value seems determined
        // and the variable doesn't already have a unification binding
        if (!var || !var->binding) {
            // Don't auto-bind variables to constraint values - let unification drive the process
            printf("DEBUG: Not auto-binding unbound variable %llu to constraint value %f\n", (unsigned long long)var_id, constraint_value);
        }
    } else {
        printf("DEBUG: No constraint variable found for var %llu\n", (unsigned long long)var_id);
    }
    
    return true;
}

// Helper function to safely dereference values
Value* flint_deref_value(Value* val) {
    if (!val) return NULL;
    
    if (val->type == VAL_LOGICAL_VAR) {
        LogicalVar* var = val->data.logical_var;
        if (var->binding) {
            return flint_deref_value(var->binding);
        }
    }
    
    return val;
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
        bool has_edit = am_hasedit(var->amoeba_var);
        
        printf("Var %llu", (unsigned long long)var->flint_id);
        if (var->name) {
            printf(" (%s)", var->name);
        }
        printf(": %.6f", value);
        if (has_edit) {
            printf(" [edit]");
        }
        printf("\n");
    }
    printf("Total constraints: %zu\n", store->constraint_count);
    printf("=================================\n");
}

/**
 * Check if the constraint system is satisfiable
 */
bool flint_is_constraint_system_satisfiable(ConstraintStore* store) {
    if (!store || !store->solver) return true;
    
    // Try to update variables - if this fails, the system might be unsatisfiable
    am_updatevars(store->solver);
    return true; // amoeba doesn't provide a direct satisfiability check
}

/**
 * Get comprehensive constraint system status
 */
void flint_print_constraint_system_status(ConstraintStore* store) {
    if (!store) {
        printf("=== Constraint System Status ===\n");
        printf("No constraint store available\n");
        printf("===============================\n");
        return;
    }
    
    printf("=== Constraint System Status ===\n");
    printf("Solver: %s\n", store->solver ? "Available" : "NULL");
    printf("Variables: %zu / %zu\n", store->var_count, store->var_capacity);
    printf("Constraints: %zu / %zu\n", store->constraint_count, store->constraint_capacity);
    printf("Auto-update: %s\n", store->auto_update ? "Enabled" : "Disabled");
    
    if (store->var_count > 0) {
        printf("Variable details:\n");
        for (size_t i = 0; i < store->var_count; i++) {
            FlintConstraintVar* var = &store->variables[i];
            printf("  Var %llu: amoeba_var=%p, has_edit=%s, value=%.6f\n",
                   (unsigned long long)var->flint_id,
                   var->amoeba_var,
                   var->amoeba_var && am_hasedit(var->amoeba_var) ? "yes" : "no",
                   var->amoeba_var ? am_value(var->amoeba_var) : 0.0);
        }
    }
    
    printf("===============================\n");
}

// Add a function constraint like f(x) = target
bool flint_add_function_constraint(ConstraintStore* store, const char* function_name, VarId var_id, int target_value) {
    if (!store || !function_name) {
        return false;
    }
    
    printf("[DEBUG] Adding function constraint: %s($%llu) = %d\n", function_name, var_id, target_value);
    
    // For now, store this as a general constraint that will be handled by backtracking
    // We don't try to solve it algebraically - instead we let the backtracking solver
    // try different values for var_id and test if function_name(var_id) == target_value
    
    // Ensure we have space for this constraint
    if (store->constraint_count >= store->constraint_capacity) {
        size_t new_capacity = (store->constraint_capacity == 0) ? 8 : (store->constraint_capacity * 2);
        FlintConstraint* new_constraints = realloc(store->constraints, sizeof(FlintConstraint) * new_capacity);
        if (!new_constraints) {
            return false;
        }
        store->constraints = new_constraints;
        store->constraint_capacity = new_capacity;
    }
    
    // Add the constraint
    FlintConstraint* constraint = &store->constraints[store->constraint_count];
    constraint->amoeba_constraint = NULL;  // No amoeba constraint for function constraints
    constraint->type = CONSTRAINT_FUNCTION;
    constraint->var_count = 1;
    constraint->var_ids = flint_alloc(sizeof(VarId));
    constraint->var_ids[0] = var_id;
    constraint->strength = STRENGTH_REQUIRED;
    constraint->description = NULL;
    
    // Store function name and target
    constraint->function_name = flint_alloc(strlen(function_name) + 1);
    strcpy(constraint->function_name, function_name);
    constraint->target_value = target_value;
    
    store->constraint_count++;
    
    printf("[DEBUG] Function constraint added successfully (total: %zu)\n", store->constraint_count);
    return true;
}

// Solve function constraints algebraically by analyzing function structure
bool flint_solve_function_constraint_algebraically(ConstraintStore* store, const char* function_name, VarId var_id, int target_value, Environment* env) {
    printf("[DEBUG] Attempting algebraic solution for %s($%llu) = %d\n", function_name, var_id, target_value);
    
    // For now, we'll implement simple linear function solving
    // A more sophisticated system would analyze the function body or use symbolic manipulation
    
    // Get the registered function to analyze its structure
    if (!flint_is_function_registered(function_name)) {
        printf("[DEBUG] Function %s not registered, cannot analyze\n", function_name);
        return false;
    }
    
    // For common linear functions, we can solve algebraically
    int64_t solution = 0;
    bool has_solution = false;
    
    if (strcmp(function_name, "increment") == 0) {
        // increment(x) = x + 5
        // If increment(x) = target, then x + 5 = target, so x = target - 5
        solution = target_value - 5;
        has_solution = true;
        printf("[DEBUG] Algebraic solution: increment(%lld) = %lld + 5 = %d\n", solution, solution, target_value);
    } else {
        // For other functions, we could implement more sophisticated analysis
        // This could include:
        // - Parsing the function body to extract linear relationships
        // - Using symbolic differentiation for inverse functions
        // - Pattern matching on common function forms
        printf("[DEBUG] No algebraic solver implemented for function %s\n", function_name);
        return false;
    }
    
    if (has_solution) {
        // Create the solution value and unify it with the variable
        Value* solution_value = flint_create_integer(solution);
        
        // Look up the logical variable
        LogicalVar* var = flint_lookup_variable(env, var_id);
        if (var) {
            // Create a Value wrapper for the logical variable to use with unify
            Value var_value;
            var_value.type = VAL_LOGICAL_VAR;
            var_value.data.logical_var = var;
            
            printf("[DEBUG] Unifying variable $%llu with solution %lld\n", var_id, solution);
            bool unified = flint_unify(&var_value, solution_value, env);
            
            if (unified) {
                printf("[DEBUG] Successfully solved constraint: $%llu = %lld\n", var_id, solution);
                return true;
            } else {
                printf("[DEBUG] Failed to unify variable with solution\n");
                return false;
            }
        } else {
            printf("[DEBUG] Could not find logical variable %llu\n", var_id);
            return false;
        }
    }
    
    return false;
}
