#include "runtime.h"
#include "lib/libdill/libdill-install/include/libdill.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

// =============================================================================
// ASYNCHRONOUS OPERATIONS AND STRUCTURED CONCURRENCY
// =============================================================================
//
// This module provides async operations and structured concurrency for Flint:
// - Coroutines for lightweight concurrent execution
// - Channels for communication between coroutines
// - Structured concurrency ensuring proper cleanup
// - Integration with Flint's linear resource system
// - Async I/O operations
// - Timeouts and scheduling
//
// Key principles:
// 1. All async operations are structured - coroutines are properly managed
// 2. Channels provide deterministic communication patterns
// 3. Linear resources are properly tracked across coroutine boundaries
// 4. Backtracking can span multiple coroutines (with careful coordination)
// 5. Async operations integrate with Flint's narrowing and unification

// =============================================================================
// ASYNC CONTEXT AND STATE
// =============================================================================

// Async context for managing coroutines and channels
struct AsyncContext {
    Environment* env;               // Associated Flint environment
    LinearTrail* linear_trail;      // Linear resource trail for this context
    int coroutine_count;           // Number of active coroutines
    bool is_structured;            // Whether this is structured concurrency
    int timeout_ms;                // Default timeout for operations
};

// Channel wrapper for Flint values (defined in runtime.h)

// Coroutine context for managing Flint state
typedef struct CoroutineContext {
    AsyncContext* async_ctx;       // Parent async context
    Environment* local_env;        // Local environment for this coroutine
    LinearCheckpoint checkpoint;   // Linear resource checkpoint
    int coroutine_id;             // Unique coroutine identifier
} CoroutineContext;

// Global async system state
static AsyncContext* global_async_context = NULL;
static int next_coroutine_id = 1;

// =============================================================================
// ASYNC CONTEXT MANAGEMENT
// =============================================================================

// Create a new async context
AsyncContext* flint_create_async_context(Environment* env) {
    AsyncContext* ctx = (AsyncContext*)malloc(sizeof(AsyncContext));
    if (!ctx) return NULL;
    
    ctx->env = env;
    ctx->linear_trail = env ? env->linear_trail : NULL;
    ctx->coroutine_count = 0;
    ctx->is_structured = true;
    ctx->timeout_ms = 5000; // 5 second default timeout
    
    return ctx;
}

// Set the global async context
void flint_set_async_context(AsyncContext* ctx) {
    global_async_context = ctx;
}

// Get the current async context
AsyncContext* flint_get_async_context(void) {
    return global_async_context;
}

// Cleanup async context
void flint_free_async_context(AsyncContext* ctx) {
    if (!ctx) return;
    
    // Wait for all coroutines to finish if structured
    if (ctx->is_structured && ctx->coroutine_count > 0) {
        printf("Warning: Cleaning up async context with %d active coroutines\n", 
               ctx->coroutine_count);
    }
    
    free(ctx);
}

// =============================================================================
// CHANNEL OPERATIONS
// =============================================================================

// Create a Flint channel
FlintChannel* flint_create_channel(size_t capacity, CType value_type) {
    FlintChannel* chan = (FlintChannel*)malloc(sizeof(FlintChannel));
    if (!chan) return NULL;
    
    // Create libdill channel - chmake creates a pair of handles
    int result = chmake(chan->dill_channel);
    if (result != 0) {
        free(chan);
        return NULL;
    }
    
    chan->is_closed = false;
    chan->capacity = capacity;
    chan->value_type = value_type;
    
    return chan;
}

// Send a value to a channel
bool flint_channel_send(FlintChannel* chan, Value* value, int timeout_ms) {
    if (!chan || chan->is_closed || !value) return false;
    
    // Create a deadline for the operation
    int64_t deadline = now() + timeout_ms;
    
    // Send the value pointer - use the send handle (index 0)
    int result = chsend(chan->dill_channel[0], &value, sizeof(Value*), deadline);
    
    if (result == 0) {
        // Successful send - handle linear resource consumption
        AsyncContext* ctx = flint_get_async_context();
        if (ctx && ctx->linear_trail) {
            flint_consume_value(value, LINEAR_OP_CHANNEL_SEND);
        }
        return true;
    }
    
    return false;
}

// Receive a value from a channel
Value* flint_channel_recv(FlintChannel* chan, int timeout_ms) {
    if (!chan || chan->is_closed) return NULL;
    
    // Create a deadline for the operation
    int64_t deadline = now() + timeout_ms;
    
    Value* value = NULL;
    int result = chrecv(chan->dill_channel[1], &value, sizeof(Value*), deadline);
    
    if (result == 0) {
        return value;
    }
    
    return NULL;
}

// Close a channel
void flint_channel_close(FlintChannel* chan) {
    if (!chan || chan->is_closed) return;
    
    chan->is_closed = true;
    hclose(chan->dill_channel[0]);
    hclose(chan->dill_channel[1]);
}

// =============================================================================
// COROUTINE OPERATIONS
// =============================================================================

// Coroutine function wrapper
struct CoroutineArgs {
    Value* (*func)(Value**, size_t, Environment*);
    Value** args;
    size_t arg_count;
    Environment* env;
    FlintChannel* result_channel;
    CoroutineContext* ctx;
};

// Coroutine entry point
coroutine void flint_coroutine_entry(struct CoroutineArgs* args) {
    CoroutineContext* ctx = args->ctx;
    
    // Set up local linear context
    if (ctx->async_ctx->linear_trail) {
        flint_set_linear_context(args->env);
    }
    
    // Execute the function
    Value* result = args->func(args->args, args->arg_count, args->env);
    
    // Send result back through channel
    if (args->result_channel) {
        flint_channel_send(args->result_channel, result, ctx->async_ctx->timeout_ms);
    }
    
    // Cleanup
    ctx->async_ctx->coroutine_count--;
    free(args->args);
    free(args);
    free(ctx);
}

// Spawn a new coroutine
FlintChannel* flint_spawn_coroutine(Value* (*func)(Value**, size_t, Environment*),
                                   Value** args, size_t arg_count, Environment* env) {
    AsyncContext* async_ctx = flint_get_async_context();
    if (!async_ctx) {
        fprintf(stderr, "No async context available for spawning coroutine\n");
        return NULL;
    }
    
    // Create result channel
    FlintChannel* result_chan = flint_create_channel(0, C_TYPE_POINTER);
    if (!result_chan) return NULL;
    
    // Create coroutine context
    CoroutineContext* ctx = (CoroutineContext*)malloc(sizeof(CoroutineContext));
    if (!ctx) {
        flint_channel_close(result_chan);
        free(result_chan);
        return NULL;
    }
    
    ctx->async_ctx = async_ctx;
    ctx->local_env = flint_create_environment(env);
    ctx->coroutine_id = next_coroutine_id++;
    
    // Create checkpoint for linear resources
    if (async_ctx->linear_trail) {
        ctx->checkpoint = flint_linear_checkpoint(async_ctx->linear_trail);
    }
    
    // Prepare arguments
    struct CoroutineArgs* coro_args = (struct CoroutineArgs*)malloc(sizeof(struct CoroutineArgs));
    if (!coro_args) {
        flint_free_environment(ctx->local_env);
        free(ctx);
        flint_channel_close(result_chan);
        free(result_chan);
        return NULL;
    }
    
    coro_args->func = func;
    coro_args->arg_count = arg_count;
    coro_args->env = ctx->local_env;
    coro_args->result_channel = result_chan;
    coro_args->ctx = ctx;
    
    // Copy arguments
    if (arg_count > 0) {
        coro_args->args = (Value**)malloc(sizeof(Value*) * arg_count);
        for (size_t i = 0; i < arg_count; i++) {
            coro_args->args[i] = flint_deep_copy_value(args[i]);
        }
    } else {
        coro_args->args = NULL;
    }
    
    // Spawn the coroutine
    int coro_handle = go(flint_coroutine_entry(coro_args));
    if (coro_handle < 0) {
        fprintf(stderr, "Failed to spawn coroutine: %s\n", strerror(errno));
        free(coro_args->args);
        free(coro_args);
        flint_free_environment(ctx->local_env);
        free(ctx);
        flint_channel_close(result_chan);
        free(result_chan);
        return NULL;
    }
    
    async_ctx->coroutine_count++;
    return result_chan;
}

// Wait for a coroutine to complete and get its result
Value* flint_await_coroutine(FlintChannel* result_channel, int timeout_ms) {
    if (!result_channel) return NULL;
    
    Value* result = flint_channel_recv(result_channel, timeout_ms);
    
    // Cleanup the channel
    flint_channel_close(result_channel);
    free(result_channel);
    
    return result;
}

// =============================================================================
// STRUCTURED CONCURRENCY OPERATIONS
// =============================================================================

// Bundle for managing multiple coroutines (defined in runtime.h)

// Create a coroutine bundle
CoroutineBundle* flint_create_bundle(size_t initial_capacity) {
    CoroutineBundle* bundle = (CoroutineBundle*)malloc(sizeof(CoroutineBundle));
    if (!bundle) return NULL;
    
    bundle->result_channels = (FlintChannel**)malloc(sizeof(FlintChannel*) * initial_capacity);
    if (!bundle->result_channels) {
        free(bundle);
        return NULL;
    }
    
    bundle->count = 0;
    bundle->capacity = initial_capacity;
    
    return bundle;
}

// Add a coroutine to the bundle
bool flint_bundle_spawn(CoroutineBundle* bundle, 
                       Value* (*func)(Value**, size_t, Environment*),
                       Value** args, size_t arg_count, Environment* env) {
    if (!bundle) return false;
    
    // Resize if needed
    if (bundle->count >= bundle->capacity) {
        size_t new_capacity = bundle->capacity * 2;
        FlintChannel** new_channels = (FlintChannel**)realloc(bundle->result_channels, 
                                                              sizeof(FlintChannel*) * new_capacity);
        if (!new_channels) return false;
        
        bundle->result_channels = new_channels;
        bundle->capacity = new_capacity;
    }
    
    // Spawn the coroutine
    FlintChannel* result_chan = flint_spawn_coroutine(func, args, arg_count, env);
    if (!result_chan) return false;
    
    bundle->result_channels[bundle->count++] = result_chan;
    return true;
}

// Wait for all coroutines in the bundle to complete
Value** flint_bundle_wait_all(CoroutineBundle* bundle, int timeout_ms) {
    if (!bundle || bundle->count == 0) return NULL;
    
    Value** results = (Value**)malloc(sizeof(Value*) * bundle->count);
    if (!results) return NULL;
    
    // Wait for each coroutine
    for (size_t i = 0; i < bundle->count; i++) {
        results[i] = flint_await_coroutine(bundle->result_channels[i], timeout_ms);
        bundle->result_channels[i] = NULL; // Channel is now cleaned up
    }
    
    return results;
}

// Wait for the first coroutine in the bundle to complete
Value* flint_bundle_wait_any(CoroutineBundle* bundle, size_t* completed_index, int timeout_ms) {
    if (!bundle || bundle->count == 0) return NULL;
    
    // Use libdill's choose to wait for any channel
    int64_t deadline = now() + timeout_ms;
    
    for (size_t i = 0; i < bundle->count; i++) {
        if (!bundle->result_channels[i]) continue;
        
        Value* result = flint_channel_recv(bundle->result_channels[i], 0); // Non-blocking
        if (result) {
            if (completed_index) *completed_index = i;
            
            // Clean up this channel
            flint_channel_close(bundle->result_channels[i]);
            free(bundle->result_channels[i]);
            bundle->result_channels[i] = NULL;
            
            return result;
        }
    }
    
    // If no immediate result, use msleep and retry pattern
    // (A more sophisticated implementation would use libdill's choose properly)
    msleep(deadline);
    return NULL;
}

// Free a coroutine bundle
void flint_free_bundle(CoroutineBundle* bundle) {
    if (!bundle) return;
    
    // Cancel any remaining coroutines
    for (size_t i = 0; i < bundle->count; i++) {
        if (bundle->result_channels[i]) {
            flint_channel_close(bundle->result_channels[i]);
            free(bundle->result_channels[i]);
        }
    }
    
    free(bundle->result_channels);
    free(bundle);
}

// =============================================================================
// ASYNC I/O OPERATIONS
// =============================================================================

// Async file read operation
coroutine Value* flint_async_read_file(const char* filename) {
    if (!filename) return NULL;
    
    // Simple file reading (in a real implementation, this would use async I/O)
    FILE* file = fopen(filename, "r");
    if (!file) return flint_create_atom("error");
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read file content
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(file);
        return flint_create_atom("error");
    }
    
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    
    Value* result = flint_create_string(content);
    free(content);
    
    return result;
}

// Sleep asynchronously
void flint_async_sleep(int milliseconds) {
    msleep(now() + milliseconds);
}

// Get current time in milliseconds
int64_t flint_now(void) {
    return now();
}

// =============================================================================
// INTEGRATION WITH FLINT RUNTIME
// =============================================================================

// Initialize the async system
void flint_init_async_system(Environment* env) {
    AsyncContext* ctx = flint_create_async_context(env);
    flint_set_async_context(ctx);
    // printf("Async system initialized with structured concurrency support\n");
}

// Cleanup the async system
void flint_cleanup_async_system(void) {
    AsyncContext* ctx = flint_get_async_context();
    if (ctx) {
        flint_free_async_context(ctx);
        flint_set_async_context(NULL);
    }
}

// Create async Flint functions for the narrowing system
Value* flint_narrow_async_spawn(Value** args, size_t arg_count, Environment* env) {
    if (arg_count < 1) return NULL;
    
    // First argument should be a function, rest are arguments to that function
    Value* func_val = args[0];
    if (func_val->type != VAL_FUNCTION) return NULL;
    
    // For now, create a simple wrapper
    // In a full implementation, this would properly handle Flint function calls
    return flint_create_atom("async_spawn_placeholder");
}

Value* flint_narrow_async_await(Value** args, size_t arg_count, Environment* env) {
    if (arg_count != 1) return NULL;
    
    // Argument should be a channel or async handle
    // For now, return a placeholder
    (void)args;
    (void)env;
    return flint_create_atom("async_await_placeholder");
}

// Register async functions with the narrowing system
void flint_register_async_functions(void) {
    // These would be registered with the narrowing system
    // Implementation depends on how the narrowing registry works
    // printf("Async functions registered with narrowing system\n");
}
