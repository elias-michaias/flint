use crate::diagnostic::SourceLocation;

/// Variable names in the new language (can be logic variables with $)
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Variable {
    pub name: String,
    pub is_logic_var: bool, // true for $var, false for regular var
    pub location: Option<SourceLocation>, // Source location for error reporting
}

impl Variable {
    pub fn new(name: String) -> Self {
        Self { 
            name, 
            is_logic_var: false,
            location: None,
        }
    }
    
    pub fn new_logic(name: String) -> Self {
        Self { 
            name, 
            is_logic_var: true,
            location: None,
        }
    }
    
    pub fn with_location(mut self, location: SourceLocation) -> Self {
        self.location = Some(location);
        self
    }
}

/// Types in the functional logic language
#[derive(Debug, Clone, PartialEq)]
pub enum FlintType {
    /// Primitive types
    Int32,
    String,
    Bool,
    Unit,
    
    /// List type: List<T>
    List(Box<FlintType>),
    
    /// Function type: A -> B
    Function {
        params: Vec<FlintType>,
        result: Box<FlintType>,
        effects: Vec<String>, // Effect names
    },
    
    /// Record type: record { name: str, age: i32 }
    Record {
        fields: Vec<(String, FlintType)>,
    },
    
    /// Named type (user-defined)
    Named(String),
    
    /// Type variable for generics
    TypeVar(String),
}

/// Effect declarations
#[derive(Debug, Clone)]
pub struct EffectDecl {
    pub name: String,
    pub operations: Vec<EffectOperation>,
}

#[derive(Debug, Clone)]
pub struct EffectOperation {
    pub name: String,
    pub signature: FlintType,
}

/// Effect handlers
#[derive(Debug, Clone)]
pub struct EffectHandler {
    pub effect_name: String,
    pub handler_name: String,
    pub implementations: Vec<HandlerImpl>,
}

#[derive(Debug, Clone)]
pub struct HandlerImpl {
    pub operation_name: String,
    pub parameters: Vec<Variable>,
    pub body: Expr,
}

/// C imports
#[derive(Debug, Clone)]
pub struct CImport {
    pub header_file: String,
    pub alias: String,
}

/// Python import declaration: import Python "pypi::package::version" as alias
#[derive(Debug, Clone)]
pub struct PythonImport {
    pub package_spec: String, // e.g., "pypi::numpy::2.3.1"
    pub alias: String,
}

/// Expressions in the functional logic language
#[derive(Debug, Clone)]
pub enum Expr {
    /// Variable reference
    Var(Variable),
    
    /// Non-consumptive variable reference (copy) - ~$var
    NonConsumptiveVar(Variable),
    
    /// Integer literal
    Int(i64),
    
    /// String literal
    Str(String),
    
    /// Boolean literal
    Bool(bool),
    
    /// Unit value
    Unit,
    
    /// List literal: [1, 2, 3] or []
    List(Vec<Expr>),
    
    /// List construction: [head|tail]
    ListCons {
        head: Box<Expr>,
        tail: Box<Expr>,
    },
    
    /// Function call
    Call {
        func: Box<Expr>,
        args: Vec<Expr>,
    },
    
    /// C function call: C.Module.function(args)
    CCall {
        module: String,
        function: String,
        args: Vec<Expr>,
    },
    
    /// Python function call: Python.module.function(args)
    PythonCall {
        module: String,
        function: String,
        args: Vec<Expr>,
    },
    
    /// Let binding: let $x = expr in body
    Let {
        var: Variable,
        value: Box<Expr>,
        body: Box<Expr>,
    },
    
    /// Let statement with explicit type: let $x: Type = expr
    LetTyped {
        var: Variable,
        var_type: FlintType,
        value: Box<Expr>,
    },
    
    /// Constraint solving: let f($x) = value
    LetConstraint {
        expr: Box<Expr>,
        target: Box<Expr>,
    },
    
    /// Block expression: { stmt1; stmt2; ... expr }
    Block {
        statements: Vec<Statement>,
        result: Option<Box<Expr>>,
    },
    
    /// Lambda: |$x| => body
    Lambda {
        params: Vec<Variable>,
        body: Box<Expr>,
    },
    
    /// Record construction: User { name: "Alice", age: 30 }
    Record {
        type_name: Option<String>,
        fields: Vec<(String, Expr)>,
    },
    
    /// Field access: user.name
    FieldAccess {
        expr: Box<Expr>,
        field: String,
    },
    
    /// Pattern matching (for lists, records, etc.)
    Match {
        expr: Box<Expr>,
        arms: Vec<MatchArm>,
    },
    
    /// Effect operation call
    EffectCall {
        effect: String,
        operation: String,
        args: Vec<Expr>,
    },
    
    /// Handle effect: handle expr with handler
    Handle {
        expr: Box<Expr>,
        handler: String,
    },
    
    /// Binary operations
    BinOp {
        op: BinOp,
        left: Box<Expr>,
        right: Box<Expr>,
    },
    
    /// Unary operations
    UnaryOp {
        op: UnaryOp,
        expr: Box<Expr>,
    },
}

/// Statements in blocks
#[derive(Debug, Clone)]
pub enum Statement {
    /// Let statement with explicit type: let $x: Type = expr
    LetTyped {
        var: Variable,
        var_type: FlintType,
        value: Expr,
    },
    
    /// Let statement with type inference: let $x = expr
    Let {
        var: Variable,
        value: Expr,
    },
    
    /// Expression statement
    Expr(Expr),
}

/// Pattern matching arms
#[derive(Debug, Clone)]
pub struct MatchArm {
    pub pattern: Pattern,
    pub guard: Option<Expr>,
    pub body: Expr,
}

/// Patterns for matching
#[derive(Debug, Clone)]
pub enum Pattern {
    /// Variable pattern: $x
    Var(Variable),
    
    /// Wildcard pattern: _
    Wildcard,
    
    /// Integer literal pattern: 42
    Int(i64),
    
    /// String literal pattern: "hello"
    Str(String),
    
    /// Boolean literal pattern: true/false
    Bool(bool),
    
    /// Unit pattern: ()
    Unit,
    
    /// Empty list pattern: []
    EmptyList,
    
    /// List cons pattern: [$head|$tail]
    ListCons {
        head: Box<Pattern>,
        tail: Box<Pattern>,
    },
    
    /// List pattern: [1, $x, 3]
    List(Vec<Pattern>),
    
    /// Record pattern: User { name: $n, age: _ }
    Record {
        type_name: Option<String>,
        fields: Vec<(String, Pattern)>,
    },
}

/// Binary operators
#[derive(Debug, Clone, PartialEq)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    Append, // |> for list/string concatenation
}

/// Unary operators
#[derive(Debug, Clone, PartialEq)]
pub enum UnaryOp {
    Not,
    Neg,
}

/// Function definitions with multiple clauses
#[derive(Debug, Clone)]
pub struct FunctionDef {
    pub name: String,
    pub type_signature: Option<FlintType>,
    pub clauses: Vec<FunctionClause>,
}

/// A single function clause/case
#[derive(Debug, Clone)]
pub struct FunctionClause {
    pub patterns: Vec<Pattern>,
    pub guard: Option<Expr>,
    pub body: Expr,
}

/// Type definitions
#[derive(Debug, Clone)]
pub enum TypeDef {
    /// Record type definition
    Record {
        name: String,
        fields: Vec<(String, FlintType)>,
    },
    
    /// Type alias
    Alias {
        name: String,
        type_expr: FlintType,
    },
    
    /// Enum/sum type
    Enum {
        name: String,
        variants: Vec<EnumVariant>,
    },
}

#[derive(Debug, Clone)]
pub struct EnumVariant {
    pub name: String,
    pub fields: Option<Vec<FlintType>>,
}

/// Top-level declarations
#[derive(Debug, Clone)]
pub enum Declaration {
    /// Type definition: User :: record { name: str, age: i32 }
    TypeDef(TypeDef),
    
    /// Function type signature: reverse :: List<T> -> List<T>
    FunctionSig {
        name: String,
        type_sig: FlintType,
    },
    
    /// Function definition: reverse :: ([]) => []
    FunctionDef(FunctionDef),
    
    /// Effect declaration: API :: effect { ... }
    EffectDecl(EffectDecl),
    
    /// Effect handler: APIInProd :: handler for API { ... }
    EffectHandler(EffectHandler),
    
    /// C import: import C "stdio.h" as IO
    CImport(CImport),
    
    /// Python import: import Python "pypi::numpy::2.3.1" as numpy
    PythonImport(PythonImport),
    
    /// Main function designation
    Main(String), // Function name that serves as main
}

/// Complete program
#[derive(Debug, Clone)]
pub struct Program {
    pub declarations: Vec<Declaration>,
}

impl Program {
    pub fn new() -> Self {
        Self {
            declarations: Vec::new(),
        }
    }
    
    /// Get all function definitions
    pub fn functions(&self) -> Vec<&FunctionDef> {
        self.declarations.iter().filter_map(|decl| {
            if let Declaration::FunctionDef(func) = decl {
                Some(func)
            } else {
                None
            }
        }).collect()
    }
    
    /// Get all function signatures  
    pub fn function_signatures(&self) -> Vec<(&String, &FlintType)> {
        self.declarations.iter().filter_map(|decl| {
            if let Declaration::FunctionSig { name, type_sig } = decl {
                Some((name, type_sig))
            } else {
                None
            }
        }).collect()
    }
    
    /// Get all type definitions
    pub fn types(&self) -> Vec<&TypeDef> {
        self.declarations.iter().filter_map(|decl| {
            if let Declaration::TypeDef(typedef) = decl {
                Some(typedef)
            } else {
                None
            }
        }).collect()
    }
    
    /// Get all effect declarations
    pub fn effects(&self) -> Vec<&EffectDecl> {
        self.declarations.iter().filter_map(|decl| {
            if let Declaration::EffectDecl(effect) = decl {
                Some(effect)
            } else {
                None
            }
        }).collect()
    }
    
    /// Get all effect handlers
    pub fn handlers(&self) -> Vec<&EffectHandler> {
        self.declarations.iter().filter_map(|decl| {
            if let Declaration::EffectHandler(handler) = decl {
                Some(handler)
            } else {
                None
            }
        }).collect()
    }
    
    /// Get all C imports
    pub fn c_imports(&self) -> Vec<&CImport> {
        self.declarations.iter().filter_map(|decl| {
            if let Declaration::CImport(import) = decl {
                Some(import)
            } else {
                None
            }
        }).collect()
    }
    
    /// Get the main function name if specified
    pub fn main_function(&self) -> Option<&str> {
        self.declarations.iter().find_map(|decl| {
            if let Declaration::Main(name) = decl {
                Some(name.as_str())
            } else {
                None
            }
        })
    }
}
