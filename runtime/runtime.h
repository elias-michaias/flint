#ifndef FLINT_RUNTIME_H
#define FLINT_RUNTIME_H

#include "types.h"

// =============================================================================
// MEMORY MANAGEMENT
// =============================================================================

// Initialize the runtime system
void flint_init_runtime(void);

// Cleanup the runtime system  
void flint_cleanup_runtime(void);

// Allocate memory with automatic cleanup tracking
void* flint_alloc(size_t size);

// Free memory (for linear types, this happens automatically)
void flint_free(void* ptr);

// =============================================================================
// VALUE MANAGEMENT
// =============================================================================

// Create different types of values
Value* flint_create_integer(int64_t val);
Value* flint_create_float(double val);
Value* flint_create_string(const char* str);
Value* flint_create_atom(const char* atom);
Value* flint_create_list(Value** elements, size_t count);
Value* flint_create_record(char** field_names, Value** field_values, size_t field_count);
Value* flint_create_function(const char* name, int arity, void* impl);
Value* flint_create_partial_app(Value* func, Value** args, int applied_count);

// Create logical variables
Value* flint_create_logical_var(bool is_linear);
LogicalVar* flint_get_logical_var(Value* val);

// Deep copy values (respecting linearity)
Value* flint_copy_value(Value* val);

// Free values (automatic for linear types)
void flint_free_value(Value* val);

// =============================================================================
// UNIFICATION ENGINE
// =============================================================================

// Main unification function with occurs check
bool flint_unify(Value* val1, Value* val2, Environment* env);

// Check if two values can be unified (without side effects)
bool flint_can_unify(Value* val1, Value* val2);

// Occurs check to prevent infinite structures
bool flint_occurs_check(VarId var_id, Value* val);

// Dereference a value (follow variable bindings)
Value* flint_deref(Value* val);

// =============================================================================
// NARROWING ENGINE  
// =============================================================================

// Evaluate a function call with narrowing
Value* flint_narrow_call(char* func_name, Value** args, size_t arg_count, Environment* env);

// Create a suspension for delayed evaluation
Suspension* flint_create_suspension(SuspensionType type, VarId* deps, size_t dep_count, void* computation);

// Resume all suspensions waiting on a variable
void flint_resume_suspensions(VarId var_id, Environment* env);

// =============================================================================
// ENVIRONMENT MANAGEMENT
// =============================================================================

// Create a new environment
Environment* flint_create_environment(Environment* parent);

// Free an environment
void flint_free_environment(Environment* env);

// Bind a variable in the environment
void flint_bind_variable(Environment* env, VarId var_id, Value* val);

// Look up a variable in the environment
LogicalVar* flint_lookup_variable(Environment* env, VarId var_id);

// =============================================================================
// BACKTRACKING AND CHOICE POINTS
// =============================================================================

// Create a choice point for backtracking
ChoicePoint* flint_create_choice_point(Environment* env, Value** alternatives, size_t alt_count);

// Backtrack to the most recent choice point
bool flint_backtrack(ChoicePoint** current_choice);

// Commit to current choice (remove choice point)
void flint_commit_choice(ChoicePoint* choice);

// =============================================================================
// CONSTRAINT STORE
// =============================================================================

// Create a new constraint store
ConstraintStore* flint_create_constraint_store(void);

// Add a constraint to the store
void flint_add_constraint(ConstraintStore* store, VarId var1, VarId var2, int constraint_type, Value* data);

// Solve constraints when variables become instantiated
bool flint_solve_constraints(ConstraintStore* store, VarId var_id, Environment* env);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

// Print a value for debugging
void flint_print_value(Value* val);

// Check if a value is ground (contains no logical variables)
bool flint_is_ground(Value* val);

// Get all free variables in a value
VarId* flint_get_free_vars(Value* val, size_t* count);

// Generate a fresh variable ID
VarId flint_fresh_var_id(void);

// =============================================================================
// PATTERN MATCHING ENGINE
// =============================================================================

// Create a pattern for matching
Pattern* flint_create_pattern(ValueType type);

// Match a value against a pattern
bool flint_pattern_match(Value* val, Pattern* pattern, Environment* env);

// Helper functions for complex pattern matching
bool flint_match_list_pattern(Value* list_val, Pattern* pattern, Environment* env);
bool flint_match_record_pattern(Value* record_val, Pattern* pattern, Environment* env);

// Free a pattern
void flint_free_pattern(Pattern* pattern);

// =============================================================================
// HIGHER-ORDER FUNCTIONS
// =============================================================================

// Apply a function to arguments (supports partial application)
Value* flint_apply_function(Value* func, Value** args, size_t arg_count, Environment* env);

// Check if a function is fully applied
bool flint_is_fully_applied(Value* func);

// =============================================================================
// NON-DETERMINISTIC CHOICE
// =============================================================================

// Create a choice between multiple values
Value* flint_create_choice(Value** alternatives, size_t count, Environment* env);

// Get all solutions for a non-deterministic computation
Value** flint_get_all_solutions(Value* expr, Environment* env, size_t* solution_count);

// =============================================================================

#endif // FLINT_RUNTIME_H
