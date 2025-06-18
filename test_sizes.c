#include <stdio.h>
#include "runtime/terms.h"
#include "runtime/knowledge_base.h"

int main() {
    printf("sizeof(term_t) = %zu bytes\n", sizeof(term_t));
    printf("sizeof(linear_resource_t) = %zu bytes\n", sizeof(linear_resource_t));
    printf("sizeof(symbol_id_t) = %zu bytes\n", sizeof(symbol_id_t));
    printf("sizeof(var_id_t) = %zu bytes\n", sizeof(var_id_t));
    printf("sizeof(uint8_t) = %zu bytes\n", sizeof(uint8_t));
    printf("sizeof(void*) = %zu bytes\n", sizeof(void*));
    printf("sizeof(int64_t) = %zu bytes\n", sizeof(int64_t));
    
    // Check alignment
    printf("\nField offsets in term_t:\n");
    term_t dummy_term;
    printf("type offset: %zu\n", (char*)&dummy_term.type - (char*)&dummy_term);
    printf("data offset: %zu\n", (char*)&dummy_term.data - (char*)&dummy_term);
    
    return 0;
}
