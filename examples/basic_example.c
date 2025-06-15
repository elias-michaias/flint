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

    add_union_mapping(kb, "fruit", "food");
    add_union_mapping(kb, "apple", "fruit");
    add_union_mapping(kb, "orange", "fruit");
    add_union_mapping(kb, "meat", "food");
    add_union_mapping(kb, "pork", "meat");
    add_union_mapping(kb, "poultry", "meat");
    add_union_mapping(kb, "chicken", "poultry");
    add_union_mapping(kb, "turkey", "poultry");
    add_type_mapping(kb, "apple1", "apple");
    add_type_mapping(kb, "chicken1", "chicken");
    add_type_mapping(kb, "turkey1", "turkey");

    add_linear_fact(kb, create_atom("apple1"));
    add_linear_fact(kb, create_atom("chicken1"));
    add_linear_fact(kb, create_atom("turkey1"));
    term_t** body_array_0 = malloc(sizeof(term_t*) * 1);
    body_array_0[0] = create_atom("food");
    add_rule(kb, create_atom("eat_rule"), body_array_0, 1, create_atom("satisfied"));
    term_t** body_array_1 = malloc(sizeof(term_t*) * 1);
    body_array_1[0] = create_atom("satisfied");
    add_rule(kb, create_atom("mood_rule"), body_array_1, 1, create_atom("happy"));

    // Query 1: 
    printf("?- ");
    print_term(create_atom("eat_rule"));
    printf(" & ");
    print_term(create_atom("mood_rule"));
    printf(".\n");
    term_t** goals_0 = malloc(2 * sizeof(term_t*));
    goals_0[0] = create_atom("eat_rule");
    goals_0[1] = create_atom("mood_rule");
    int success_0 = linear_resolve_query_with_type(kb, goals_0, 2, 0);
    if (success_0 == 0) {
        printf("false.\n");
    }
    for (int i = 0; i < 2; i++) {
        free(goals_0[i]);
    }
    free(goals_0);

    // Clean up
    free_linear_kb(kb);
    return 0;  // Always return success - false is a valid result
}
