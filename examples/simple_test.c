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

    add_linear_fact(kb, create_atom("alice"));
    add_linear_fact(kb, create_atom("bob"));
    term_t* args_0[2];
    args_0[0] = create_atom("alice");
    args_0[1] = create_atom("bob");
    add_persistent_fact(kb, create_clone(create_compound("parent", args_0, 2)));
    term_t** body_array_1 = malloc(sizeof(term_t*) * 1);
    term_t* args_2[2];
    args_2[0] = create_var("$x");
    args_2[1] = create_var("$y");
    body_array_1[0] = create_compound("parent", args_2, 2);
    term_t* args_3[2];
    args_3[0] = create_var("$x");
    args_3[1] = create_var("$y");
    add_rule(kb, create_atom("simple_rule"), body_array_1, 1, create_compound("ancestor", args_3, 2));

    // Query 1: 
    printf("?- ");
    term_t* args_4[2];
    args_4[0] = create_atom("alice");
    args_4[1] = create_var("$y");
    print_term(create_compound("ancestor", args_4, 2));
    printf(".\n");
    term_t** goals_0 = malloc(1 * sizeof(term_t*));
    term_t* args_5[2];
    args_5[0] = create_atom("alice");
    args_5[1] = create_var("$y");
    goals_0[0] = create_compound("ancestor", args_5, 2);
    enhanced_solution_list_t* enhanced_solutions_0 = create_enhanced_solution_list();
    (void)linear_resolve_query_enhanced(kb, goals_0, 1, enhanced_solutions_0);
    if (enhanced_solutions_0->count > 0) {
        for (int sol = 0; sol < enhanced_solutions_0->count; sol++) {
            print_enhanced_solution(&enhanced_solutions_0->solutions[sol]);
            if (sol < enhanced_solutions_0->count - 1) {
                printf(";\n");
            } else {
                printf(".\n");
            }
        }
    } else {
        printf("false.\n");
    }
    free_enhanced_solution_list(enhanced_solutions_0);
    for (int i = 0; i < 1; i++) {
        free(goals_0[i]);
    }
    free(goals_0);

    // Clean up
    free_linear_kb(kb);
    return 0;  // Always return success - false is a valid result
}
