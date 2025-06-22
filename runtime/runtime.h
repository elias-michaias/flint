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

// Function application and higher-order operations
Value* flint_apply_function(Value* func, Value** args, size_t arg_count, Environment* env);
bool flint_is_fully_applied(Value* func);

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

// Safe dereference with null checking (used internally)
Value* flint_deref_value(Value* val);

// =============================================================================
// UNIFIED CONSTRAINT-UNIFICATION INTERFACE
// =============================================================================

// Unified constraint-aware unification (preferred for functional logic programming)
bool flint_unify_with_constraints(Value* val1, Value* val2, Environment* env);

// Create constraint relationships with automatic variable management
bool flint_constrain_variables(Environment* env, VarId* var_ids, size_t var_count, 
                              ArithmeticOp constraint_type, double constant,
                              ConstraintStrength strength);

// Convenience functions for common constraint patterns
bool flint_add_sum_constraint(Environment* env, VarId x, VarId y, VarId z, ConstraintStrength strength);
bool flint_constrain_to_value(Environment* env, VarId var_id, double value, ConstraintStrength strength);

// Constraint propagation helpers
void flint_propagate_constraints_from_values(ConstraintStore* store, Value* val1, Value* val2, Environment* env);
void flint_extract_variable_ids(Value* val, VarId* var_array, size_t* count, size_t max_vars);

// Variable registration helper
bool flint_register_variable_with_env(Environment* env, Value* var_value);

// Variable registration with environment
bool flint_register_variable_with_env(Environment* env, Value* var_value);

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

// Create a new constraint store with amoeba solver
ConstraintStore* flint_create_constraint_store(void);

// Free a constraint store and all associated resources
void flint_free_constraint_store(ConstraintStore* store);

// Variable management
FlintConstraintVar* flint_get_or_create_constraint_var(ConstraintStore* store, VarId var_id, const char* name);
void flint_suggest_constraint_value(ConstraintStore* store, VarId var_id, double value);
double flint_get_constraint_value(ConstraintStore* store, VarId var_id);

// Constraint creation
FlintConstraint* flint_add_arithmetic_constraint(ConstraintStore* store, 
                                                ArithmeticOp op,
                                                VarId* variables,
                                                size_t var_count,
                                                double constant,
                                                ConstraintStrength strength);

// Convenience functions for common constraints
FlintConstraint* flint_add_equals_constraint(ConstraintStore* store, VarId var1, VarId var2, ConstraintStrength strength);
FlintConstraint* flint_add_addition_constraint(ConstraintStore* store, VarId x, VarId y, VarId sum, ConstraintStrength strength);
FlintConstraint* flint_add_subtraction_constraint(ConstraintStore* store, VarId x, VarId y, VarId diff, ConstraintStrength strength);
FlintConstraint* flint_add_inequality_constraint(ConstraintStore* store, VarId var1, VarId var2, bool less_than, ConstraintStrength strength);

// Constraint removal
void flint_remove_constraint(ConstraintStore* store, FlintConstraint* constraint);

// Legacy constraint functions (for compatibility)
void flint_add_constraint(ConstraintStore* store, VarId var1, VarId var2, int constraint_type, Value* data);
bool flint_solve_constraints(ConstraintStore* store, VarId var_id, Environment* env);

// Debugging
void flint_print_constraint_values(ConstraintStore* store);

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
// LINEAR RESOURCE MANAGEMENT
// =============================================================================

// Initialize/cleanup the linear resource management system
void flint_init_linear_system(void);
void flint_cleanup_linear_system(void);

// Core linear resource operations
void flint_mark_linear(Value* value);
void flint_mark_consumed(Value* value, LinearOp operation);
bool flint_is_consumed(Value* value);
Value* flint_consume_value(Value* value, LinearOp operation);

// Opt-in non-consumptive operations
Value* flint_copy_for_sharing(Value* value);
Value* flint_deep_copy_value(Value* value);

// Linear operations with automatic consumption
Value* flint_linear_unify(Value* val1, Value* val2, Environment* env);
Value* flint_linear_apply_function(Value* func, Value** args, size_t arg_count, Environment* env);
Value* flint_linear_list_access(Value* list, size_t index);
LinearListDestructure flint_linear_list_destructure(Value* list);

// Trail management for backtracking
LinearTrail* flint_create_linear_trail(void);
void flint_free_linear_trail(LinearTrail* trail);
void flint_trail_record_consumption(LinearTrail* trail, Value* value, LinearOp operation);
LinearCheckpoint flint_trail_create_checkpoint(LinearTrail* trail);
void flint_trail_rollback_to_checkpoint(LinearTrail* trail, LinearCheckpoint checkpoint);
void flint_trail_commit_checkpoint(LinearTrail* trail, LinearCheckpoint checkpoint);

// Additional convenience functions for the linear system
void flint_set_linear_context(Environment* env);
void flint_clear_linear_context(void);
Value* flint_share_value(Value* value);
LinearCheckpoint flint_linear_checkpoint(LinearTrail* trail);
void flint_linear_restore(LinearTrail* trail, LinearCheckpoint checkpoint);
LinearListDestructure flint_linear_destructure_list(Value* list);

// Choice point integration
LinearCheckpoint flint_choice_create_linear_checkpoint(void);
void flint_choice_rollback_linear(LinearCheckpoint checkpoint);
void flint_choice_commit_linear(LinearCheckpoint checkpoint);

// =============================================================================
// C INTEROPERABILITY
// =============================================================================

// C type enumeration for interop
typedef enum {
    C_TYPE_VOID,
    C_TYPE_INT,
    C_TYPE_LONG,
    C_TYPE_DOUBLE,
    C_TYPE_STRING,      // char*
    C_TYPE_POINTER      // void*
} CType;

// Register C functions with Flint
bool flint_register_c_function(const char* name, void* func_ptr, 
                               CType return_type, CType* param_types, size_t param_count,
                               bool consumes_args);

// Call a registered C function from Flint (deterministic computation)
Value* flint_call_c_function(const char* name, Value** args, size_t arg_count, Environment* env);

// Integration with narrowing system
Value* flint_narrow_c_function(const char* name, Value** args, size_t arg_count, Environment* env);

// Convenience functions for common C function signatures
bool flint_register_c_int_function(const char* name, int (*func)(int));
bool flint_register_c_string_function(const char* name, char* (*func)(char*));
bool flint_register_c_math_function(const char* name, double (*func)(double));
bool flint_register_c_binary_int_function(const char* name, int (*func)(int, int));

// Create Flint function wrapper for C function
Value* flint_create_c_function_wrapper(const char* c_func_name);

// Initialize and cleanup C interop
void flint_init_builtin_c_functions(void);
void flint_cleanup_c_interop(void);

// =============================================================================
// ASYNCHRONOUS OPERATIONS AND STRUCTURED CONCURRENCY
// =============================================================================

// Forward declarations for async types
typedef struct AsyncContext AsyncContext;

// Channel wrapper for Flint values
typedef struct FlintChannel {
    int dill_channel[2];           // libdill channel handle (send and recv)
    bool is_closed;                // Whether the channel is closed
    size_t capacity;               // Channel capacity (0 = synchronous)
    CType value_type;              // Type of values in this channel
} FlintChannel;

// Bundle for managing multiple coroutines
typedef struct CoroutineBundle {
    FlintChannel** result_channels;
    size_t count;
    size_t capacity;
} CoroutineBundle;

// Async context management
AsyncContext* flint_create_async_context(Environment* env);
void flint_set_async_context(AsyncContext* ctx);
AsyncContext* flint_get_async_context(void);
void flint_free_async_context(AsyncContext* ctx);

// Channel operations
FlintChannel* flint_create_channel(size_t capacity, CType value_type);
bool flint_channel_send(FlintChannel* chan, Value* value, int timeout_ms);
Value* flint_channel_recv(FlintChannel* chan, int timeout_ms);
void flint_channel_close(FlintChannel* chan);

// Coroutine operations
FlintChannel* flint_spawn_coroutine(Value* (*func)(Value**, size_t, Environment*),
                                   Value** args, size_t arg_count, Environment* env);
Value* flint_await_coroutine(FlintChannel* result_channel, int timeout_ms);

// Structured concurrency (bundles)
CoroutineBundle* flint_create_bundle(size_t initial_capacity);
bool flint_bundle_spawn(CoroutineBundle* bundle, 
                       Value* (*func)(Value**, size_t, Environment*),
                       Value** args, size_t arg_count, Environment* env);
Value** flint_bundle_wait_all(CoroutineBundle* bundle, int timeout_ms);
Value* flint_bundle_wait_any(CoroutineBundle* bundle, size_t* completed_index, int timeout_ms);
void flint_free_bundle(CoroutineBundle* bundle);

// Async I/O operations
Value* flint_async_read_file(const char* filename);
void flint_async_sleep(int milliseconds);

// Get current time in milliseconds (for timing measurements)
int64_t flint_now(void);

// Integration with Flint runtime
void flint_init_async_system(Environment* env);
void flint_cleanup_async_system(void);
void flint_register_async_functions(void);

// Narrowing functions for async operations
Value* flint_narrow_async_spawn(Value** args, size_t arg_count, Environment* env);
Value* flint_narrow_async_await(Value** args, size_t arg_count, Environment* env);

// =============================================================================
// LIST OPERATIONS
// =============================================================================

// List creation and basic operations
Value* flint_list_create(Value** elements, size_t count);
Value* flint_list_create_empty(void);
Value* flint_list_create_single(Value* element);
size_t flint_list_length(Value* list);
bool flint_list_is_empty(Value* list);

// List access and manipulation
Value* flint_list_get_element(Value* list, size_t index);
Value* flint_list_get_head(Value* list);
Value* flint_list_get_tail(Value* list);
Value* flint_list_prepend(Value* element, Value* list);
Value* flint_list_append_element(Value* list, Value* element);

// List operations (append, reverse, etc.)
Value* flint_list_append(Value* list1, Value* list2);
Value* flint_list_reverse(Value* list);

// List printing and groundness
void flint_list_print(Value* list);
bool flint_list_is_ground(Value* list);

// Linear list operations
Value* flint_list_linear_access(Value* list, size_t index);
LinearListDestructure flint_list_linear_destructure(Value* list);
Value* flint_list_deep_copy(Value* list);
void flint_list_free(Value* list);

// List pattern matching
bool flint_list_match_pattern(Value* list_val, Pattern* pattern, Environment* env);

// List unification
bool flint_list_unify(Value* val1, Value* val2, Environment* env);

// Narrowing operations (constraint-based list operations)
Value* flint_list_narrow_append(Value** args, size_t arg_count, Environment* env);
Value* flint_list_narrow_reverse(Value** args, size_t arg_count, Environment* env);
Value* flint_list_narrow_length(Value** args, size_t arg_count, Environment* env);

// =============================================================================

#endif // FLINT_RUNTIME_H
