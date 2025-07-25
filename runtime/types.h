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

#include <nlopt.h>

// Arithmetic operations for constraints
typedef enum {
    ARITH_ADD,       // X + Y = Z
    ARITH_SUB,       // X - Y = Z
    ARITH_MUL,       // X * Y = Z
    ARITH_DIV,       // X / Y = Z
    ARITH_EQUAL,     // X = Y
    ARITH_LEQ,       // X <= Y
    ARITH_GEQ        // X >= Y
} ArithmeticOp;

// Constraint relation types (maps to amoeba relations)
typedef enum {
    CONSTRAINT_EQUAL = 2,        // AM_EQUAL
    CONSTRAINT_LEQ = 1,          // AM_LESSEQUAL  
    CONSTRAINT_GEQ = 3,          // AM_GREATEQUAL
    CONSTRAINT_UNIFY = 100,      // Custom unification constraint
    CONSTRAINT_TYPE = 101,       // Custom type constraint
    CONSTRAINT_FUNCTION = 102    // Custom function constraint f(x) = target
} ConstraintType;

// Strength levels for constraints (maps to amoeba strengths)
typedef enum {
    STRENGTH_REQUIRED = 1000000000,   // AM_REQUIRED
    STRENGTH_STRONG = 1000000,        // AM_STRONG
    STRENGTH_MEDIUM = 1000,           // AM_MEDIUM
    STRENGTH_WEAK = 1               // AM_WEAK
} ConstraintStrength;

// Unique identifier type for logical variables
typedef uint64_t VarId;

// Function registry for dynamic function calls during constraint solving
typedef struct FunctionRegistryEntry {
    char* name;                    // Function name
    int arity;                     // Number of arguments
    void* func_ptr;               // Function pointer (generic)
    struct FunctionRegistryEntry* next; // Linked list
} FunctionRegistryEntry;

typedef struct FunctionRegistry {
    FunctionRegistryEntry* entries;
    size_t count;
} FunctionRegistry;

// Linear operation types for tracking consumption
typedef enum {
    LINEAR_OP_UNIFY,
    LINEAR_OP_FUNCTION_CALL,
    LINEAR_OP_DESTRUCTURE,
    LINEAR_OP_PATTERN_MATCH,
    LINEAR_OP_ASSIGNMENT,
    LINEAR_OP_EXPLICIT_CONSUME,
    LINEAR_OP_CHANNEL_SEND,
    LINEAR_OP_VARIABLE_USE        // Variable consumption after first use
} LinearOp;

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
    VAL_PARTIAL,         // Partial structures
    VAL_CONSUMED         // Consumed linear values (freed memory)
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
        struct {
            void* original_address;  // Address of the consumed value (for debugging)
            LinearOp consumption_op; // How it was consumed
        } consumed;
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
    SUSP_ARITHMETIC,        // Delayed arithmetic constraint
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

// Suspension computation data structure
typedef struct SuspensionComputation {
    SuspensionType type;
    char* function_name;     // Function name for function calls
    char* expr_code;         // For simple expressions
    Value** operands;        // Operands for arithmetic operations
    size_t operand_count;    // Number of operands
    void* data;              // Generic data pointer
} SuspensionComputation;

// Arithmetic constraint for pending constraint mechanism
typedef struct ArithmeticConstraint {
    char* operation;        // "add", "subtract", "multiply", etc.
    Value* left;           // Left operand (can be logical var or concrete)
    Value* right;          // Right operand (can be logical var or concrete)
    Value* result;         // Result (can be logical var or concrete)
    VarId* dependency_vars; // Variables this constraint depends on
    size_t dependency_count; // Number of dependencies
} ArithmeticConstraint;

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

// Flint constraint variable - links VarId to amoeba variable
typedef struct {
    VarId flint_id;              // Flint variable ID
    char* name;                  // Optional name for debugging
    double value;                // Current value of the variable
} FlintConstraintVar;

// Flint constraint - wraps amoeba constraint with metadata
typedef struct {
    ConstraintType type;                      // Flint constraint type
    ConstraintStrength strength;              // Constraint strength
    VarId* var_ids;                           // Variable IDs involved
    size_t var_count;                         // Number of variables
    char* description;                        // Optional description
    double* coefficients;                     // Coefficients for linear constraints
    double constant_term;                     // Constant term for linear constraints
    
    // For function constraints
    char* function_name;                      // Function name (for CONSTRAINT_FUNCTION)
    int target_value;                         // Target value (for CONSTRAINT_FUNCTION)
} FlintConstraint;

// Constraint store integrating amoeba solver
struct ConstraintStore {
    nlopt_opt nlopt_solver;              // NLopt optimizer object
    FlintConstraintVar* variables;         // Constraint variables
    size_t var_count;
    size_t var_capacity;
    FlintConstraint* constraints;          // Flint constraints  
    size_t constraint_count;
    size_t constraint_capacity;
    bool auto_update;                      // Auto-update variable values
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
