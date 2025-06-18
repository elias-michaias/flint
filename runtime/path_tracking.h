#ifndef PATH_TRACKING_H
#define PATH_TRACKING_H

// Linear path tracking for showing resource consumption chains
typedef struct path_step {
    enum {
        PATH_CONSUME,    // Consumed a resource: "turkey1"
        PATH_RULE_APPLY, // Applied a rule: "eat"
        PATH_PRODUCE     // Produced a resource: "=> satisfied"
    } type;
    char* item_name;           // Name of resource or rule
    char* produced_name;       // For PATH_PRODUCE, what was produced
    struct path_step* next;
} path_step_t;

typedef struct linear_path {
    path_step_t* steps;        // Linked list of path steps
    path_step_t* last_step;    // For easy appending
} linear_path_t;

// Path tracking functions
linear_path_t* create_linear_path();
void add_path_consume(linear_path_t* path, const char* resource_name);
void add_path_rule_apply(linear_path_t* path, const char* rule_name);
void add_path_produce(linear_path_t* path, const char* rule_name, const char* produced_name);
void print_linear_path(linear_path_t* path);
void free_linear_path(linear_path_t* path);
linear_path_t* copy_linear_path(linear_path_t* path);

#endif // PATH_TRACKING_H
