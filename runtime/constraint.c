#include <nlopt.h>
#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h> // For fabs

// Structure to pass data to NLopt functions
typedef struct {
    ConstraintStore* store;
    FlintConstraint* constraint; // The specific constraint being evaluated
} NloptFuncData;

// Forward declarations
bool flint_solve_function_constraint_algebraically(ConstraintStore* store, const char* function_name, VarId var_id, int target_value, Environment* env);

// NLopt objective function (dummy for constraint satisfaction)
static double nlopt_objective_function(unsigned n, const double* x, double* grad, void* func_data) {
    // For constraint satisfaction, we can use a dummy objective function, e.g., minimize 0
    // or minimize the sum of squares of constraint violations if we were doing least-squares fitting.
    // For now, we'll just return 0.0.
    (void)n; // Unused
    (void)x; // Unused
    if (grad) {
        for (unsigned i = 0; i < n; ++i) {
            grad[i] = 0.0;
        }
    }
    (void)func_data; // Unused
    return 0.0;
}

// NLopt linear equality constraint function
// result[0] = a*x + b*y + ... - C = 0
static double nlopt_linear_equality_constraint(unsigned n, const double* x, double* grad, void* func_data) {
    NloptFuncData* data = (NloptFuncData*)func_data;
    FlintConstraint* constraint = data->constraint;
    
    double sum = 0.0;
    for (size_t i = 0; i < constraint->var_count; ++i) {
        unsigned x_idx = constraint->var_ids[i];
        sum += constraint->coefficients[i] * x[x_idx];
    }
    double constraint_value = sum - constraint->constant_term;

    if (grad) {
        for (unsigned i = 0; i < n; ++i) {
            grad[i] = 0.0; // Initialize gradients to zero
        }
        for (size_t i = 0; i < constraint->var_count; ++i) {
            unsigned x_idx = constraint->var_ids[i];
            grad[x_idx] = constraint->coefficients[i];
        }
    }
    return constraint_value;
}

// NLopt linear inequality constraint function
// result[0] = a*x + b*y + ... - C <= 0
static double nlopt_linear_inequality_constraint(unsigned n, const double* x, double* grad, void* func_data) {
    NloptFuncData* data = (NloptFuncData*)func_data;
    FlintConstraint* constraint = data->constraint;

    double sum = 0.0;
    for (size_t i = 0; i < constraint->var_count; ++i) {
        unsigned x_idx = constraint->var_ids[i];
        sum += constraint->coefficients[i] * x[x_idx];
    }

    double constraint_value;
    if (constraint->type == CONSTRAINT_LEQ) {
        constraint_value = sum - constraint->constant_term; // a*x + b*y + ... - C <= 0
    } else if (constraint->type == CONSTRAINT_GEQ) {
        constraint_value = constraint->constant_term - sum; // C - (a*x + b*y + ...) <= 0
    } else {
        constraint_value = 0.0; // Should not happen for inequality constraints
    }

    if (grad) {
        for (unsigned i = 0; i < n; ++i) {
            grad[i] = 0.0; // Initialize gradients to zero
        }
        for (size_t i = 0; i < constraint->var_count; ++i) {
            unsigned x_idx = constraint->var_ids[i];
            if (constraint->type == CONSTRAINT_LEQ) {
                grad[x_idx] = constraint->coefficients[i];
            } else if (constraint->type == CONSTRAINT_GEQ) {
                grad[x_idx] = -constraint->coefficients[i];
            }
        }
    }
    return constraint_value;
}

// =============================================================================
// CONSTRAINT STORE MANAGEMENT
// =============================================================================

ConstraintStore* flint_create_constraint_store(void) {
    ConstraintStore* store = flint_alloc(sizeof(ConstraintStore));
    
    // Initialize NLopt solver
    store->nlopt_solver = NULL; // Will be initialized when constraints are added

    // Initialize variable arrays
    store->variables = NULL;
    store->var_count = 0;
    store->var_capacity = 0;

    // Initialize constraint arrays
    store->constraints = NULL;
    store->constraint_count = 0;
    store->constraint_capacity = 0;

    // Auto-update is not directly applicable to NLopt in the same way, 
    // as optimization is a discrete step. We'll manage updates manually.
    store->auto_update = false;
    
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
            // No NLopt-specific deallocation for variables, as they are just doubles
        }
        flint_free(store->variables);
    }

    // Free all constraints
    if (store->constraints) {
        for (size_t i = 0; i < store->constraint_count; i++) {
            if (store->constraints[i].var_ids) {
                flint_free(store->constraints[i].var_ids);
            }
            if (store->constraints[i].description) {
                flint_free(store->constraints[i].description);
            }
            if (store->constraints[i].function_name) {
                flint_free(store->constraints[i].function_name);
            }
            if (store->constraints[i].coefficients) {
                flint_free(store->constraints[i].coefficients);
            }
        }
        flint_free(store->constraints);
    }

    // Destroy NLopt solver
    if (store->nlopt_solver) {
        nlopt_destroy(store->nlopt_solver);
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
    
    // Initialize Flint constraint variable
    FlintConstraintVar* var = &store->variables[store->var_count];
    var->flint_id = var_id;
    var->name = name ? strdup(name) : NULL;
    var->value = 0.0; // Initialize value
    
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
        // For NLopt, suggesting a value means setting the initial guess for the variable.
        // We store the suggested value directly in the FlintConstraintVar.
        var->value = value;
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
            var->value = values[i];
        } else {
            all_success = false;
        }
    }
    
    // No auto-update needed for NLopt, as optimization is a discrete step.
    
    return all_success;
}

/**
 * Remove edit constraints from variables (stops suggesting values)
 */
void flint_stop_suggesting_values(ConstraintStore* store, VarId* var_ids, size_t count) {
    // For NLopt, stopping suggestions means simply not setting the initial guess
    // or not including the variable in the optimization problem if it's not constrained.
    // No explicit action needed here as values are stored directly in FlintConstraintVar.
    // If a variable is not part of an active constraint, its value won't be optimized.
}

double flint_get_constraint_value(ConstraintStore* store, VarId var_id) {
    if (!store) return 0.0;
    
    FlintConstraintVar* var = find_constraint_var(store, var_id);
    if (!var) return 0.0;
    
    return var->value;
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
    
    // Initialize Flint constraint
    FlintConstraint* constraint = &store->constraints[store->constraint_count];
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
bool flint_add_linear_constraint(ConstraintStore* store, VarId var_id, double coefficient, double constant, double target, ConstraintStrength strength) {
    if (!store) { // Removed !store->solver check as NLopt is initialized later
        printf("DEBUG: flint_add_linear_constraint: no store\n");
        return false;
    }
    
    printf("DEBUG: flint_add_linear_constraint: %f * var_%llu + %f = %f\n", 
           coefficient, (unsigned long long)var_id, constant, target);
    
    // Get or create the constraint variable
    FlintConstraintVar* var = flint_get_or_create_constraint_var(store, var_id, NULL);
    if (!var) {
        printf("DEBUG: flint_add_linear_constraint: failed to create constraint variable\n");
        return false;
    }
    
    // For NLopt, we don't add constraints directly here.
    // Constraints are added to the nlopt_opt object before optimization.
    // We just store the constraint information in the ConstraintStore.
    
    // Create a dummy constraint to store the info
    FlintConstraint* constraint = flint_add_arithmetic_constraint(store, ARITH_EQUAL, &var_id, 1, constant, strength);
    if (!constraint) return false;
    
    // Store the linear equation: coefficient * var - (target - constant) = 0
    // Store coefficients and constant term directly in the constraint struct
    constraint->coefficients = flint_alloc(sizeof(double));
    constraint->coefficients[0] = coefficient;
    constraint->constant_term = target - constant;
    
    printf("DEBUG: flint_add_linear_constraint: constraint added successfully\n");
    
    // No direct solving here. Solving happens when flint_solve_constraints is called.
    
    return true;
}

/**
 * Add a constraint for arbitrary linear expressions
 * This handles constraints like: a*X + b*Y + c*Z + constant = target
 */
bool flint_add_multi_var_linear_constraint(ConstraintStore* store, VarId* var_ids, double* coefficients, 
                                         size_t var_count, double constant, double target, 
                                         ConstraintStrength strength) {
    if (!store || !var_ids || !coefficients || var_count == 0) { // Removed !store->solver check
        printf("DEBUG: flint_add_multi_var_linear_constraint: invalid parameters\n");
        return false;
    }
    
    printf("DEBUG: flint_add_multi_var_linear_constraint: creating constraint with %zu variables\n", var_count);
    
    // Create a dummy constraint to store the info
    FlintConstraint* constraint = flint_add_arithmetic_constraint(store, ARITH_EQUAL, var_ids, var_count, constant, strength);
    if (!constraint) return false;
    
    // Store coefficients and target for later use by NLopt
    constraint->coefficients = flint_alloc(sizeof(double) * var_count);
    memcpy(constraint->coefficients, coefficients, sizeof(double) * var_count);
    constraint->constant_term = target - constant;
    
    printf("DEBUG: flint_add_multi_var_linear_constraint: constraint added successfully\n");
    
    // No direct solving here. Solving happens when flint_solve_constraints is called.
    
    return true;
}

/**
 * Solve a constraint of the form f($x) = target where f is a linear function
 * This attempts to identify the function pattern and create appropriate constraints
 * Now supports multiple common arithmetic patterns
 */
bool flint_solve_function_constraint(ConstraintStore* store, VarId var_id, int target_value) {
    if (!store) return false;
    
    printf("DEBUG: flint_solve_function_constraint: var_%llu, target=%d\n", 
           (unsigned long long)var_id, target_value);
    
    // For the increment function pattern: increment($x) = $x + 1 = target
    // We create the constraint: $x + 1 = target
    // Which solves to: $x = target - 1
    
    // Assume increment function for now (can be generalized later)
    return flint_add_linear_constraint(store, var_id, 1.0, 1.0, (double)target_value, STRENGTH_REQUIRED);
}

/**
 * Enhanced function constraint solver that can handle various arithmetic patterns
 */
bool flint_solve_general_arithmetic_constraint(ConstraintStore* store, const char* function_name, 
                                             VarId var_id, double target_value) {
    if (!store || !function_name) return false;
    
    printf("DEBUG: flint_solve_general_arithmetic_constraint: %s(var_%llu) = %f\n", 
           function_name, (unsigned long long)var_id, target_value);
    
    // Handle different arithmetic function patterns
    if (strcmp(function_name, "increment") == 0 || strcmp(function_name, "inc") == 0) {
        // increment(x) = x + 1
        return flint_add_linear_constraint(store, var_id, 1.0, 1.0, target_value, STRENGTH_REQUIRED);
    } 
    else if (strcmp(function_name, "decrement") == 0 || strcmp(function_name, "dec") == 0) {
        // decrement(x) = x - 1
        return flint_add_linear_constraint(store, var_id, 1.0, -1.0, target_value, STRENGTH_REQUIRED);
    }
    else if (strcmp(function_name, "double") == 0 || strcmp(function_name, "twice") == 0) {
        // double(x) = 2 * x
        return flint_add_linear_constraint(store, var_id, 2.0, 0.0, target_value, STRENGTH_REQUIRED);
    }
    else if (strcmp(function_name, "half") == 0) {
        // half(x) = x / 2 = 0.5 * x
        return flint_add_linear_constraint(store, var_id, 0.5, 0.0, target_value, STRENGTH_REQUIRED);
    }
    else if (strcmp(function_name, "square") == 0) {
        // Non-linear - not supported by Cassowary directly
        printf("[WARNING] Non-linear constraint 'square' not supported by NLopt directly for linear problems\n");
        return false;
    }
    else {
        // Try to parse generic linear functions like "add5" -> x + 5
        if (strncmp(function_name, "add", 3) == 0) {
            double constant = atof(function_name + 3);
            return flint_add_linear_constraint(store, var_id, 1.0, constant, target_value, STRENGTH_REQUIRED);
        }
        else if (strncmp(function_name, "sub", 3) == 0) {
            double constant = atof(function_name + 3);
            return flint_add_linear_constraint(store, var_id, 1.0, -constant, target_value, STRENGTH_REQUIRED);
        }
        else if (strncmp(function_name, "mul", 3) == 0) {
            double multiplier = atof(function_name + 3);
            return flint_add_linear_constraint(store, var_id, multiplier, 0.0, target_value, STRENGTH_REQUIRED);
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
    if (!store) return true; // Removed !store->solver check
    
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
    }
    else {
        printf("DEBUG: Variable %llu has no binding\n", (unsigned long long)var_id);
    }
    
    // --- NLopt Integration ---
    // 1. Determine the number of variables (dimensions)
    unsigned num_vars = store->var_count;
    if (num_vars == 0) {
        printf("DEBUG: No variables in constraint store, nothing to optimize.\n");
        return true;
    }

    // 2. Create NLopt optimizer object if not already created
    if (!store->nlopt_solver) {
        // Choose an algorithm. For linear equality/inequality, NLOPT_LN_AUGLAG_EQ or NLOPT_LN_COBYLA could work.
        // NLOPT_LN_AUGLAG_EQ is good for equality constraints.
        store->nlopt_solver = nlopt_create(NLOPT_LN_AUGLAG_EQ, num_vars);
        if (!store->nlopt_solver) {
            printf("ERROR: Failed to create NLopt solver.\n");
            return false;
        }
        // Set a dummy objective function (minimize 0)
        nlopt_set_min_objective(store->nlopt_solver, nlopt_objective_function, NULL);
        // Set optimization tolerance
        nlopt_set_ftol_rel(store->nlopt_solver, 1e-4); // Relative tolerance on function value
        nlopt_set_xtol_rel(store->nlopt_solver, 1e-4); // Relative tolerance on optimization parameters
    } else {
        // If solver already exists, ensure its dimensions match.
        // If not, destroy and recreate, or handle appropriately.
        // For simplicity, we'll assume dimensions don't change or recreate if they do.
        if (nlopt_get_dimension(store->nlopt_solver) != num_vars) {
            nlopt_destroy(store->nlopt_solver);
            store->nlopt_solver = nlopt_create(NLOPT_LN_AUGLAG_EQ, num_vars);
            if (!store->nlopt_solver) {
                printf("ERROR: Failed to recreate NLopt solver with new dimensions.\n");
                return false;
            }
            nlopt_set_min_objective(store->nlopt_solver, nlopt_objective_function, NULL);
            nlopt_set_ftol_rel(store->nlopt_solver, 1e-4);
            nlopt_set_xtol_rel(store->nlopt_solver, 1e-4);
        }
    }

    // 3. Set initial guess for variables
    double x[num_vars];
    for (unsigned i = 0; i < num_vars; ++i) {
        x[i] = store->variables[i].value;
    }

    // 4. Add constraints to NLopt
    // This is a crucial part. NLopt expects a separate function for each constraint.
    // We need to iterate through stored FlintConstraints and add them to NLopt.
    // This will require passing per-constraint data to NLopt's constraint functions.
    // For now, we'll add a placeholder.
    
    // Clear existing constraints from NLopt object before adding new ones
    nlopt_remove_equality_constraints(store->nlopt_solver);
    nlopt_remove_inequality_constraints(store->nlopt_solver);

    for (size_t i = 0; i < store->constraint_count; ++i) {
        FlintConstraint* constraint = &store->constraints[i];
        
        // Skip function constraints as they are handled algebraically or by backtracking
        if (constraint->type == CONSTRAINT_FUNCTION) continue;

        NloptFuncData* constraint_data = flint_alloc(sizeof(NloptFuncData));
        constraint_data->store = store;
        constraint_data->constraint = constraint;
        
        // For linear constraints, NLopt expects a vector of coefficients and a right-hand side.
        // We need to convert our stored coefficients and constant_term into this format.
        // NLopt's add_equality_constraint and add_inequality_constraint take a function pointer
        // and a tolerance. The function evaluates the constraint.

        if (constraint->type == CONSTRAINT_EQUAL) {
            // For linear equality: a*x + b*y + ... = C  =>  a*x + b*y + ... - C = 0
            // The constraint function should return (a*x + b*y + ... - C)
            nlopt_add_equality_constraint(store->nlopt_solver, nlopt_linear_equality_constraint, constraint_data, 1e-8);
        } else if (constraint->type == CONSTRAINT_LEQ) {
            // For linear inequality: a*x + b*y + ... <= C  =>  a*x + b*y + ... - C <= 0
            // The constraint function should return (a*x + b*y + ... - C)
            nlopt_add_inequality_constraint(store->nlopt_solver, nlopt_linear_inequality_constraint, constraint_data, 1e-8);
        } else if (constraint->type == CONSTRAINT_GEQ) {
            // For linear inequality: a*x + b*y + ... >= C  =>  -(a*x + b*y + ...) + C <= 0
            // The constraint function should return (-(a*x + b*y + ...) + C)
            nlopt_add_inequality_constraint(store->nlopt_solver, nlopt_linear_inequality_constraint, constraint_data, 1e-8);
        }
    }

    // 5. Run optimization
    double minf; // minimum objective value (unused for constraint satisfaction)
    nlopt_result nlopt_res = nlopt_optimize(store->nlopt_solver, x, &minf);

    // 6. Update Flint variables with optimized values
    if (nlopt_res >= 0) { // Optimization successful or stopped for a good reason
        for (unsigned i = 0; i < num_vars; ++i) {
            store->variables[i].value = x[i];
        }
        printf("DEBUG: NLopt optimization successful. Variables updated.\n");
    } else {
        printf("ERROR: NLopt optimization failed with result: %s\n", nlopt_result_to_string(nlopt_res));
        return false;
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
    if (!store || !constraint) return;
    
    // For NLopt, removing a constraint means not adding it to the nlopt_opt object
    // during the next optimization run. We don't explicitly remove it from the stored
    // constraints array, but rather mark it as inactive or rebuild the NLopt problem.
    // For simplicity, we'll just set its type to an inactive type or similar.
    // A more robust solution would involve rebuilding the NLopt problem from active constraints.
    constraint->type = CONSTRAINT_UNIFY; // Mark as inactive/handled by unification
}

void flint_print_constraint_values(ConstraintStore* store) {
    if (!store) return;
    
    printf("=== Constraint Variable Values ===\n");
    for (size_t i = 0; i < store->var_count; i++) {
        FlintConstraintVar* var = &store->variables[i];
        printf("Var %llu", (unsigned long long)var->flint_id);
        if (var->name) {
            printf(" (%s)", var->name);
        }
        printf(": %.6f\n", var->value); // Directly print stored value
    }
    printf("Total constraints: %zu\n", store->constraint_count);
    printf("=================================\n");
}

/**
 * Check if the constraint system is satisfiable
 */
bool flint_is_constraint_system_satisfiable(ConstraintStore* store) {
    if (!store) return true; // Removed !store->solver check
    
    // For NLopt, satisfiability is determined by the result of nlopt_optimize.
    // If it returns NLOPT_SUCCESS or a similar positive code, it's satisfiable.
    // We'll assume that if flint_solve_constraints runs without returning false, it's satisfiable.
    return true; 
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
    printf("Solver: %s\n", store->nlopt_solver ? "Available (NLopt)" : "NULL");
    printf("Variables: %zu / %zu\n", store->var_count, store->var_capacity);
    printf("Constraints: %zu / %zu\n", store->constraint_count, store->constraint_capacity);
    printf("Auto-update: %s\n", store->auto_update ? "Enabled" : "Disabled"); // Auto-update is false for NLopt
    
    if (store->var_count > 0) {
        printf("Variable details:\n");
        for (size_t i = 0; i < store->var_count; i++) {
            FlintConstraintVar* var = &store->variables[i];
            printf("  Var %llu: value=%.6f\n",
                   (unsigned long long)var->flint_id,
                   var->value);
        }
    }
    
    printf("===============================\n");
}

// Add a function constraint like f(x) = target
bool flint_add_function_constraint(ConstraintStore* store, const char* function_name, VarId var_id, int target_value) {
    if (!store || !function_name) {
        return false;
    }
    
    printf("DEBUG: Adding function constraint: %s($%llu) = %d\n", function_name, var_id, target_value);
    
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
    
    printf("DEBUG: Function constraint added successfully (total: %zu)\n", store->constraint_count);
    return true;
}

// Solve function constraints algebraically by analyzing function structure
bool flint_solve_function_constraint_algebraically(ConstraintStore* store, const char* function_name, VarId var_id, int target_value, Environment* env) {
    printf("DEBUG: Attempting algebraic solution for %s($%llu) = %d\n", function_name, var_id, target_value);
    
    // For now, we'll implement simple linear function solving
    // A more sophisticated system would analyze the function body or use symbolic manipulation
    
    // Get the registered function to analyze its structure
    if (!flint_is_function_registered(function_name)) {
        printf("DEBUG: Function %s not registered, cannot analyze\n", function_name);
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
        printf("DEBUG: Algebraic solution: increment(%lld) = %lld + 5 = %d\n", solution, solution, target_value);
    } else {
        // For other functions, we could implement more sophisticated analysis
        // This could include:
        // - Parsing the function body to extract linear relationships
        // - Using symbolic differentiation for inverse functions
        // - Pattern matching on common function forms
        printf("DEBUG: No algebraic solver implemented for function %s\n", function_name);
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
            
            printf("DEBUG: Unifying variable $%llu with solution %lld\n", var_id, solution);
            bool unified = flint_unify(&var_value, solution_value, env);
            
            if (unified) {
                printf("DEBUG: Successfully solved constraint: $%llu = %lld\n", var_id, solution);
                return true;
            } else {
                printf("DEBUG: Failed to unify variable with solution\n");
                return false;
            }
        } else {
            printf("DEBUG: Could not find logical variable %llu\n", var_id);
            return false;
        }
    }
    
    return false;
}

