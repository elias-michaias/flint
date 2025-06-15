#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "runtime.h"

// Logical Programming Runtime - types defined in header

// Linear Knowledge Base (empty)
linear_kb_t* kb;

void initialize_kb() {
    kb = create_linear_kb();
}

int main() {
    // Initialize linear knowledge base
    kb = create_linear_kb();

    add_type_mapping(kb, "alice", "person");
    add_type_mapping(kb, "bob", "person");
    add_type_mapping(kb, "carol", "person");
    add_type_mapping(kb, "dave", "person");

    add_linear_fact(kb, create_atom("alice"));
    add_linear_fact(kb, create_atom("bob"));
    add_linear_fact(kb, create_atom("carol"));
    add_linear_fact(kb, create_atom("dave"));
    term_t* args_0[2];
    args_0[0] = create_atom("alice");
    args_0[1] = create_atom("bob");
    add_linear_fact(kb, create_clone(create_compound("parent", args_0, 2)));
    term_t* args_1[2];
    args_1[0] = create_atom("bob");
    args_1[1] = create_atom("carol");
    add_linear_fact(kb, create_clone(create_compound("parent", args_1, 2)));
    term_t* args_2[2];
    args_2[0] = create_atom("carol");
    args_2[1] = create_atom("dave");
    add_linear_fact(kb, create_clone(create_compound("parent", args_2, 2)));
    term_t* args_3[1];
    args_3[0] = create_atom("dave");
    add_linear_fact(kb, create_clone(create_compound("tall", args_3, 1)));
    term_t** body_array_4 = malloc(sizeof(term_t*) * 1);
    term_t* args_5[2];
    args_5[0] = create_var("$x");
    args_5[1] = create_var("$y");
    body_array_4[0] = create_compound("parent", args_5, 2);
    term_t* args_6[2];
    args_6[0] = create_var("$x");
    args_6[1] = create_var("$y");
    add_rule(kb, create_atom("ancestor_base"), body_array_4, 1, create_compound("ancestor", args_6, 2));
    term_t** body_array_7 = malloc(sizeof(term_t*) * 2);
    term_t* args_8[2];
    args_8[0] = create_var("$x");
    args_8[1] = create_var("$z");
    body_array_7[0] = create_compound("parent", args_8, 2);
    term_t* args_9[2];
    args_9[0] = create_var("$z");
    args_9[1] = create_var("$y");
    body_array_7[1] = create_compound("ancestor", args_9, 2);
    term_t* args_10[2];
    args_10[0] = create_var("$x");
    args_10[1] = create_var("$y");
    add_rule(kb, create_atom("ancestor_step"), body_array_7, 2, create_compound("ancestor", args_10, 2));

    // Query 1: 
    printf("?- ");
    term_t* args_11[2];
    args_11[0] = create_var("$x");
    args_11[1] = create_atom("carol");
    print_term(create_compound("ancestor", args_11, 2));
    printf(" & ");
    term_t* args_12[1];
    args_12[0] = create_var("$x");
    print_term(create_compound("tall", args_12, 1));
    printf(".\n");
    term_t** goals_0 = malloc(2 * sizeof(term_t*));
    term_t* args_13[2];
    args_13[0] = create_var("$x");
    args_13[1] = create_atom("carol");
    goals_0[0] = create_compound("ancestor", args_13, 2);
    term_t* args_14[1];
    args_14[0] = create_var("$x");
    goals_0[1] = create_compound("tall", args_14, 1);
    enhanced_solution_list_t* enhanced_solutions_0 = create_enhanced_solution_list();
    int found_enhanced_0 = linear_resolve_query_enhanced(kb, goals_0, 2, enhanced_solutions_0);
    if (enhanced_solutions_0->count > 0) {
        for (int sol = 0; sol < enhanced_solutions_0->count; sol++) {
            print_enhanced_solution(&enhanced_solutions_0->solutions[sol]);
            if (sol < enhanced_solutions_0->count - 1) printf("; ");
        }
        printf(".\n");
    } else {
        printf("false.\n");
    }
    free_enhanced_solution_list(enhanced_solutions_0);
    for (int i = 0; i < 2; i++) {
        free(goals_0[i]);
    }
    free(goals_0);

    // Clean up
    free_linear_kb(kb);
    return 0;  // Always return success - false is a valid result
}
