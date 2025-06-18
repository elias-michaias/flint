#ifndef SOLUTIONS_H
#define SOLUTIONS_H

#include "unification.h"

#define MAX_SOLUTIONS 100

// Variable binding for enhanced solutions
typedef struct variable_binding {
    char* var_name;
    term_t* value;
} variable_binding_t;

// Enhanced solution structure
typedef struct enhanced_solution {
    substitution_t substitution;
    variable_binding_t* bindings;
    int binding_count;
} enhanced_solution_t;

// Enhanced solution list
typedef struct enhanced_solution_list {
    int count;
    int capacity;
    enhanced_solution_t* solutions;
} enhanced_solution_list_t;

// Basic solution list for backtracking
typedef struct solution_list {
    int count;
    int capacity;
    substitution_t* solutions;
} solution_list_t;

// Solution list functions
solution_list_t* create_solution_list();
void add_solution(solution_list_t* list, substitution_t* solution);
void free_solution_list(solution_list_t* list);

// Enhanced solution functions
enhanced_solution_list_t* create_enhanced_solution_list();
void add_enhanced_solution(enhanced_solution_list_t* list, substitution_t* subst);
void print_enhanced_solution(enhanced_solution_t* solution);
void free_enhanced_solution_list(enhanced_solution_list_t* list);
int enhanced_solutions_are_equivalent(enhanced_solution_t* solution, substitution_t* subst);

#endif // SOLUTIONS_H
