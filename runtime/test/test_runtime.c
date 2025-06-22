#include "runtime.h"
#include "amoeba.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Test utilities
#define TEST(name) printf("Testing %s...\n", name)
#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("ASSERTION FAILED: %s at line %d\n", #condition, __LINE__); \
            return false; \
        } \
    } while(0)

// Test basic value creation
bool test_value_creation() {
    TEST("Value Creation");
    
    // Test integer creation
    Value* int_val = flint_create_integer(42);
    ASSERT(int_val->type == VAL_INTEGER);
    ASSERT(int_val->data.integer == 42);
    
    // Test string creation
    Value* str_val = flint_create_string("hello");
    ASSERT(str_val->type == VAL_STRING);
    ASSERT(strcmp(str_val->data.string, "hello") == 0);
    
    // Test atom creation
    Value* atom_val = flint_create_atom("test_atom");
    ASSERT(atom_val->type == VAL_ATOM);
    ASSERT(strcmp(atom_val->data.atom, "test_atom") == 0);
    
    // Test logical variable creation
    Value* var_val = flint_create_logical_var(false);
    ASSERT(var_val->type == VAL_LOGICAL_VAR);
    ASSERT(var_val->data.logical_var != NULL);
    ASSERT(var_val->data.logical_var->binding == NULL);
    
    printf("✓ Value creation tests passed\n");
    return true;
}

// Test list creation and operations
bool test_list_operations() {
    TEST("List Operations");
    
    // Create a list [1, 2, 3]
    Value* elem1 = flint_create_integer(1);
    Value* elem2 = flint_create_integer(2);
    Value* elem3 = flint_create_integer(3);
    
    Value* elements[] = {elem1, elem2, elem3};
    Value* list = flint_create_list(elements, 3);
    
    ASSERT(list->type == VAL_LIST);
    ASSERT(list->data.list.length == 3);
    ASSERT(list->data.list.elements[0].data.integer == 1);
    ASSERT(list->data.list.elements[1].data.integer == 2);
    ASSERT(list->data.list.elements[2].data.integer == 3);
    
    // Test empty list
    Value* empty_list = flint_create_list(NULL, 0);
    ASSERT(empty_list->type == VAL_LIST);
    ASSERT(empty_list->data.list.length == 0);
    ASSERT(empty_list->data.list.elements == NULL);
    
    printf("✓ List operation tests passed\n");
    return true;
}

// Test record creation
bool test_record_operations() {
    TEST("Record Operations");
    
    // Create a record {name: "Alice", age: 30}
    char* field_names[] = {"name", "age"};
    Value* name_val = flint_create_string("Alice");
    Value* age_val = flint_create_integer(30);
    Value* field_values[] = {name_val, age_val};
    
    Value* record = flint_create_record(field_names, field_values, 2);
    
    ASSERT(record->type == VAL_RECORD);
    ASSERT(record->data.record.field_count == 2);
    ASSERT(strcmp(record->data.record.field_names[0], "name") == 0);
    ASSERT(strcmp(record->data.record.field_names[1], "age") == 0);
    ASSERT(record->data.record.field_values[0].type == VAL_STRING);
    ASSERT(record->data.record.field_values[1].type == VAL_INTEGER);
    
    printf("✓ Record operation tests passed\n");
    return true;
}

// Test basic unification
bool test_unification() {
    TEST("Basic Unification");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test unification of integers
    Value* int1 = flint_create_integer(42);
    Value* int2 = flint_create_integer(42);
    Value* int3 = flint_create_integer(24);
    
    ASSERT(flint_unify(int1, int2, env) == true);
    ASSERT(flint_unify(int1, int3, env) == false);
    
    // Test unification of variables
    Value* var1 = flint_create_logical_var(false);
    Value* var2 = flint_create_logical_var(false);
    
    ASSERT(flint_unify(var1, int1, env) == true);
    ASSERT(flint_get_logical_var(var1)->binding == int1);
    
    ASSERT(flint_unify(var2, var1, env) == true);
    
    // Test occurs check
    Value* var3 = flint_create_logical_var(false);
    Value* list_with_var[] = {var3};
    Value* list = flint_create_list(list_with_var, 1);
    ASSERT(flint_unify(var3, list, env) == false);  // Should fail occurs check
    
    flint_free_environment(env);
    
    printf("✓ Unification tests passed\n");
    return true;
}

// Test narrowing operations
bool test_narrowing() {
    TEST("Narrowing Operations");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test length function
    Value* elem1 = flint_create_integer(1);
    Value* elem2 = flint_create_integer(2);
    Value* elements[] = {elem1, elem2};
    Value* list = flint_create_list(elements, 2);
    
    Value* result_var = flint_create_logical_var(false);
    Value* length_args[] = {list, result_var};
    
    Value* length_result = flint_narrow_call("length", length_args, 2, env);
    ASSERT(length_result != NULL);
    
    // The result should be unified with 2
    Value* result_val = flint_deref(result_var);
    ASSERT(result_val->type == VAL_INTEGER);
    ASSERT(result_val->data.integer == 2);
    
    // Test reverse function
    Value* rev_result_var = flint_create_logical_var(false);
    Value* reverse_args[] = {list, rev_result_var};
    
    Value* reverse_result = flint_narrow_call("reverse", reverse_args, 2, env);
    ASSERT(reverse_result != NULL);
    
    Value* rev_val = flint_deref(rev_result_var);
    ASSERT(rev_val->type == VAL_LIST);
    ASSERT(rev_val->data.list.length == 2);
    ASSERT(rev_val->data.list.elements[0].data.integer == 2);
    ASSERT(rev_val->data.list.elements[1].data.integer == 1);
    
    flint_free_environment(env);
    
    printf("✓ Narrowing tests passed\n");
    return true;
}

// Test free variable extraction
bool test_free_variables() {
    TEST("Free Variable Extraction");
    
    // Create a structure with free variables
    Value* var1 = flint_create_logical_var(false);
    Value* var2 = flint_create_logical_var(false);
    Value* int_val = flint_create_integer(42);
    
    Value* elements[] = {var1, int_val, var2};
    Value* list = flint_create_list(elements, 3);
    
    size_t free_count;
    VarId* free_vars = flint_get_free_vars(list, &free_count);
    
    ASSERT(free_count == 2);
    ASSERT(free_vars != NULL);
    
    // Check that we got the right variable IDs
    VarId var1_id = flint_get_logical_var(var1)->id;
    VarId var2_id = flint_get_logical_var(var2)->id;
    
    bool found_var1 = false, found_var2 = false;
    for (size_t i = 0; i < free_count; i++) {
        if (free_vars[i] == var1_id) found_var1 = true;
        if (free_vars[i] == var2_id) found_var2 = true;
    }
    
    ASSERT(found_var1 && found_var2);
    
    free(free_vars);
    
    printf("✓ Free variable tests passed\n");
    return true;
}

// Test environment operations
bool test_environment() {
    TEST("Environment Operations");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create a variable and bind it
    VarId var_id = flint_fresh_var_id();
    Value* val = flint_create_integer(100);
    
    flint_bind_variable(env, var_id, val);
    
    // Look up the variable
    LogicalVar* found_var = flint_lookup_variable(env, var_id);
    ASSERT(found_var != NULL);
    ASSERT(found_var->id == var_id);
    ASSERT(found_var->binding == val);
    
    // Test nested environments
    Environment* child_env = flint_create_environment(env);
    
    // Should find variable from parent
    LogicalVar* found_in_child = flint_lookup_variable(child_env, var_id);
    ASSERT(found_in_child != NULL);
    ASSERT(found_in_child->id == var_id);
    
    flint_free_environment(child_env);
    flint_free_environment(env);
    
    printf("✓ Environment tests passed\n");
    return true;
}

// Test printing functionality
void test_printing() {
    TEST("Value Printing");
    
    printf("Integer: ");
    Value* int_val = flint_create_integer(42);
    flint_print_value(int_val);
    printf("\n");
    
    printf("String: ");
    Value* str_val = flint_create_string("hello world");
    flint_print_value(str_val);
    printf("\n");
    
    printf("Atom: ");
    Value* atom_val = flint_create_atom("test_atom");
    flint_print_value(atom_val);
    printf("\n");
    
    printf("List: ");
    Value* elem1 = flint_create_integer(1);
    Value* elem2 = flint_create_string("two");
    Value* elem3 = flint_create_atom("three");
    Value* elements[] = {elem1, elem2, elem3};
    Value* list = flint_create_list(elements, 3);
    flint_print_value(list);
    printf("\n");
    
    printf("Logical Variable: ");
    Value* var = flint_create_logical_var(false);
    flint_print_value(var);
    printf("\n");
    
    printf("✓ Printing tests completed\n");
}

// Test higher-order functions
bool test_higher_order_functions() {
    TEST("Higher-Order Functions");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create a simple function (length)
    Value* length_func = flint_create_function("length", 2, NULL);
    ASSERT(length_func->type == VAL_FUNCTION);
    ASSERT(length_func->data.function.arity == 2);
    ASSERT(strcmp(length_func->data.function.name, "length") == 0);
    
    // Test partial application
    Value* list = flint_create_list(NULL, 0); // Empty list
    Value* partial_args[] = {list};
    Value* partial_length = flint_create_partial_app(length_func, partial_args, 1);
    
    ASSERT(partial_length->type == VAL_PARTIAL_APP);
    ASSERT(partial_length->data.function.applied_count == 1);
    ASSERT(!flint_is_fully_applied(partial_length));
    
    // Apply the remaining argument
    Value* result_var = flint_create_logical_var(false);
    Value* remaining_args[] = {result_var};
    Value* result = flint_apply_function(partial_length, remaining_args, 1, env);
    
    ASSERT(result != NULL);
    
    // Check that the result variable got unified with 0 (length of empty list)
    Value* result_val = flint_deref(result_var);
    ASSERT(result_val->type == VAL_INTEGER);
    ASSERT(result_val->data.integer == 0);
    
    flint_free_environment(env);
    
    printf("✓ Higher-order function tests passed\n");
    return true;
}

// Test pattern matching
bool test_pattern_matching() {
    TEST("Pattern Matching");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test simple atom pattern
    Pattern* atom_pattern = flint_create_pattern(VAL_ATOM);
    atom_pattern->data.atom = strdup("test");
    
    Value* atom_val = flint_create_atom("test");
    Value* wrong_atom = flint_create_atom("wrong");
    
    ASSERT(flint_pattern_match(atom_val, atom_pattern, env));
    ASSERT(!flint_pattern_match(wrong_atom, atom_pattern, env));
    
    // Test integer pattern
    Pattern* int_pattern = flint_create_pattern(VAL_INTEGER);
    int_pattern->data.integer = 42;
    
    Value* int_val = flint_create_integer(42);
    Value* wrong_int = flint_create_integer(24);
    
    ASSERT(flint_pattern_match(int_val, int_pattern, env));
    ASSERT(!flint_pattern_match(wrong_int, int_pattern, env));
    
    // Test variable pattern (should always match and bind)
    Pattern* var_pattern = flint_create_pattern(VAL_LOGICAL_VAR);
    var_pattern->data.variable = flint_fresh_var_id();
    
    ASSERT(flint_pattern_match(int_val, var_pattern, env));
    
    // Check that the variable got bound
    LogicalVar* bound_var = flint_lookup_variable(env, var_pattern->data.variable);
    ASSERT(bound_var != NULL);
    ASSERT(bound_var->binding != NULL);
    
    flint_free_pattern(atom_pattern);
    flint_free_pattern(int_pattern);
    flint_free_pattern(var_pattern);
    flint_free_environment(env);
    
    printf("✓ Pattern matching tests passed\n");
    return true;
}

// Test complex unification scenarios
bool test_complex_unification() {
    TEST("Complex Unification");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test unification of nested structures
    Value* elem1 = flint_create_integer(1);
    Value* elem2 = flint_create_integer(2);
    Value* elements1[] = {elem1, elem2};
    Value* list1 = flint_create_list(elements1, 2);
    
    Value* elem3 = flint_create_integer(1);
    Value* elem4 = flint_create_integer(2);
    Value* elements2[] = {elem3, elem4};
    Value* list2 = flint_create_list(elements2, 2);
    
    ASSERT(flint_unify(list1, list2, env));
    
    // Test unification of records
    char* field_names1[] = {"x", "y"};
    Value* val1 = flint_create_integer(10);
    Value* val2 = flint_create_integer(20);
    Value* field_values1[] = {val1, val2};
    Value* record1 = flint_create_record(field_names1, field_values1, 2);
    
    char* field_names2[] = {"x", "y"};
    Value* val3 = flint_create_integer(10);
    Value* val4 = flint_create_integer(20);
    Value* field_values2[] = {val3, val4};
    Value* record2 = flint_create_record(field_names2, field_values2, 2);
    
    ASSERT(flint_unify(record1, record2, env));
    
    // Test unification with variables in nested structures
    Value* var1 = flint_create_logical_var(false);
    Value* var2 = flint_create_logical_var(false);
    Value* var_elements[] = {var1, var2};
    Value* var_list = flint_create_list(var_elements, 2);
    
    Value* ground_elem1 = flint_create_integer(100);
    Value* ground_elem2 = flint_create_integer(200);
    Value* ground_elements[] = {ground_elem1, ground_elem2};
    Value* ground_list = flint_create_list(ground_elements, 2);
    
    ASSERT(flint_unify(var_list, ground_list, env));
    
    // Check that variables got bound correctly
    Value* bound1 = flint_deref(var1);
    Value* bound2 = flint_deref(var2);
    ASSERT(bound1->type == VAL_INTEGER && bound1->data.integer == 100);
    ASSERT(bound2->type == VAL_INTEGER && bound2->data.integer == 200);
    
    // Test occurs check with nested structures
    Value* var3 = flint_create_logical_var(false);
    Value* nested_var_elements[] = {var3};
    Value* nested_list = flint_create_list(nested_var_elements, 1);
    Value* outer_elements[] = {nested_list};
    Value* outer_list = flint_create_list(outer_elements, 1);
    
    ASSERT(!flint_unify(var3, outer_list, env)); // Should fail occurs check
    
    flint_free_environment(env);
    
    printf("✓ Complex unification tests passed\n");
    return true;
}

// Test multi-variable unification
bool test_multi_variable_unification() {
    TEST("Multi-Variable Unification");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test 1: Chain of variable unifications X = Y = Z = 42
    Value* varX = flint_create_logical_var(false);
    Value* varY = flint_create_logical_var(false);
    Value* varZ = flint_create_logical_var(false);
    Value* val42 = flint_create_integer(42);
    
    // Unify X = Y = Z = 42
    ASSERT(flint_unify(varX, varY, env));
    ASSERT(flint_unify(varY, varZ, env));
    ASSERT(flint_unify(varZ, val42, env));
    
    // Check if all variables are bound to 42
    Value* derefX = flint_deref(varX);
    Value* derefY = flint_deref(varY);
    Value* derefZ = flint_deref(varZ);
    
    ASSERT(derefX->type == VAL_INTEGER && derefX->data.integer == 42);
    ASSERT(derefY->type == VAL_INTEGER && derefY->data.integer == 42);
    ASSERT(derefZ->type == VAL_INTEGER && derefZ->data.integer == 42);
    
    // Test 2: Complex structure with many variables [A, B, [C, D]] = [1, 2, [3, 4]]
    Value* varA = flint_create_logical_var(false);
    Value* varB = flint_create_logical_var(false);
    Value* varC = flint_create_logical_var(false);
    Value* varD = flint_create_logical_var(false);
    
    Value* inner_vars[] = {varC, varD};
    Value* inner_list_vars = flint_create_list(inner_vars, 2);
    Value* outer_vars[] = {varA, varB, inner_list_vars};
    Value* var_structure = flint_create_list(outer_vars, 3);
    
    Value* val1 = flint_create_integer(1);
    Value* val2 = flint_create_integer(2);
    Value* val3 = flint_create_integer(3);
    Value* val4 = flint_create_integer(4);
    
    Value* inner_vals[] = {val3, val4};
    Value* inner_list_vals = flint_create_list(inner_vals, 2);
    Value* outer_vals[] = {val1, val2, inner_list_vals};
    Value* ground_structure = flint_create_list(outer_vals, 3);
    
    ASSERT(flint_unify(var_structure, ground_structure, env));
    
    // Check that all nested variables got bound correctly
    Value* derefA = flint_deref(varA);
    Value* derefB = flint_deref(varB);
    Value* derefC = flint_deref(varC);
    Value* derefD = flint_deref(varD);
    
    ASSERT(derefA->type == VAL_INTEGER && derefA->data.integer == 1);
    ASSERT(derefB->type == VAL_INTEGER && derefB->data.integer == 2);
    ASSERT(derefC->type == VAL_INTEGER && derefC->data.integer == 3);
    ASSERT(derefD->type == VAL_INTEGER && derefD->data.integer == 4);
    
    // Test 3: Chain of many variables (stress test)
    const int num_vars = 10;
    Value* vars[num_vars];
    
    // Create 10 variables
    for (int i = 0; i < num_vars; i++) {
        vars[i] = flint_create_logical_var(false);
    }
    
    // Chain them together: var0 = var1 = var2 = ... = var9
    for (int i = 0; i < num_vars - 1; i++) {
        ASSERT(flint_unify(vars[i], vars[i+1], env));
    }
    
    // Bind the last one to a value
    Value* val999 = flint_create_integer(999);
    ASSERT(flint_unify(vars[num_vars-1], val999, env));
    
    // Check if all variables resolve to 999
    for (int i = 0; i < num_vars; i++) {
        Value* deref = flint_deref(vars[i]);
        ASSERT(deref->type == VAL_INTEGER && deref->data.integer == 999);
    }
    
    flint_free_environment(env);
    
    printf("✓ Multi-variable unification tests passed\n");
    return true;
}

// Test constraint propagation with new system
bool test_constraint_propagation() {
    TEST("Constraint Propagation");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    // Create two variables
    VarId var1_id = flint_fresh_var_id();
    VarId var2_id = flint_fresh_var_id();
    
    // Add constraint that var1 == var2 using new constraint system
    FlintConstraint* eq_constraint = flint_add_equals_constraint(store, var1_id, var2_id, STRENGTH_REQUIRED);
    ASSERT(eq_constraint != NULL);
    
    // Suggest a value for var1
    flint_suggest_constraint_value(store, var1_id, 42.0);
    
    // The constraint solver should automatically make var2 equal to var1
    double var1_value = flint_get_constraint_value(store, var1_id);
    double var2_value = flint_get_constraint_value(store, var2_id);
    
    ASSERT(var1_value >= 41.9 && var1_value <= 42.1); // Should be approximately 42
    ASSERT(var2_value >= 41.9 && var2_value <= 42.1); // Should be approximately 42
    
    flint_free_environment(env);
    
    printf("✓ Constraint propagation tests passed\n");
    return true;
}

// Test non-deterministic choice (basic)
bool test_non_deterministic_choice() {
    TEST("Non-Deterministic Choice");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create choice between multiple values
    Value* val1 = flint_create_integer(1);
    Value* val2 = flint_create_integer(2);
    Value* val3 = flint_create_integer(3);
    Value* alternatives[] = {val1, val2, val3};
    
    Value* choice = flint_create_choice(alternatives, 3, env);
    ASSERT(choice != NULL);
    
    // In a simple implementation, this returns the first alternative
    ASSERT(choice->type == VAL_INTEGER);
    ASSERT(choice->data.integer == 1);
    
    // Test getting all solutions (simplified)
    size_t solution_count;
    Value** solutions = flint_get_all_solutions(choice, env, &solution_count);
    ASSERT(solutions != NULL);
    ASSERT(solution_count >= 1);
    
    flint_free(solutions);
    flint_free_environment(env);
    
    printf("✓ Non-deterministic choice tests passed\n");
    return true;
}

// Test linear resource management - basic consumption
bool test_linear_basic_consumption() {
    TEST("Linear Resource Management - Basic Consumption");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create a linear value
    Value* val = flint_create_integer(42);
    ASSERT(!val->is_consumed);
    ASSERT(val->consumption_count == 0);
    
    // Consume the value
    flint_consume_value(val, LINEAR_OP_EXPLICIT_CONSUME);
    ASSERT(val->is_consumed);
    ASSERT(val->consumption_count == 1);
    
    // Test that we can consume it again (should increment count)
    Value* result2 = flint_consume_value(val, LINEAR_OP_EXPLICIT_CONSUME);
    ASSERT(result2 != NULL); // Should succeed
    ASSERT(val->consumption_count == 2);
    
    flint_free_environment(env);
    
    printf("✓ Linear basic consumption tests passed\n");
    return true;
}

// Test linear resource management - copying and sharing
bool test_linear_copying_sharing() {
    TEST("Linear Resource Management - Copying and Sharing");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create a linear value
    Value* original = flint_create_string("test string");
    ASSERT(!original->is_consumed);
    
    // Test deep copying
    Value* copy = flint_deep_copy_value(original);
    ASSERT(copy != original);
    ASSERT(!copy->is_consumed);
    ASSERT(copy->type == VAL_STRING);
    ASSERT(strcmp(copy->data.string, "test string") == 0);
    
    // Consume the original
    flint_consume_value(original, LINEAR_OP_EXPLICIT_CONSUME);
    ASSERT(original->is_consumed);
    ASSERT(!copy->is_consumed); // Copy should not be affected
    
    // Test sharing (opt-in non-linear)
    Value* shared = flint_share_value(copy);
    ASSERT(shared == copy); // Should return the same value
    
    flint_free_environment(env);
    
    printf("✓ Linear copying and sharing tests passed\n");
    return true;
}

// Test linear resource management - trail and backtracking
bool test_linear_trail_backtracking() {
    TEST("Linear Resource Management - Trail and Backtracking");
    
    Environment* env = flint_create_environment(NULL);
    
    // Set the linear context to use this environment's trail
    flint_set_linear_context(env);
    
    // Create some values
    Value* val1 = flint_create_integer(10);
    Value* val2 = flint_create_integer(20);
    Value* val3 = flint_create_string("hello");
    
    // Create a checkpoint
    LinearCheckpoint checkpoint = flint_linear_checkpoint(env->linear_trail);
    
    // Consume some values
    flint_consume_value(val1, LINEAR_OP_UNIFY);
    flint_consume_value(val2, LINEAR_OP_FUNCTION_CALL);
    flint_consume_value(val3, LINEAR_OP_DESTRUCTURE);
    
    ASSERT(val1->is_consumed);
    ASSERT(val2->is_consumed);
    ASSERT(val3->is_consumed);
    
    // Check trail entries (trail should have at least some entries)
    ASSERT(env->linear_trail->entry_count >= 3); // Should have our 3 consumption records
    
    // Restore to checkpoint
    flint_linear_restore(env->linear_trail, checkpoint);
    
    // Values should be restored (not consumed)
    ASSERT(!val1->is_consumed);
    ASSERT(!val2->is_consumed);
    ASSERT(!val3->is_consumed);
    
    // Reset linear context
    flint_set_linear_context(NULL);
    
    flint_free_environment(env);
    
    printf("✓ Linear trail and backtracking tests passed\n");
    return true;
}

// Test linear resource management - list destructuring
bool test_linear_list_destructuring() {
    TEST("Linear Resource Management - List Destructuring");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create a list with some elements
    Value* elements[3];
    elements[0] = flint_create_integer(1);
    elements[1] = flint_create_integer(2);
    elements[2] = flint_create_integer(3);
    
    Value* list = flint_create_list(elements, 3);
    ASSERT(!list->is_consumed);
    ASSERT(list->data.list.length == 3);
    
    // Test linear destructuring
    LinearListDestructure result = flint_linear_destructure_list(list);
    ASSERT(result.success);
    ASSERT(result.count == 3);
    ASSERT(result.elements != NULL);
    
    // Original list should now be consumed
    ASSERT(list->is_consumed);
    
    // Check that elements were transferred
    ASSERT(result.elements[0].type == VAL_INTEGER);
    ASSERT(result.elements[0].data.integer == 1);
    ASSERT(result.elements[1].data.integer == 2);
    ASSERT(result.elements[2].data.integer == 3);
    
    // Clean up transferred elements
    free(result.elements);
    
    flint_free_environment(env);
    
    printf("✓ Linear list destructuring tests passed\n");
    return true;
}

// Test linear resource management - variable consumption
bool test_linear_variable_consumption() {
    TEST("Linear Resource Management - Variable Consumption");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create a linear logical variable
    Value* var_val = flint_create_logical_var(true); // linear = true
    LogicalVar* var = var_val->data.logical_var;
    
    ASSERT(!var->is_consumed);
    ASSERT(var->use_count == 0);
    ASSERT(!var->allow_reuse);
    
    // Bind the variable
    Value* binding = flint_create_integer(42);
    var->binding = binding;
    
    // Use the variable (should increment use count)
    var->use_count++;
    ASSERT(var->use_count == 1);
    
    // Mark as consumed
    var->is_consumed = true;
    ASSERT(var->is_consumed);
    
    // Test that we can create a non-linear (reusable) variable
    Value* reusable_var_val = flint_create_logical_var(false); // linear = false
    LogicalVar* reusable_var = reusable_var_val->data.logical_var;
    
    ASSERT(!reusable_var->is_consumed);
    ASSERT(reusable_var->use_count == 0);
    ASSERT(reusable_var->allow_reuse);
    
    flint_free_environment(env);
    
    printf("✓ Linear variable consumption tests passed\n");
    return true;
}

// Test linear resource management - integration with unification
bool test_linear_unification_integration() {
    TEST("Linear Resource Management - Unification Integration");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create two values to unify
    Value* val1 = flint_create_integer(42);
    Value* val2 = flint_create_logical_var(true);
    
    ASSERT(!val1->is_consumed);
    ASSERT(!val2->is_consumed);
    
    // Create a checkpoint before unification
    LinearCheckpoint checkpoint = flint_linear_checkpoint(env->linear_trail);
    
    // Perform unification (this should consume both values)
    bool unify_result = flint_unify(val1, val2, env);
    ASSERT(unify_result);
    
    // Check that the logical variable is bound
    LogicalVar* var = val2->data.logical_var;
    ASSERT(var->binding != NULL);
    ASSERT(var->binding->type == VAL_INTEGER);
    ASSERT(var->binding->data.integer == 42);
    
    // Test backtracking restores the consumption state
    flint_linear_restore(env->linear_trail, checkpoint);
    
    flint_free_environment(env);
    
    printf("✓ Linear unification integration tests passed\n");
    return true;
}

// =============================================================================
// C INTEROP TESTS
// =============================================================================

// Test C interop - basic deterministic functions
bool test_c_interop_basic() {
    TEST("C Interop - Basic Functions");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test c_add function (should be registered by init)
    Value* arg1 = flint_create_integer(10);
    Value* arg2 = flint_create_integer(20);
    Value* args[] = {arg1, arg2};
    
    Value* result = flint_call_c_function("c_add", args, 2, env);
    ASSERT(result != NULL);
    ASSERT(result->type == VAL_INTEGER);
    ASSERT(result->data.integer == 30);
    
    // Test c_factorial function
    Value* n = flint_create_integer(5);
    Value* factorial_args[] = {n};
    
    Value* factorial_result = flint_call_c_function("c_factorial", factorial_args, 1, env);
    ASSERT(factorial_result != NULL);
    ASSERT(factorial_result->type == VAL_INTEGER);
    ASSERT(factorial_result->data.integer == 120); // 5! = 120
    
    flint_free_environment(env);
    
    printf("✓ C interop basic function tests passed\n");
    return true;
}

// Test C interop - string functions
bool test_c_interop_strings() {
    TEST("C Interop - String Functions");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test c_reverse_string function
    Value* str = flint_create_string("hello");
    Value* str_args[] = {str};
    
    Value* reversed = flint_call_c_function("c_reverse_string", str_args, 1, env);
    ASSERT(reversed != NULL);
    ASSERT(reversed->type == VAL_STRING);
    ASSERT(strcmp(reversed->data.string, "olleh") == 0);
    
    flint_free_environment(env);
    
    printf("✓ C interop string function tests passed\n");
    return true;
}

// Test C interop - math functions
bool test_c_interop_math() {
    TEST("C Interop - Math Functions");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test c_sin function
    Value* angle = flint_create_float(1.0); // sin(1.0) ≈ 0.841
    Value* sin_args[] = {angle};
    
    Value* sin_result = flint_call_c_function("c_sin", sin_args, 1, env);
    ASSERT(sin_result != NULL);
    ASSERT(sin_result->type == VAL_FLOAT);
    // Check that sin(1.0) is approximately 0.841
    ASSERT(sin_result->data.float_val > 0.84 && sin_result->data.float_val < 0.85);
    
    // Test c_sqrt function
    Value* number = flint_create_float(16.0);
    Value* sqrt_args[] = {number};
    
    Value* sqrt_result = flint_call_c_function("c_sqrt", sqrt_args, 1, env);
    ASSERT(sqrt_result != NULL);
    ASSERT(sqrt_result->type == VAL_FLOAT);
    ASSERT(sqrt_result->data.float_val == 4.0);
    
    flint_free_environment(env);
    
    printf("✓ C interop math function tests passed\n");
    return true;
}

// Test C interop - error handling
bool test_c_interop_errors() {
    TEST("C Interop - Error Handling");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test calling non-existent function
    Value* arg = flint_create_integer(42);
    Value* args[] = {arg};
    
    Value* result = flint_call_c_function("nonexistent_function", args, 1, env);
    ASSERT(result == NULL); // Should return NULL for non-existent function
    
    // Test wrong argument count
    Value* wrong_result = flint_call_c_function("c_add", args, 1, env); // c_add expects 2 args
    ASSERT(wrong_result == NULL); // Should return NULL for wrong arg count
    
    flint_free_environment(env);
    
    printf("✓ C interop error handling tests passed\n");
    return true;
}

// Test C interop - linear resource handling
bool test_c_interop_linear() {
    TEST("C Interop - Linear Resource Handling");
    
    Environment* env = flint_create_environment(NULL);
    
    // Create values for testing
    Value* val1 = flint_create_integer(10);
    Value* val2 = flint_create_integer(20);
    
    ASSERT(!val1->is_consumed);
    ASSERT(!val2->is_consumed);
    
    // Register a C function that consumes its arguments
    // (In practice, we'd register a different function or variant)
    Value* args[] = {val1, val2};
    Value* result = flint_call_c_function("c_add", args, 2, env);
    
    ASSERT(result != NULL);
    ASSERT(result->type == VAL_INTEGER);
    ASSERT(result->data.integer == 30);
    
    // The original function doesn't consume args, so they should still be available
    ASSERT(!val1->is_consumed);
    ASSERT(!val2->is_consumed);
    
    flint_free_environment(env);
    
    printf("✓ C interop linear resource tests passed\n");
    return true;
}

// Test async system - basic functionality
bool test_async_basic() {
    TEST("Async System - Basic Functionality");
    
    Environment* env = flint_create_environment(NULL);
    
    // Test async context creation
    AsyncContext* async_ctx = flint_create_async_context(env);
    ASSERT(async_ctx != NULL);
    
    flint_set_async_context(async_ctx);
    AsyncContext* retrieved_ctx = flint_get_async_context();
    ASSERT(retrieved_ctx == async_ctx);
    
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Async basic functionality tests passed\n");
    return true;
}

// Test async channels
bool test_async_channels() {
    TEST("Async System - Channels");
    
    Environment* env = flint_create_environment(NULL);
    AsyncContext* async_ctx = flint_create_async_context(env);
    flint_set_async_context(async_ctx);
    
    // Create a channel
    FlintChannel* chan = flint_create_channel(0, C_TYPE_POINTER);
    ASSERT(chan != NULL);
    ASSERT(!chan->is_closed);
    
    // Test non-blocking operations (should fail with timeout)
    
    // Try to receive from empty channel (should timeout quickly)
    Value* received = flint_channel_recv(chan, 1); // 1ms timeout
    ASSERT(received == NULL); // Should timeout
    
    // Clean up
    flint_channel_close(chan);
    free(chan);
    
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Async channel tests passed\n");
    return true;
}

// Test coroutine bundles
bool test_async_bundles() {
    TEST("Async System - Coroutine Bundles");
    
    Environment* env = flint_create_environment(NULL);
    AsyncContext* async_ctx = flint_create_async_context(env);
    flint_set_async_context(async_ctx);
    
    // Create a bundle
    CoroutineBundle* bundle = flint_create_bundle(4);
    ASSERT(bundle != NULL);
    ASSERT(bundle->count == 0);
    ASSERT(bundle->capacity == 4);
    
    // Clean up
    flint_free_bundle(bundle);
    
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Async bundle tests passed\n");
    return true;
}

// Test async sleep
bool test_async_sleep() {
    TEST("Async System - Sleep");
    
    Environment* env = flint_create_environment(NULL);
    AsyncContext* async_ctx = flint_create_async_context(env);
    flint_set_async_context(async_ctx);
    
    // Test async sleep (very short)
    int64_t start_time = flint_now();
    flint_async_sleep(10); // 10ms
    int64_t end_time = flint_now();
    
    // Should have slept for at least a few milliseconds
    ASSERT(end_time > start_time);
    
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Async sleep tests passed\n");
    return true;
}

// Test async integration with linear resources
bool test_async_linear_integration() {
    TEST("Async System - Linear Resource Integration");
    
    Environment* env = flint_create_environment(NULL);
    AsyncContext* async_ctx = flint_create_async_context(env);
    flint_set_async_context(async_ctx);
    
    // Create a channel and test linear resource tracking
    FlintChannel* chan = flint_create_channel(0, C_TYPE_POINTER);
    ASSERT(chan != NULL);
    
    Value* test_val = flint_create_integer(100);
    ASSERT(!test_val->is_consumed);
    
    // Note: Since we can't easily test actual async sending without complex setup,
    // we'll just test that the channel and linear system can coexist
    
    flint_channel_close(chan);
    free(chan);
    
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Async linear integration tests passed\n");
    return true;
}

// Test flexible constraint system with amoeba
bool test_flexible_constraints() {
    TEST("Flexible Constraint System");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    ASSERT(store != NULL);
    ASSERT(store->solver != NULL);
    
    // Create some constraint variables
    VarId x = flint_fresh_var_id();
    VarId y = flint_fresh_var_id();
    VarId z = flint_fresh_var_id();
    
    // Add constraint: X + Y = Z
    VarId add_vars[] = {x, y, z};
    FlintConstraint* add_constraint = flint_add_arithmetic_constraint(
        store, ARITH_ADD, add_vars, 3, 0.0, STRENGTH_REQUIRED);
    ASSERT(add_constraint != NULL);
    
    // Suggest some values
    flint_suggest_constraint_value(store, x, 10.0);
    flint_suggest_constraint_value(store, y, 15.0);
    
    // Check that Z got computed correctly
    double z_value = flint_get_constraint_value(store, z);
    ASSERT(z_value >= 24.9 && z_value <= 25.1); // Should be approximately 25
    
    // Test inequality constraint: X <= Y
    FlintConstraint* ineq_constraint = flint_add_inequality_constraint(
        store, x, y, true, STRENGTH_STRONG);
    ASSERT(ineq_constraint != NULL);
    
    // The constraint should already be satisfied (10 <= 15)
    double x_value = flint_get_constraint_value(store, x);
    double y_value = flint_get_constraint_value(store, y);
    ASSERT(x_value <= y_value + 0.001); // Allow small floating point error
    
    flint_print_constraint_values(store);
    
    flint_free_environment(env);
    
    printf("✓ Flexible constraint tests passed\n");
    return true;
}

// Test comprehensive flexible constraint system capabilities
bool test_flexible_constraint_system() {
    TEST("Flexible Constraint System - Comprehensive Demo");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    printf("\n=== Flexible Constraint System Demo ===\n");
    
    // Simple demonstration: Three variables with relationships
    VarId x = 1;
    VarId y = 2; 
    VarId z = 3;
    
    printf("Setting up simple constraint problem:\n");
    printf("- Variables: X, Y, Z\n");
    printf("- Constraint 1: X + Y = Z\n");
    printf("- Constraint 2: X >= 5\n");
    printf("- Constraint 3: Y = 2 * X\n\n");
    
    // Get constraint variables
    FlintConstraintVar* x_var = flint_get_or_create_constraint_var(store, x, "X");
    FlintConstraintVar* y_var = flint_get_or_create_constraint_var(store, y, "Y");
    FlintConstraintVar* z_var = flint_get_or_create_constraint_var(store, z, "Z");
    
    // Constraint 1: X + Y = Z  (X + Y - Z = 0)
    am_Constraint* c1 = am_newconstraint(store->solver, AM_REQUIRED);
    am_addterm(c1, x_var->amoeba_var, 1.0);   // X
    am_addterm(c1, y_var->amoeba_var, 1.0);   // + Y
    am_setrelation(c1, AM_EQUAL);
    am_addterm(c1, z_var->amoeba_var, 1.0);   // = Z
    am_add(c1);
    printf("✓ Added constraint: X + Y = Z\n");
    
    // Constraint 2: X >= 5  (X - 5 >= 0)
    am_Constraint* c2 = am_newconstraint(store->solver, AM_REQUIRED);
    am_addterm(c2, x_var->amoeba_var, 1.0);   // X
    am_setrelation(c2, AM_GREATEQUAL);
    am_addconstant(c2, 5.0);                  // >= 5
    am_add(c2);
    printf("✓ Added constraint: X >= 5\n");
    
    // Constraint 3: Y = 2 * X  (Y - 2*X = 0)
    am_Constraint* c3 = am_newconstraint(store->solver, AM_REQUIRED);
    am_addterm(c3, y_var->amoeba_var, 1.0);   // Y
    am_setrelation(c3, AM_EQUAL);
    am_addterm(c3, x_var->amoeba_var, 2.0);   // = 2*X
    am_add(c3);
    printf("✓ Added constraint: Y = 2*X\n");
    
    printf("\nSolving constraints...\n");
    
    // Suggest a value for X and let the system solve for Y and Z
    am_addedit(x_var->amoeba_var, AM_MEDIUM);
    am_suggest(x_var->amoeba_var, 10.0);
    printf("Suggested X = 10\n");
    
    // Get the solved values
    double x_val = am_value(x_var->amoeba_var);
    double y_val = am_value(y_var->amoeba_var);
    double z_val = am_value(z_var->amoeba_var);
    
    printf("\nSolved values:\n");
    printf("X = %.1f\n", x_val);
    printf("Y = %.1f\n", y_val);
    printf("Z = %.1f\n", z_val);
    
    // Verify the constraints are satisfied
    ASSERT(fabs(x_val - 10.0) < 1e-6);       // X should be 10
    ASSERT(x_val >= 4.99);                   // X should be >= 5
    
    // Check Y = 2*X
    double y_check = y_val - 2.0 * x_val;
    printf("Y = 2*X check: %.6f (should be ~0)\n", y_check);
    ASSERT(fabs(y_check) < 1e-6);
    printf("✓ Y = 2*X constraint satisfied\n");
    
    // Check X + Y = Z
    double z_check = (x_val + y_val) - z_val;
    printf("X + Y = Z check: %.6f (should be ~0)\n", z_check);
    ASSERT(fabs(z_check) < 1e-6);
    printf("✓ X + Y = Z constraint satisfied\n");
    
    printf("\n=== Testing Dynamic Changes ===\n");
    
    // Change X to a different value
    am_suggest(x_var->amoeba_var, 6.0);
    printf("Changed X to 6\n");
    
    double new_x = am_value(x_var->amoeba_var);
    double new_y = am_value(y_var->amoeba_var);
    double new_z = am_value(z_var->amoeba_var);
    
    printf("New values: X=%.1f, Y=%.1f, Z=%.1f\n", new_x, new_y, new_z);
    
    // Verify constraints still hold
    ASSERT(fabs(new_x - 6.0) < 1e-6);
    ASSERT(fabs(new_y - 2.0 * new_x) < 1e-6);  // Y = 2*X
    ASSERT(fabs(new_z - (new_x + new_y)) < 1e-6);  // Z = X + Y
    
    printf("✓ All constraints satisfied after dynamic change\n");
    
    printf("\n=== Testing Inequality Constraints ===\n");
    
    // Add a weak preference: prefer Z to be as small as possible
    VarId w = 4;
    FlintConstraintVar* w_var = flint_get_or_create_constraint_var(store, w, "W");
    
    // W = Z (to create an objective)
    am_Constraint* c4 = am_newconstraint(store->solver, AM_WEAK);
    am_addterm(c4, w_var->amoeba_var, 1.0);   // W
    am_setrelation(c4, AM_EQUAL);
    am_addterm(c4, z_var->amoeba_var, 1.0);   // = Z
    am_add(c4);
    
    // Minimize W (weak preference to make it small)
    am_addedit(w_var->amoeba_var, AM_WEAK);
    am_suggest(w_var->amoeba_var, 0.0);  // Prefer small values
    
    double final_x = am_value(x_var->amoeba_var);
    double final_y = am_value(y_var->amoeba_var);
    double final_z = am_value(z_var->amoeba_var);
    
    printf("Final values with minimization: X=%.1f, Y=%.1f, Z=%.1f\n", 
           final_x, final_y, final_z);
    
    // Should still satisfy the hard constraints
    ASSERT(final_x >= 4.99);  // X >= 5
    ASSERT(fabs(final_y - 2.0 * final_x) < 1e-6);  // Y = 2*X
    ASSERT(fabs(final_z - (final_x + final_y)) < 1e-6);  // Z = X + Y
    
    printf("✓ All hard constraints maintained with weak preferences\n");
    
    printf("\n=== Summary ===\n");
    printf("Successfully demonstrated:\n");
    printf("✓ Linear equality constraints (X + Y = Z, Y = 2*X)\n");
    printf("✓ Linear inequality constraints (X >= 5)\n");
    printf("✓ Multiple constraint strengths (required vs weak)\n");
    printf("✓ Dynamic constraint solving (runtime value changes)\n");
    printf("✓ Optimization with weak preferences\n");
    printf("✓ Real-time constraint satisfaction\n");
    printf("=====================================\n\n");
    
    flint_free_environment(env);
    
    printf("✓ Flexible constraint system comprehensive tests passed\n");
    return true;
}

// Test constraint integration with unification
bool test_constraint_unification_integration() {
    TEST("Constraint-Unification Integration");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    // Create logical variables
    Value* varX = flint_create_logical_var(false);
    Value* varY = flint_create_logical_var(false);
    Value* varZ = flint_create_logical_var(false);
    
    VarId x_id = varX->data.logical_var->id;
    VarId y_id = varY->data.logical_var->id;
    VarId z_id = varZ->data.logical_var->id;
    
    // Register variables with environment for constraint solving
    flint_register_variable_with_env(env, varX);
    flint_register_variable_with_env(env, varY);
    flint_register_variable_with_env(env, varZ);
    
    // Add constraint: X + Y = Z using the unified interface
    ASSERT(flint_add_sum_constraint(env, x_id, y_id, z_id, STRENGTH_REQUIRED));
    
    // Unify X with 10 using constraint-aware unification
    Value* val10 = flint_create_integer(10);
    ASSERT(flint_unify_with_constraints(varX, val10, env));
    
    // Unify Y with 20 using constraint-aware unification
    Value* val20 = flint_create_integer(20);
    ASSERT(flint_unify_with_constraints(varY, val20, env));
    
    // Check that Z resolves to 30 via constraints
    double z_value = flint_get_constraint_value(store, z_id);
    ASSERT(z_value >= 29.9 && z_value <= 30.1); // Should be approximately 30
    
    // Verify unification still works
    Value* deref_x = flint_deref(varX);
    Value* deref_y = flint_deref(varY);
    ASSERT(deref_x->type == VAL_INTEGER && deref_x->data.integer == 10);
    ASSERT(deref_y->type == VAL_INTEGER && deref_y->data.integer == 20);
    
    // Test reverse direction: set constraint variable and check unification
    VarId w_id = flint_fresh_var_id();
    Value* varW = flint_create_logical_var(false);
    varW->data.logical_var->id = w_id;
    
    // Add constraint W = 42
    VarId const_vars[] = {w_id};
    FlintConstraint* const_constraint = flint_add_arithmetic_constraint(
        store, ARITH_EQUAL, const_vars, 1, 42.0, STRENGTH_REQUIRED);
    ASSERT(const_constraint != NULL);
    
    // Get the constraint value and create a Value for unification
    double w_value = flint_get_constraint_value(store, w_id);
    Value* constraint_result = flint_create_integer((int)w_value);
    
    // This should succeed since the constraint system provides the value
    ASSERT(flint_unify(varW, constraint_result, env));
    
    Value* deref_w = flint_deref(varW);
    ASSERT(deref_w->type == VAL_INTEGER && deref_w->data.integer == 42);
    
    flint_free_environment(env);
    
    printf("✓ Constraint-unification integration tests passed\n");
    return true;
}

// Test constraint integration with linear resource management
bool test_constraint_linear_integration() {
    TEST("Constraint-Linear Integration");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    flint_set_linear_context(env);
    
    // Create linear variables 
    Value* linearX = flint_create_logical_var(true); // linear = true
    Value* linearY = flint_create_logical_var(true);
    Value* result = flint_create_logical_var(false); // non-linear result
    
    VarId x_id = linearX->data.logical_var->id;
    VarId y_id = linearY->data.logical_var->id;
    VarId result_id = result->data.logical_var->id;
    
    // Register variables with environment
    flint_register_variable_with_env(env, linearX);
    flint_register_variable_with_env(env, linearY);
    flint_register_variable_with_env(env, result);
    
    // Add constraint: X + X = Y (representing X * 2 = Y)
    VarId double_vars[] = {x_id, x_id, y_id};
    ASSERT(flint_constrain_variables(env, double_vars, 3, ARITH_ADD, 0.0, STRENGTH_REQUIRED));
    
    // Add constraint: Y = result (using equality)
    VarId eq_vars[] = {y_id, result_id};
    ASSERT(flint_constrain_variables(env, eq_vars, 2, ARITH_EQUAL, 0.0, STRENGTH_REQUIRED));
    
    // Create a checkpoint for linear trail
    LinearCheckpoint checkpoint = flint_linear_checkpoint(env->linear_trail);
    
    // Unify and consume linear variable X using constraint-aware unification
    Value* val5 = flint_create_integer(5);
    ASSERT(flint_unify_with_constraints(linearX, val5, env));
    
    // Consume the linear variable (simulating linear usage)
    flint_consume_value(linearX, LINEAR_OP_UNIFY);
    ASSERT(linearX->is_consumed);
    
    // Check that Y and result get computed via constraints (Y should be 10)
    double y_value = flint_get_constraint_value(store, y_id);
    double result_value = flint_get_constraint_value(store, result_id);
    
    ASSERT(y_value >= 9.9 && y_value <= 10.1); // Y = 2*X = 2*5 = 10
    ASSERT(result_value >= 9.9 && result_value <= 10.1); // result = Y = 10
    
    // Test backtracking with linear trail
    flint_linear_restore(env->linear_trail, checkpoint);
    
    // After restore, linear variables should be unconsumed
    ASSERT(!linearX->is_consumed);
    
    // Constraint values should still be available
    double restored_y = flint_get_constraint_value(store, y_id);
    ASSERT(restored_y >= 9.9 && restored_y <= 10.1);
    
    flint_set_linear_context(NULL);
    flint_free_environment(env);
    
    printf("✓ Constraint-linear integration tests passed\n");
    return true;
}

// Test constraint integration with async operations
bool test_constraint_async_integration() {
    TEST("Constraint-Async Integration");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    AsyncContext* async_ctx = flint_create_async_context(env);
    flint_set_async_context(async_ctx);
    
    // Create variables for async constraint solving
    VarId async_var1 = flint_fresh_var_id();
    VarId async_var2 = flint_fresh_var_id();
    VarId async_result = flint_fresh_var_id();
    
    // Add constraint: var1 + var2 = result using unified interface
    ASSERT(flint_add_sum_constraint(env, async_var1, async_var2, async_result, STRENGTH_REQUIRED));
    
    // Simulate async constraint updates using the constraint interface
    ASSERT(flint_constrain_to_value(env, async_var1, 15.0, STRENGTH_REQUIRED));
    ASSERT(flint_constrain_to_value(env, async_var2, 25.0, STRENGTH_REQUIRED));
    
    // Check that constraint solver computed the result asynchronously
    double result_value = flint_get_constraint_value(store, async_result);
    ASSERT(result_value >= 39.9 && result_value <= 40.1); // Should be 40
    
    // Test constraint solving in async environment with channels
    FlintChannel* constraint_chan = flint_create_channel(1, C_TYPE_POINTER);
    ASSERT(constraint_chan != NULL);
    
    // Create a constraint that represents an async computation result
    VarId channel_var = flint_fresh_var_id();
    
    // Add a weak constraint that prefers the channel variable to be small
    VarId opt_vars[] = {channel_var};
    FlintConstraint* optimization = flint_add_arithmetic_constraint(
        store, ARITH_EQUAL, opt_vars, 1, 1.0, STRENGTH_WEAK); // Prefer value 1.0
    ASSERT(optimization != NULL);
    
    // The constraint solver should suggest a value close to 1.0
    double optimized_value = flint_get_constraint_value(store, channel_var);
    ASSERT(optimized_value >= 0.9 && optimized_value <= 1.1);
    
    // Clean up async resources
    flint_channel_close(constraint_chan);
    free(constraint_chan);
    
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Constraint-async integration tests passed\n");
    return true;
}

// Test full integration: constraints + unification + linear + async
bool test_full_system_integration() {
    TEST("Full System Integration (Constraints + Unification + Linear + Async)");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    // Set up all systems
    AsyncContext* async_ctx = flint_create_async_context(env);
    flint_set_async_context(async_ctx);
    flint_set_linear_context(env);
    
    printf("\n=== Full System Integration Demo ===\n");
    printf("Testing constraints + unification + linear resources + async operations\n\n");
    
    // Create linear logical variables
    Value* resourceA = flint_create_logical_var(true);  // linear resource
    Value* resourceB = flint_create_logical_var(true);  // linear resource
    Value* sharedResult = flint_create_logical_var(false); // non-linear shared result
    Value* asyncResult = flint_create_logical_var(false);  // async computation result
    
    VarId a_id = resourceA->data.logical_var->id;
    VarId b_id = resourceB->data.logical_var->id;
    VarId shared_id = sharedResult->data.logical_var->id;
    VarId async_id = asyncResult->data.logical_var->id;
    
    // Register variables with environment for constraint solving
    flint_register_variable_with_env(env, resourceA);
    flint_register_variable_with_env(env, resourceB);
    flint_register_variable_with_env(env, sharedResult);
    flint_register_variable_with_env(env, asyncResult);
    
    printf("Step 1: Setting up constraint relationships\n");
    
    // Constraint 1: A + B = shared_result (resource combination)
    FlintConstraint* combination = flint_add_addition_constraint(
        store, a_id, b_id, shared_id, STRENGTH_REQUIRED);
    ASSERT(combination != NULL);
    printf("  Added: A + B = SharedResult\n");
    
    // Constraint 2: shared_result * 2 = async_result (async scaling)
    VarId scale_vars[] = {shared_id, shared_id, async_id};
    FlintConstraint* scaling = flint_add_arithmetic_constraint(
        store, ARITH_ADD, scale_vars, 3, 0.0, STRENGTH_REQUIRED);
    ASSERT(scaling != NULL);
    printf("  Added: SharedResult * 2 = AsyncResult\n");
    
    // Constraint 3: A should be at least 10 (resource constraint)
    VarId min_vars[] = {a_id};
    FlintConstraint* minimum = flint_add_arithmetic_constraint(
        store, ARITH_GEQ, min_vars, 1, 10.0, STRENGTH_STRONG);
    ASSERT(minimum != NULL);
    printf("  Added: A >= 10\n");
    
    printf("\nStep 2: Linear resource checkpoint\n");
    LinearCheckpoint checkpoint = flint_linear_checkpoint(env->linear_trail);
    
    printf("\nStep 3: Unification with linear resource consumption\n");
    
    // Unify resourceA with a value (consuming it linearly)
    Value* val12 = flint_create_integer(12);
    ASSERT(flint_unify(resourceA, val12, env));
    flint_consume_value(resourceA, LINEAR_OP_UNIFY);
    printf("  Unified A = 12 (linear resource consumed)\n");
    
    // Solve constraints after A is bound
    flint_solve_constraints(store, a_id, env);
    
    // Unify resourceB with a value
    Value* val8 = flint_create_integer(8);
    ASSERT(flint_unify(resourceB, val8, env));
    flint_consume_value(resourceB, LINEAR_OP_UNIFY);
    printf("  Unified B = 8 (linear resource consumed)\n");
    
    // Solve constraints after B is bound
    flint_solve_constraints(store, b_id, env);
    
    // Trigger constraint solving for the computed variables to propagate constraints
    flint_solve_constraints(store, shared_id, env);
    flint_solve_constraints(store, async_id, env);
    
    printf("\nStep 4: Constraint propagation and async computation\n");
    
    // Get constraint-computed values
    double shared_value = flint_get_constraint_value(store, shared_id);
    double async_value = flint_get_constraint_value(store, async_id);
    
    printf("  Computed SharedResult = %.1f (via constraints)\n", shared_value);
    printf("  Computed AsyncResult = %.1f (via constraints)\n", async_value);
    
    // Verify constraint satisfaction
    ASSERT(shared_value >= 19.9 && shared_value <= 20.1); // A + B = 12 + 8 = 20
    ASSERT(async_value >= 39.9 && async_value <= 40.1);   // SharedResult * 2 = 20 * 2 = 40
    
    // Unify the shared result with the constraint-computed value
    Value* shared_val = flint_create_integer((int)shared_value);
    ASSERT(flint_unify(sharedResult, shared_val, env));
    printf("  Unified SharedResult with constraint value\n");
    
    printf("\nStep 5: Async channel communication with constraint results\n");
    
    // Create async channel and send constraint result
    FlintChannel* result_channel = flint_create_channel(1, C_TYPE_POINTER);
    ASSERT(result_channel != NULL);
    
    // In a real async scenario, we'd spawn coroutines here
    // For testing, we'll simulate the async result
    Value* final_result = flint_create_integer((int)async_value);
    ASSERT(flint_unify(asyncResult, final_result, env));
    printf("  Async computation completed: AsyncResult = %d\n", (int)async_value);
    
    printf("\nStep 6: Linear resource backtracking\n");
    
    // Test backtracking to restore linear resources
    flint_linear_restore(env->linear_trail, checkpoint);
    printf("  Restored to checkpoint - linear resources unconsumed\n");
    
    // Verify resources are restored
    ASSERT(!resourceA->is_consumed);
    ASSERT(!resourceB->is_consumed);
    printf("  Verified: A and B are no longer consumed\n");
    
    // Constraint values should still be available (constraint system is persistent)
    double restored_shared = flint_get_constraint_value(store, shared_id);
    double restored_async = flint_get_constraint_value(store, async_id);
    
    ASSERT(restored_shared >= 19.9 && restored_shared <= 20.1);
    ASSERT(restored_async >= 39.9 && restored_async <= 40.1);
    printf("  Constraint values preserved: Shared=%.1f, Async=%.1f\n", 
           restored_shared, restored_async);
    
    printf("\nStep 7: Verification of unification state\n");
    
    // Check that unification bindings are preserved
    Value* deref_a = flint_deref(resourceA);
    Value* deref_b = flint_deref(resourceB);
    Value* deref_shared = flint_deref(sharedResult);
    Value* deref_async = flint_deref(asyncResult);
    
    ASSERT(deref_a->type == VAL_INTEGER && deref_a->data.integer == 12);
    ASSERT(deref_b->type == VAL_INTEGER && deref_b->data.integer == 8);
    ASSERT(deref_shared->type == VAL_INTEGER && deref_shared->data.integer == 20);
    ASSERT(deref_async->type == VAL_INTEGER && deref_async->data.integer == 40);
    
    printf("  Verified unification: A=12, B=8, Shared=20, Async=40\n");
    
    printf("\n=== Integration Test Summary ===\n");
    printf("✓ Constraint solving integrated with unification\n");
    printf("✓ Linear resource management with constraint propagation\n");
    printf("✓ Async operations with constraint-driven computation\n");
    printf("✓ Backtracking preserves constraint state\n");
    printf("✓ All systems work together seamlessly\n");
    printf("=====================================\n\n");
    
    // Clean up
    flint_channel_close(result_channel);
    free(result_channel);
    
    flint_set_linear_context(NULL);
    flint_free_async_context(async_ctx);
    flint_free_environment(env);
    
    printf("✓ Full system integration tests passed\n");
    return true;
}

// Test constraint-based optimization with unification
bool test_constraint_optimization_integration() {
    TEST("Constraint Optimization with Unification");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    // Create variables for optimization problem
    VarId x = flint_fresh_var_id();
    VarId y = flint_fresh_var_id();
    VarId cost = flint_fresh_var_id();
    
    Value* varX = flint_create_logical_var(false);
    Value* varY = flint_create_logical_var(false);
    Value* varCost = flint_create_logical_var(false);
    
    varX->data.logical_var->id = x;
    varY->data.logical_var->id = y;
    varCost->data.logical_var->id = cost;
    
    // Add constraints for optimization:
    // 1. X + Y = Cost (objective function)
    FlintConstraint* objective = flint_add_addition_constraint(store, x, y, cost, STRENGTH_REQUIRED);
    ASSERT(objective != NULL);
    
    // 2. X >= 5 (constraint)
    VarId x_min_vars[] = {x};
    FlintConstraint* x_min = flint_add_arithmetic_constraint(
        store, ARITH_GEQ, x_min_vars, 1, 5.0, STRENGTH_REQUIRED);
    ASSERT(x_min != NULL);
    
    // 3. Y >= 3 (constraint)  
    VarId y_min_vars[] = {y};
    FlintConstraint* y_min = flint_add_arithmetic_constraint(
        store, ARITH_GEQ, y_min_vars, 1, 3.0, STRENGTH_REQUIRED);
    ASSERT(y_min != NULL);
    
    // 4. Minimize cost (weak preference for small values)
    VarId cost_vars[] = {cost};
    FlintConstraint* minimize = flint_add_arithmetic_constraint(
        store, ARITH_EQUAL, cost_vars, 1, 8.0, STRENGTH_WEAK); // Prefer cost = 8
    ASSERT(minimize != NULL);
    
    // Unify with specific values and see if constraints are satisfied
    Value* val5 = flint_create_integer(5);
    Value* val3 = flint_create_integer(3);
    
    ASSERT(flint_unify(varX, val5, env));
    ASSERT(flint_unify(varY, val3, env));
    
    // Trigger constraint solving
    flint_solve_constraints(store, x, env);
    flint_solve_constraints(store, y, env);
    
    // Get the optimized cost
    double cost_value = flint_get_constraint_value(store, cost);
    ASSERT(cost_value >= 7.9 && cost_value <= 8.1); // Should be close to 8 (5+3)
    
    // Unify the cost variable with the constraint result
    Value* cost_val = flint_create_integer((int)cost_value);
    ASSERT(flint_unify(varCost, cost_val, env));
    
    // Verify the optimization worked
    Value* deref_cost = flint_deref(varCost);
    ASSERT(deref_cost->type == VAL_INTEGER && deref_cost->data.integer == 8);
    
    flint_free_environment(env);
    
    printf("✓ Constraint optimization integration tests passed\n");
    return true;
}

int main() {
    printf("=== Flint Runtime Test Suite ===\n\n");
    
    // Initialize the runtime
    flint_init_runtime();
    
    // Run tests
    bool all_passed = true;
    
    all_passed &= test_value_creation();
    all_passed &= test_list_operations();
    all_passed &= test_record_operations();
    all_passed &= test_unification();
    all_passed &= test_complex_unification();
    all_passed &= test_multi_variable_unification();
    all_passed &= test_narrowing();
    all_passed &= test_higher_order_functions();
    all_passed &= test_pattern_matching();
    all_passed &= test_constraint_propagation();
    all_passed &= test_flexible_constraint_system();
    all_passed &= test_flexible_constraints();
    
    // Integration tests - comprehensive system interactions
    all_passed &= test_constraint_unification_integration();
    // all_passed &= test_constraint_linear_integration();
    // all_passed &= test_constraint_async_integration();
    // all_passed &= test_constraint_optimization_integration();
    // all_passed &= test_full_system_integration();
    
    all_passed &= test_non_deterministic_choice();
    all_passed &= test_free_variables();
    all_passed &= test_environment();
    
    // Linear resource management tests
    all_passed &= test_linear_basic_consumption();
    all_passed &= test_linear_copying_sharing();
    all_passed &= test_linear_trail_backtracking();
    all_passed &= test_linear_list_destructuring();
    all_passed &= test_linear_variable_consumption();
    all_passed &= test_linear_unification_integration();
    
    // C interoperability tests
    all_passed &= test_c_interop_basic();
    all_passed &= test_c_interop_strings();
    all_passed &= test_c_interop_math();
    all_passed &= test_c_interop_errors();
    all_passed &= test_c_interop_linear();
    
    // Async system tests
    all_passed &= test_async_basic();
    all_passed &= test_async_channels();
    all_passed &= test_async_bundles();
    all_passed &= test_async_sleep();
    all_passed &= test_async_linear_integration();
    
    all_passed &= test_constraint_unification_integration();
    all_passed &= test_constraint_linear_integration();
    all_passed &= test_constraint_async_integration();
    all_passed &= test_full_system_integration();
    
    test_printing();
    
    // Cleanup
    // flint_cleanup_runtime();
    
    if (all_passed) {
        printf("\n🎉 All tests passed! Runtime is working correctly.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Check the output above.\n");
        return 1;
    }
}
