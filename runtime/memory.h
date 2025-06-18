#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdlib.h>
#include "symbol_table.h"

// Forward declarations
struct linear_kb;
// Forward declarations
struct term;
struct linear_resource;
struct consumption_metadata;
struct linear_kb;

// Linear pointer type for memory management
typedef struct {
    void* ptr;
    size_t size;
} linear_ptr_t;

// String handling (simplified)
typedef struct {
    char* data;
    size_t length;
} linear_string_t;

// Memory management functions
linear_ptr_t linear_alloc(size_t size);
void linear_free(linear_ptr_t lptr);
int64_t linear_load(linear_ptr_t lptr);
void linear_store(linear_ptr_t lptr, int64_t value);

// String functions
linear_string_t linear_string_create(const char* str);
void linear_string_free(linear_string_t str);
linear_string_t linear_string_concat(linear_string_t a, linear_string_t b);

// Enhanced linear memory management functions
void set_auto_deallocation(struct linear_kb* kb, int enabled);
void add_optional_linear_fact(struct linear_kb* kb, struct term* fact);
void add_exponential_linear_fact(struct linear_kb* kb, struct term* fact);
size_t estimate_term_memory_size(struct term* term);
void auto_deallocate_resource(struct linear_kb* kb, struct linear_resource* resource);
int get_memory_usage_estimate(struct linear_kb* kb);
void print_memory_state(struct linear_kb* kb, const char* context);

// Compiler-directed memory management functions
void register_consumption_metadata(struct linear_kb* kb, const char* resource_name, const char* consumption_point, 
                                   int is_optional, int is_persistent, size_t estimated_size);
struct consumption_metadata* find_consumption_metadata(struct linear_kb* kb, const char* resource_name);
void free_linear_resource(struct linear_kb* kb, struct linear_resource* resource);
int should_deallocate_resource(struct linear_kb* kb, const char* resource_name, const char* current_point);

#endif // MEMORY_H
