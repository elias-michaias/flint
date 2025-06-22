#ifndef FLINT_TYPES_H
#define FLINT_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Forward declarations
typedef struct Value Value;
typedef struct LogicalVar LogicalVar;
typedef struct Suspension Suspension;
typedef struct ChoicePoint ChoicePoint;
typedef struct Environment Environment;
typedef struct ConstraintStore ConstraintStore;

// Unique identifier type for logical variables
typedef uint64_t VarId;

// Value types in the functional logic system
typedef enum {
    VAL_INTEGER,
    VAL_FLOAT, 
    VAL_STRING,
    VAL_ATOM,
    VAL_LIST,
    VAL_RECORD,
    VAL_LOGICAL_VAR,
    VAL_FUNCTION,        // Function values for higher-order programming
    VAL_PARTIAL_APP,     // Partially applied functions
    VAL_SUSPENSION,      // Suspended computations
    VAL_PARTIAL          // Partial structures
} ValueType;

// Function value structure
typedef struct FunctionValue {
    char* name;              // Function name
    int arity;              // Number of arguments
    Value** partial_args;   // Partially applied arguments (NULL if not partial)
    int applied_count;      // Number of arguments already applied
    void* impl;             // Function implementation pointer
} FunctionValue;

// Representation of values in the system
struct Value {
    ValueType type;
    
    // Linear resource tracking (enforced at language level)
    bool is_consumed;           // Whether this value has been consumed
    uint32_t consumption_count; // Number of times consumed (for debugging/trail)
    
    union {
        int64_t integer;
        double float_val;
        char* string;
        char* atom;
        struct {
            Value* elements;
            size_t length;
            size_t capacity;
        } list;
        struct {
            char** field_names;
            Value* field_values;
            size_t field_count;
        } record;
        LogicalVar* logical_var;
        FunctionValue function;     // Function and partial application values
        Suspension* suspension;     // Suspended computations
        struct {
            Value* base;        // The partial structure
            VarId* free_vars;   // Array of free variable IDs
            size_t var_count;
        } partial;
    } data;
};

// Logical variables with constraint tracking
struct LogicalVar {
    VarId id;
    Value* binding;         // NULL if uninstantiated
    Suspension* waiters;    // Suspensions waiting on this variable
    uint32_t use_count;     // Number of times this variable has been used (for linearity)
    bool is_consumed;       // True if this variable has been consumed
    bool allow_reuse;       // Explicitly marked as reusable (opt-in non-linear)
};

// Suspension types for different kinds of delayed computations
typedef enum {
    SUSP_UNIFICATION,       // Delayed unification
    SUSP_FUNCTION_CALL,     // Delayed function evaluation
    SUSP_CONSTRAINT,        // Delayed constraint checking
    SUSP_NARROWING          // Delayed narrowing operation
} SuspensionType;

// Suspensions represent delayed computations
struct Suspension {
    SuspensionType type;
    VarId* dependent_vars;  // Variables this suspension depends on
    size_t var_count;
    void* computation;      // The actual suspended computation
    Suspension* next;       // Linked list of suspensions
    bool is_active;         // Whether this suspension is still relevant
};

// Choice points for backtracking in narrowing
struct ChoicePoint {
    Environment* env_snapshot;
    ConstraintStore* constraint_snapshot;
    Value* alternatives;    // Array of alternative values to try
    size_t alt_count;
    size_t current_alt;
    ChoicePoint* parent;    // Stack of choice points
};

// Environment for variable bindings and scope
struct Environment {
    LogicalVar** variables;
    size_t var_count;
    size_t capacity;
    Environment* parent;    // Lexical scoping
    ConstraintStore* constraint_store;  // Associated constraint store
    struct LinearTrail* linear_trail;  // Trail for backtracking linear operations
};

// Constraint types for the constraint store
typedef enum {
    CONSTRAINT_EQUAL,
    CONSTRAINT_UNIFY,
    CONSTRAINT_TYPE
} ConstraintType;

// Constraint store for accumulated constraints
struct ConstraintStore {
    struct {
        VarId var1, var2;
        ConstraintType type;
        Value* constraint_data;
    }* constraints;
    size_t constraint_count;
    size_t capacity;
};

// Function signature for narrowing operations
typedef Value* (*NarrowingFunc)(Value** args, size_t arg_count, Environment* env);

// Function signature for suspension resumption
typedef bool (*ResumptionFunc)(Suspension* susp, Environment* env);

// Pattern for more complex pattern matching
typedef struct Pattern {
    ValueType type;
    union {
        int64_t integer;
        char* atom;
        struct {
            struct Pattern* elements;
            size_t count;
            bool has_tail;              // For list patterns like [H|T]
            struct Pattern* tail;
        } list_pattern;
        struct {
            char** field_names;
            struct Pattern* field_patterns;
            size_t field_count;
        } record_pattern;
        VarId variable;                 // Variable in pattern
    } data;
} Pattern;

// Linear memory management types
typedef struct LinearSnapshot {
    VarId var_id;
    uint32_t use_count;
    bool is_consumed;
    bool allow_reuse;
    Value* binding;
} LinearSnapshot;

// Operations that can be performed on linear values
typedef enum {
    OP_CONSUME,           // Default: consume the value
    OP_BORROW,           // Read-only access (non-consumptive)
    OP_DUPLICATE,        // Explicitly duplicate for reuse
    OP_SHARE            // Mark as shareable (opt-in non-linear)
} LinearOperation;

// =============================================================================
// LINEAR RESOURCE MANAGEMENT TYPES
// =============================================================================

// Linear operation types for tracking consumption
typedef enum {
    LINEAR_OP_UNIFY,
    LINEAR_OP_FUNCTION_CALL,
    LINEAR_OP_DESTRUCTURE,
    LINEAR_OP_PATTERN_MATCH,
    LINEAR_OP_ASSIGNMENT,
    LINEAR_OP_EXPLICIT_CONSUME
} LinearOp;

// Trail entry for tracking consumed values
typedef struct LinearTrailEntry {
    Value* consumed_value;      // The value that was consumed
    LinearOp operation;         // What operation consumed it
    size_t timestamp;           // When it was consumed
    bool is_active;             // Whether this entry is still active
} LinearTrailEntry;

// Checkpoint for backtracking
typedef size_t LinearCheckpoint;

// Linear trail for backtracking support
typedef struct LinearTrail {
    LinearTrailEntry* entries;
    size_t entry_count;
    size_t capacity;
    
    // Checkpoint stack for nested backtracking
    LinearCheckpoint* checkpoint_stack;
    size_t checkpoint_count;
    size_t checkpoint_capacity;
} LinearTrail;

// Result type for linear list destructuring
typedef struct LinearListDestructure {
    Value* elements;            // Ownership transferred to caller
    size_t count;
    bool success;
} LinearListDestructure;

#endif // FLINT_TYPES_H
