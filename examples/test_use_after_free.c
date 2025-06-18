#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "runtime.h"

// Logical Programming Runtime - types defined in header

// Linear Knowledge Base
linear_kb_t* kb;

void initialize_kb() {
    symbol_table_t* symbols = create_symbol_table();
    kb = create_linear_kb(symbols);
    set_auto_deallocation(kb, 1);  // Enable automatic deallocation

    // Linear Fact: has_item(_)
    term_t** term_array_0 = malloc(sizeof(term_t*) * 1);
    term_array_0[0] = create_atom(kb->symbols, "bread");
    term_t** fact_args_0 = term_array_0;
    term_t* fact_0 = create_compound(kb->symbols, "has_item", fact_args_0, 1);
    add_linear_fact(kb, fact_0);


    // Linear Fact: has_item(_)
    term_t** term_array_1 = malloc(sizeof(term_t*) * 1);
    term_array_1[0] = create_atom(kb->symbols, "bread");
    term_t** fact_args_1 = term_array_1;
    term_t* fact_1 = create_compound(kb->symbols, "has_item", fact_args_1, 1);
    add_linear_fact(kb, fact_1);


    // Rule: take_rule :- has_item(item)
    term_t* rule_head_2 = create_atom(kb->symbols, "take_rule");
    term_t** term_array_2 = malloc(sizeof(term_t*) * 1);
    term_t* args_3[1];
    args_3[0] = create_var_named(kb->symbols, "$item");
    term_array_2[0] = create_compound(kb->symbols, "has_item", args_3, 1);
    term_t** rule_body_2 = term_array_2;
    // Consumption point: rule_2_body_0
    term_t* args_4[2];
    args_4[0] = create_atom(kb->symbols, "alice");
    args_4[1] = create_var_named(kb->symbols, "$item");
    add_rule(kb, rule_head_2, rule_body_2, 1, create_compound(kb->symbols, "owns", args_4, 2));


    // Rule: double_take_rule :- has_item(item)
    term_t* rule_head_3 = create_atom(kb->symbols, "double_take_rule");
    term_t** term_array_5 = malloc(sizeof(term_t*) * 1);
    term_t* args_6[1];
    args_6[0] = create_var_named(kb->symbols, "$item");
    term_array_5[0] = create_compound(kb->symbols, "has_item", args_6, 1);
    term_t** rule_body_3 = term_array_5;
    // Consumption point: rule_3_body_0
    term_t* args_7[2];
    args_7[0] = create_atom(kb->symbols, "alice");
    args_7[1] = create_var_named(kb->symbols, "$item");
    add_rule(kb, rule_head_3, rule_body_3, 1, create_compound(kb->symbols, "double_owns", args_7, 2));


}

int main() {
    // Initialize linear knowledge base
    initialize_kb();

    add_type_mapping(kb, "bread", "item");
    add_type_mapping(kb, "alice", "customer");

    print_memory_state(kb, "INITIAL STATE - Before executing queries");

    // Query 1: 
    printf("?- ");
    term_t* args_8[2];
    args_8[0] = create_atom(kb->symbols, "alice");
    args_8[1] = create_atom(kb->symbols, "bread");
    print_term(create_compound(kb->symbols, "owns", args_8, 2), kb->symbols);
    printf(" & ");
    term_t* args_9[2];
    args_9[0] = create_atom(kb->symbols, "alice");
    args_9[1] = create_atom(kb->symbols, "bread");
    print_term(create_compound(kb->symbols, "double_owns", args_9, 2), kb->symbols);
    printf(".\n");
    term_t** goals_0 = malloc(2 * sizeof(term_t*));
    term_t* args_10[2];
    args_10[0] = create_atom(kb->symbols, "alice");
    args_10[1] = create_atom(kb->symbols, "bread");
    goals_0[0] = create_compound(kb->symbols, "owns", args_10, 2);
    term_t* args_11[2];
    args_11[0] = create_atom(kb->symbols, "alice");
    args_11[1] = create_atom(kb->symbols, "bread");
    goals_0[1] = create_compound(kb->symbols, "double_owns", args_11, 2);
    enhanced_solution_list_t* enhanced_solutions_0 = create_enhanced_solution_list();
    (void)linear_resolve_query_enhanced(kb, goals_0, 2, enhanced_solutions_0);
    if (enhanced_solutions_0->count > 0) {
        printf("true (%d solution%s found).\n", enhanced_solutions_0->count, enhanced_solutions_0->count == 1 ? "" : "s");
    } else {
        printf("false.\n");
    }
    free_enhanced_solution_list(enhanced_solutions_0);
    for (int i = 0; i < 2; i++) {
        free(goals_0[i]);
    }
    free(goals_0);

    // Print final memory state before cleanup
    print_memory_state(kb, "FINAL STATE - Before program exit");

    // Clean up
    free_linear_kb(kb);
    return 0;  // Always return success - false is a valid result
}
