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
    add_type_mapping(kb, "charlie", "person");
    add_type_mapping(kb, "apple", "food");
    add_type_mapping(kb, "banana", "food");

    add_linear_fact(kb, create_atom("alice"));
    add_linear_fact(kb, create_atom("bob"));
    add_linear_fact(kb, create_atom("charlie"));
    add_linear_fact(kb, create_atom("apple"));
    add_linear_fact(kb, create_atom("banana"));
    term_t* args_0[2];
    args_0[0] = create_atom("alice");
    args_0[1] = create_atom("bob");
    add_persistent_fact(kb, create_clone(create_compound("parent", args_0, 2)));

    // Query 1: 
    printf("?- ");
    term_t* args_1[2];
    args_1[0] = create_atom("alice");
    args_1[1] = create_var("$x");
    print_term(create_compound("parent", args_1, 2));
    printf(".\n");
    term_t** goals_0 = malloc(1 * sizeof(term_t*));
    term_t* args_2[2];
    args_2[0] = create_atom("alice");
    args_2[1] = create_var("$x");
    goals_0[0] = create_compound("parent", args_2, 2);
    enhanced_solution_list_t* enhanced_solutions_0 = create_enhanced_solution_list();
    int found_enhanced_0 = linear_resolve_query_enhanced(kb, goals_0, 1, enhanced_solutions_0);
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
    for (int i = 0; i < 1; i++) {
        free(goals_0[i]);
    }
    free(goals_0);

    // Clean up
    free_linear_kb(kb);
    return 0;  // Always return success - false is a valid result
}
