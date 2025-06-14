use std::collections::HashMap;

/// Represents a variable name
pub type VarName = String;

/// Represents a function name
pub type FunctionName = String;

/// Represents a type name
pub type TypeName = String;

/// Type declarations
#[derive(Debug, Clone, PartialEq)]
pub enum LogicType {
    /// Built-in types
    Integer,
    String,
    /// User-defined type (e.g., person)
    Named(TypeName),
    /// Union type (e.g., fruit = apple | orange)
    Union(Vec<LogicType>),
    /// Function type (for predicates): A -> B -> ... -> type
    Arrow(Vec<LogicType>),
    /// The special "type" type for predicates
    Type,
}

/// Type definition with optional nested union types
#[derive(Debug, Clone, PartialEq)]
pub struct TypeDefinition {
    pub name: String,
    pub union_variants: Option<UnionVariants>, // for complex union structures
}

/// Union variants can be nested and complex
#[derive(Debug, Clone, PartialEq)]
pub enum UnionVariants {
    /// Simple list of variants: (apple | orange)
    Simple(Vec<String>),
    /// Nested unions: fruit (apple | orange) | meat (pork | poultry (chicken | turkey))
    Nested(Vec<UnionVariant>),
}

/// A single variant in a union, which can itself have sub-variants
#[derive(Debug, Clone, PartialEq)]
pub struct UnionVariant {
    pub name: String,
    pub sub_variants: Option<UnionVariants>,
}

/// Type declaration for predicates
#[derive(Debug, Clone, PartialEq)]
pub struct PredicateType {
    pub name: String,
    pub signature: LogicType, // Now uses the arrow type
}

/// Term type declaration  
#[derive(Debug, Clone, PartialEq)]
pub struct TermType {
    pub name: String,
    pub term_type: LogicType,
}

/// Linear Logic expressions
#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    /// Variable reference
    Var(VarName),
    
    /// Integer literal
    Int(i64),
    
    /// String literal
    Str(String),
    
    /// Function application
    App {
        func: FunctionName,
        args: Vec<Expr>,
    },
    
    /// Linear function definition
    /// `lam x. body` where x is consumed exactly once
    Lambda {
        param: VarName,
        body: Box<Expr>,
    },
    
    /// Let binding with linear consumption
    /// `let x = value in body` where x must be used exactly once in body
    Let {
        var: VarName,
        value: Box<Expr>,
        body: Box<Expr>,
    },
    
    /// Conditional expression
    If {
        cond: Box<Expr>,
        then_branch: Box<Expr>,
        else_branch: Box<Expr>,
    },
    
    /// Pair construction (multiplicative conjunction)
    Pair(Box<Expr>, Box<Expr>),
    
    /// Pair destruction (pattern matching)
    MatchPair {
        expr: Box<Expr>,
        left_var: VarName,
        right_var: VarName,
        body: Box<Expr>,
    },
    
    /// Linear resource allocation
    Alloc {
        size: Box<Expr>,
    },
    
    /// Linear resource deallocation
    Free {
        ptr: Box<Expr>,
    },
    
    /// Memory read
    Load {
        ptr: Box<Expr>,
    },
    
    /// Memory write (consumes the pointer linearly)
    Store {
        ptr: Box<Expr>,
        value: Box<Expr>,
    },
    
    /// Binary operations
    BinOp {
        op: BinOpKind,
        left: Box<Expr>,
        right: Box<Expr>,
    },
}

/// Binary operation kinds
#[derive(Debug, Clone, PartialEq)]
pub enum BinOpKind {
    Add,
    Sub,
    Mul,
    Div,
    Eq,
    Lt,
    Gt,
    And,
    Or,
}

/// Linear Logic types
#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    /// Integer type
    Int,
    
    /// String type
    Str,
    
    /// Linear function type A ⊸ B
    Linear(Box<Type>, Box<Type>),
    
    /// Multiplicative conjunction A ⊗ B (pairs)
    Tensor(Box<Type>, Box<Type>),
    
    /// Linear pointer to T
    LinearPtr(Box<Type>),
    
    /// Unit type
    Unit,
}

/// Function definition
#[derive(Debug, Clone)]
pub struct Function {
    pub name: FunctionName,
    pub param: VarName,
    pub param_type: Type,
    pub return_type: Type,
    pub body: Expr,
}

/// Top-level program
#[derive(Debug, Clone)]
pub struct Program {
    pub type_definitions: Vec<TypeDefinition>,  // New syntax: type fruit (apple | orange)
    pub type_declarations: Vec<PredicateType>,
    pub term_types: Vec<TermType>,
    pub functions: Vec<Function>,
    pub clauses: Vec<Clause>,
    pub queries: Vec<Query>,
    pub main: Option<Expr>,
}

/// Type environment for tracking variable types and usage
#[derive(Debug, Clone)]
pub struct TypeEnv {
    /// Maps variables to their types
    types: HashMap<VarName, Type>,
    /// Tracks which variables have been used (for linearity checking)
    used: HashMap<VarName, bool>,
}

impl TypeEnv {
    pub fn new() -> Self {
        Self {
            types: HashMap::new(),
            used: HashMap::new(),
        }
    }
    
    pub fn bind(&mut self, var: VarName, ty: Type) {
        self.types.insert(var.clone(), ty);
        self.used.insert(var, false);
    }
    
    pub fn lookup(&self, var: &VarName) -> Option<&Type> {
        self.types.get(var)
    }
    
    pub fn use_var(&mut self, var: &VarName) -> Result<(), String> {
        if let Some(used) = self.used.get_mut(var) {
            if *used {
                return Err(format!("Variable '{}' used more than once", var));
            }
            *used = true;
            Ok(())
        } else {
            Err(format!("Variable '{}' not in scope", var))
        }
    }
    
    pub fn check_all_used(&self) -> Result<(), String> {
        for (var, used) in &self.used {
            if !used {
                return Err(format!("Variable '{}' is not used", var));
            }
        }
        Ok(())
    }
    
    pub fn split(self) -> (Self, Self) {
        // For branching contexts, we need to ensure linear variables
        // are used in exactly one branch
        (self.clone(), self)
    }
}

/// Logical programming constructs
#[derive(Debug, Clone, PartialEq)]
pub enum Term {
    /// Atom (constant) with type
    Atom {
        name: String,
        type_name: Option<LogicType>,
    },
    
    /// Variable with type
    Var {
        name: String,
        type_name: Option<LogicType>,
    },
    
    /// Compound term: functor(args...)
    Compound {
        functor: String,
        args: Vec<Term>,
    },
    
    /// Integer
    Integer(i64),
    
    /// Cloned term: !term (creates a copy for linear consumption)
    Clone(Box<Term>),
}

/// Resource state for linear logic
#[derive(Debug, Clone, PartialEq)]
pub enum ResourceState {
    Available,
    Consumed,
}

/// Linear resource with unique ID and consumption state
#[derive(Debug, Clone)]
pub struct LinearResource {
    pub id: usize,
    pub clause: Clause,
    pub state: ResourceState,
}

/// Logical clause (fact or rule)
#[derive(Debug, Clone, PartialEq)]
pub enum Clause {
    /// Fact: predicate(args).
    Fact {
        predicate: String,
        args: Vec<Term>,
        persistent: bool,  // true for !fact, false for fact
    },
    
    /// Rule: head :- body.
    Rule {
        head: Term,
        body: Vec<Term>,
        produces: Option<Term>,  // Optional production after =>
    },
}

/// Query to resolve
#[derive(Debug, Clone)]
pub struct Query {
    pub goals: Vec<Term>,
}

/// Unification substitution
#[derive(Debug, Clone)]
pub struct Substitution {
    pub bindings: HashMap<String, Term>,
}

impl Substitution {
    pub fn new() -> Self {
        Self {
            bindings: HashMap::new(),
        }
    }
    
    pub fn bind(&mut self, var: String, term: Term) {
        self.bindings.insert(var, term);
    }
    
    pub fn lookup(&self, var: &str) -> Option<&Term> {
        self.bindings.get(var)
    }
    
    pub fn apply(&self, term: &Term) -> Term {
        match term {
            Term::Var { name, type_name: _ } => {
                if let Some(binding) = self.lookup(name) {
                    self.apply(binding)
                } else {
                    term.clone()
                }
            }
            Term::Compound { functor, args } => {
                Term::Compound {
                    functor: functor.clone(),
                    args: args.iter().map(|arg| self.apply(arg)).collect(),
                }
            }
            _ => term.clone(),
        }
    }
}
