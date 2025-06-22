#include "runtime.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

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

// Test constraint propagation
bool test_constraint_propagation() {
    TEST("Constraint Propagation");
    
    Environment* env = flint_create_environment(NULL);
    ConstraintStore* store = flint_create_constraint_store();
    env->constraint_store = store;
    
    // Create two variables
    Value* var1 = flint_create_logical_var(false);
    Value* var2 = flint_create_logical_var(false);
    
    VarId var1_id = flint_get_logical_var(var1)->id;
    VarId var2_id = flint_get_logical_var(var2)->id;
    
    // Add variables to environment first
    flint_bind_variable(env, var1_id, var1);
    flint_bind_variable(env, var2_id, var2);
    
    // Add constraint that var1 == var2
    flint_add_constraint(store, var1_id, var2_id, CONSTRAINT_EQUAL, NULL);
    
    // Bind var1 to a value 
    Value* val = flint_create_integer(42);
    LogicalVar* var1_logical = flint_lookup_variable(env, var1_id);
    var1_logical->binding = val;  // Bind directly
    
    // Solve constraints - var2 should get bound to the same value
    ASSERT(flint_solve_constraints(store, var1_id, env));
    
    // Check that var2 got bound (need to dereference to get final value)
    LogicalVar* var2_binding = flint_lookup_variable(env, var2_id);
    ASSERT(var2_binding != NULL);
    ASSERT(var2_binding->binding != NULL);
    
    Value* final_val = flint_deref(var2_binding->binding);
    ASSERT(final_val->type == VAL_INTEGER);
    ASSERT(final_val->data.integer == 42);
    
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
    all_passed &= test_narrowing();
    all_passed &= test_higher_order_functions();
    all_passed &= test_pattern_matching();
    all_passed &= test_constraint_propagation();
    all_passed &= test_non_deterministic_choice();
    all_passed &= test_free_variables();
    all_passed &= test_environment();
    all_passed &= test_higher_order_functions();
    all_passed &= test_pattern_matching();
    all_passed &= test_complex_unification();
    all_passed &= test_constraint_propagation();
    all_passed &= test_non_deterministic_choice();
    
    test_printing();
    
    // Cleanup
    flint_cleanup_runtime();
    
    if (all_passed) {
        printf("\n🎉 All tests passed! Runtime is working correctly.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Check the output above.\n");
        return 1;
    }
}
