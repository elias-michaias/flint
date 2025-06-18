#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "terms.h"
#include "memory.h"
#include "knowledge_base.h"

// Linear memory allocation
linear_ptr_t linear_alloc(size_t size) {
    linear_ptr_t lptr;
    lptr.ptr = malloc(size);
    lptr.size = size;
    if (!lptr.ptr) {
        fprintf(stderr, "Linear allocation failed for size %zu\n", size);
        exit(1);
    }
    return lptr;
}

// Linear memory deallocation
void linear_free(linear_ptr_t lptr) {
    if (lptr.ptr) {
        free(lptr.ptr);
    }
}

// Load value from linear pointer
int64_t linear_load(linear_ptr_t lptr) {
    if (!lptr.ptr || lptr.size < sizeof(int64_t)) {
        fprintf(stderr, "Invalid linear pointer access\n");
        exit(1);
    }
    return *(int64_t*)lptr.ptr;
}

// Store value to linear pointer
void linear_store(linear_ptr_t lptr, int64_t value) {
    if (!lptr.ptr || lptr.size < sizeof(int64_t)) {
        fprintf(stderr, "Invalid linear pointer access\n");
        exit(1);
    }
    *(int64_t*)lptr.ptr = value;
}

// Create linear string
linear_string_t linear_string_create(const char* str) {
    linear_string_t lstr;
    lstr.length = strlen(str);
    lstr.data = malloc(lstr.length + 1);
    if (!lstr.data) {
        fprintf(stderr, "String allocation failed\n");
        exit(1);
    }
    strcpy(lstr.data, str);
    return lstr;
}

// Free linear string
void linear_string_free(linear_string_t str) {
    if (str.data) {
        free(str.data);
    }
}

// Concatenate two linear strings (consuming both)
linear_string_t linear_string_concat(linear_string_t a, linear_string_t b) {
    linear_string_t result;
    result.length = a.length + b.length;
    result.data = malloc(result.length + 1);
    if (!result.data) {
        fprintf(stderr, "String concatenation allocation failed\n");
        exit(1);
    }
    
    strcpy(result.data, a.data);
    strcat(result.data, b.data);
    
    // Free the input strings (linear consumption)
    linear_string_free(a);
    linear_string_free(b);
    
    return result;
}

// Memory size estimation for automatic management
size_t estimate_term_memory_size(term_t* term) {
    if (!term) return 0;
    
    size_t size = sizeof(term_t);
    
    switch (term->type) {
        case TERM_ATOM:
            if (term->data.atom) {
                size += strlen(term->data.atom) + 1;
            }
            break;
        case TERM_VAR:
            if (term->data.var) {
                size += strlen(term->data.var) + 1;
            }
            break;
        case TERM_COMPOUND:
            if (term->data.compound.functor) {
                size += strlen(term->data.compound.functor) + 1;
            }
            size += term->data.compound.arity * sizeof(term_t*);
            for (int i = 0; i < term->data.compound.arity; i++) {
                size += estimate_term_memory_size(term->data.compound.args[i]);
            }
            break;
        case TERM_CLONE:
            size += estimate_term_memory_size(term->data.cloned);
            break;
        case TERM_INTEGER:
            // Just the union size, already counted in sizeof(term_t)
            break;
    }
    
    return size;
}

// Automatic deallocation when resource is consumed
void auto_deallocate_resource(linear_kb_t* kb, linear_resource_t* resource) {
    if (!kb->auto_deallocate || resource->persistent) {
        return;  // Don't deallocate persistent or when auto-deallocation is disabled
    }
    
    if (!resource->deallocated) {
        #ifdef DEBUG
        printf("DEBUG: Auto-deallocating consumed resource: ");
        print_term(resource->fact);
        printf(" (freed %zu bytes)\n", resource->memory_size);
        #endif
        
        // Mark as deallocated but don't actually free yet (for debugging)
        resource->deallocated = 1;
        
        // In a production system, you might actually free the memory here:
        // free_term(resource->fact);
        // resource->fact = NULL;
    }
}

// Compiler-directed resource deallocation (immediate, precise)
void free_linear_resource(linear_kb_t* kb, linear_resource_t* resource) {
    if (resource && !resource->deallocated && !resource->persistent) {
        #ifdef DEBUG
        printf("DEBUG: Compiler-directed deallocation of resource: ");
        print_term(resource->fact);
        printf(" (freed %zu bytes)\n", resource->memory_size);
        #endif
        
        // Mark as deallocated
        resource->deallocated = 1;
        
        // In production, actually free the memory:
        // free_term(resource->fact);
        // resource->fact = NULL;
        
        // Update memory tracking
        if (kb->total_memory_allocated >= resource->memory_size) {
            kb->total_memory_allocated -= resource->memory_size;
        }
    }
}

// Print current memory state for debugging
void print_memory_state(linear_kb_t* kb, const char* context) {
    #ifndef DEBUG
    (void)kb;      // Suppress unused parameter warning
    (void)context; // Suppress unused parameter warning
    #endif
    #ifdef DEBUG
    printf("DEBUG: MEMORY STATE [%s]:\n", context);
    
    size_t total_allocated = 0;
    size_t total_active = 0;
    size_t total_deallocated = 0;
    int active_count = 0;
    int deallocated_count = 0;
    
    linear_resource_t* current = kb->resources;
    while (current != NULL) {
        total_allocated += current->memory_size;
        
        if (current->deallocated) {
            total_deallocated += current->memory_size;
            deallocated_count++;
            printf("  [FREED] ");
        } else {
            total_active += current->memory_size;
            active_count++;
            if (current->consumed) {
                printf("  [CONSUMED] ");
            } else {
                printf("  [ACTIVE] ");
            }
        }
        
        print_term(current->fact);
        printf(" (%zu bytes)\n", current->memory_size);
        
        current = current->next;
    }
    
    printf("  SUMMARY: %d active (%zu bytes), %d deallocated (%zu bytes), total allocated: %zu bytes\n",
           active_count, total_active, deallocated_count, total_deallocated, total_allocated);
    printf("DEBUG: END MEMORY STATE\n\n");
    #endif
}
