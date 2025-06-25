use crate::ast;
use std::collections::{HashMap, HashSet};

/// Intermediate Representation for Flint code
/// This provides an explicit, type-aware, constraint-rich representation
/// that analyzes actual function body operations to generate constraints

#[derive(Debug, Clone, PartialEq)]
pub struct IRProgram {
    pub functions: Vec<IRFunction>,
    pub global_constraints: Vec<IRConstraint>,
    pub symbol_table: HashMap<String, Symbol>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct IRFunction {
    pub name: String,
    pub parameters: Vec<Symbol>,
    pub return_type: IRType,
    pub effects: Vec<String>,
    pub body: IRExpression,
    pub determinism: Determinism,
    pub constraints: Vec<IRConstraint>,
    pub binding_analysis: HashMap<Symbol, BindingStatus>,
}

/// A symbol in the IR (represents variables, temporaries, etc.)
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Symbol {
    pub name: String,
    pub is_logic_var: bool,
    pub is_temporary: bool,
}

impl Symbol {
    pub fn new(name: String) -> Self {
        Self {
            name,
            is_logic_var: false,
            is_temporary: false,
        }
    }
    
    pub fn from_variable(var: &ast::Variable) -> Self {
        Self {
            name: var.name.clone(),
            is_logic_var: var.is_logic_var,
            is_temporary: false,
        }
    }
    
    pub fn temporary(name: String) -> Self {
        Self {
            name,
            is_logic_var: false,
            is_temporary: true,
        }
    }
}

/// IR expressions - explicit representation of computations
#[derive(Debug, Clone, PartialEq)]
pub enum IRExpression {
    /// Symbol reference
    Symbol(Symbol),
    
    /// Literal values
    Literal(Literal),
    
    /// List of expressions
    List(Vec<IRExpression>),
    
    /// List construction [head|tail]
    ListCons {
        head: Box<IRExpression>,
        tail: Box<IRExpression>,
    },
    
    /// List append operation
    ListAppend {
        lists: Vec<IRExpression>,
    },
    
    /// List head operation
    ListHead {
        list: Box<IRExpression>,
    },
    
    /// List tail operation
    ListTail {
        list: Box<IRExpression>,
    },
    
    /// Function call
    FunctionCall {
        function: Box<IRExpression>,
        arguments: Vec<IRExpression>,
    },
    
    /// C function call
    CCall {
        module: String,
        function: String,
        arguments: Vec<IRExpression>,
    },
    
    /// Python function call (via Python C API)
    PythonCall {
        module: String,
        function: String,
        arguments: Vec<IRExpression>,
    },

    /// Effect call
    EffectCall {
        effect: String,
        operation: String,
        arguments: Vec<IRExpression>,
    },
    
    /// Record construction
    Record {
        type_name: Option<String>,
        fields: Vec<(String, IRExpression)>,
    },
    
    /// Field access
    FieldAccess {
        record: Box<IRExpression>,
        field: String,
    },
    
    /// Inline arithmetic operation (deterministic mode)
    InlineArithmetic {
        operation: ArithmeticOp,
        left: Box<IRExpression>,
        right: Box<IRExpression>,
    },
    
    /// Constraint-based arithmetic (non-deterministic mode)
    ConstraintArithmetic {
        operation: ArithmeticOp,
        operands: Vec<IRExpression>,
        result: Symbol,
    },
}

/// Literal values in IR
#[derive(Debug, Clone, PartialEq)]
pub enum Literal {
    Integer(i64),
    String(String),
    Boolean(bool),
    Unit,
}

/// Constraints extracted from actual operations
#[derive(Debug, Clone, PartialEq)]
pub enum IRConstraint {
    /// Arithmetic constraint: result = operand1 op operand2
    Arithmetic {
        operation: ArithmeticOp,
        operands: Vec<IRExpression>,
        result: IRExpression,
    },
    
    /// Relational constraint: operand1 rel operand2
    Relational {
        operation: RelationalOp,
        operands: Vec<IRExpression>,
    },
    
    /// Logical constraint: result = operand1 logical_op operand2
    Logical {
        operation: LogicalOp,
        operands: Vec<IRExpression>,
        result: IRExpression,
    },
    
    /// Unification constraint: left = right
    Unification {
        left: IRExpression,
        right: IRExpression,
    },
    
    /// Function call constraint
    FunctionCall {
        function: String,
        arguments: Vec<IRExpression>,
        result: IRExpression,
    },
    
    /// Type constraint: symbol has expected_type
    TypeConstraint {
        symbol: Symbol,
        expected_type: IRType,
    },
}

/// Arithmetic operations
#[derive(Debug, Clone, PartialEq)]
pub enum ArithmeticOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Negate,
}

impl ArithmeticOp {
    /// Convert to C operator string
    pub fn to_c_op(&self) -> &'static str {
        match self {
            ArithmeticOp::Add => "+",
            ArithmeticOp::Subtract => "-",
            ArithmeticOp::Multiply => "*",
            ArithmeticOp::Divide => "/",
            ArithmeticOp::Modulo => "%",
            ArithmeticOp::Negate => "-", // Unary minus
        }
    }
    
    /// Check if this operation is unary
    pub fn is_unary(&self) -> bool {
        matches!(self, ArithmeticOp::Negate)
    }
}

/// Mode analysis for arithmetic operations
#[derive(Debug, Clone, PartialEq)]
pub enum ArithmeticMode {
    /// All operands are known values - can inline as direct C arithmetic
    Deterministic,
    /// Some operands are unbound variables - requires constraint solving
    Constraint,
}

/// Analysis result for an arithmetic expression
#[derive(Debug, Clone, PartialEq)]
pub struct ArithmeticAnalysis {
    pub mode: ArithmeticMode,
    pub unbound_vars: Vec<Symbol>,
    pub can_inline: bool,
}

/// Relational operations
#[derive(Debug, Clone, PartialEq)]
pub enum RelationalOp {
    Equal,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
}

/// Logical operations
#[derive(Debug, Clone, PartialEq)]
pub enum LogicalOp {
    And,
    Or,
    Not,
}

/// Types in the IR
#[derive(Debug, Clone, PartialEq)]
pub enum IRType {
    Integer,
    String,
    Boolean,
    Unit,
    List(Box<IRType>),
    Function {
        params: Vec<IRType>,
        result: Box<IRType>,
    },
    Variable(String), // Type variable
    Unknown,
}

/// Determinism analysis
#[derive(Debug, Clone, PartialEq)]
pub enum Determinism {
    /// Always produces the same result for the same inputs
    Deterministic,
    
    /// May suspend waiting for more bindings
    Suspendable,
    
    /// May produce different results (e.g., choice points, effects)
    NonDeterministic,
    
    /// Analysis incomplete or unknown
    Unknown,
}

/// Variable binding status
#[derive(Debug, Clone, PartialEq)]
pub enum BindingStatus {
    /// Variable has a definite value
    Bound,
    
    /// Variable is unbound (free)
    Free,
    
    /// Binding status determined at runtime
    Dynamic,
}

/// IR Builder - converts AST to IR by analyzing actual operations
pub struct IRBuilder<'a> {
    /// AST program being analyzed
    program: &'a ast::Program,
    
    /// Binding status of symbols
    symbol_bindings: HashMap<Symbol, BindingStatus>,
    
    /// Current determinism context
    current_determinism: Determinism,
    
    /// Temporary variable counter
    temp_counter: usize,
    
    /// Function definitions for recursive analysis
    function_defs: HashMap<String, &'a ast::FunctionDef>,
    
    /// Track when we're analyzing the left side of a unification constraint
    in_unification_context: bool,
}

impl<'a> IRBuilder<'a> {
    pub fn new(program: &'a ast::Program) -> Self {
        let mut function_defs = HashMap::new();
        
        // Index all function definitions for recursive analysis
        for func in program.functions() {
            function_defs.insert(func.name.clone(), func);
        }
        
        Self {
            program,
            symbol_bindings: HashMap::new(),
            current_determinism: Determinism::Deterministic,
            temp_counter: 0,
            function_defs,
            in_unification_context: false,
        }
    }
    
    /// Build IR from the AST program
    pub fn build(&mut self) -> IRProgram {
        let mut ir_functions = Vec::new();
        let mut global_constraints = Vec::new();
        let mut symbol_table = HashMap::new();
        
        // Analyze each function
        for func_def in self.program.functions() {
            let ir_function = self.analyze_function_definition(func_def);
            
            // Collect symbols from this function
            for param in &ir_function.parameters {
                symbol_table.insert(param.name.clone(), param.clone());
            }
            
            ir_functions.push(ir_function);
        }
        
        IRProgram {
            functions: ir_functions,
            global_constraints,
            symbol_table,
        }
    }
    
    /// Analyze a function definition to create IR
    fn analyze_function_definition(&mut self, func_def: &ast::FunctionDef) -> IRFunction {
        // Reset analysis state for this function
        self.symbol_bindings.clear();
        self.current_determinism = Determinism::Deterministic;
        
        // Convert parameters to symbols
        let mut parameters = Vec::new();
        
        // For now, we'll analyze the first clause
        // In a full implementation, we'd need to handle multiple clauses
        if let Some(first_clause) = func_def.clauses.first() {
            for pattern in &first_clause.patterns {
                if let ast::Pattern::Var(var) = pattern {
                    let symbol = Symbol::from_variable(var);
                    // Parameters start as free (unbound) since they're inputs
                    self.update_binding_status(&symbol, BindingStatus::Free);
                    parameters.push(symbol);
                }
            }
            
            // Analyze the function body
            let (body, constraints) = self.analyze_function_body(&first_clause.body);
            
            IRFunction {
                name: func_def.name.clone(),
                parameters,
                body,
                determinism: self.current_determinism.clone(),
                constraints,
                binding_analysis: self.symbol_bindings.clone(),
                return_type: self.extract_return_type(func_def),
                effects: self.extract_effects(func_def),
            }
        } else {
            // Empty function
            IRFunction {
                name: func_def.name.clone(),
                parameters,
                body: IRExpression::Literal(Literal::Unit),
                determinism: Determinism::Deterministic,
                constraints: Vec::new(),
                binding_analysis: HashMap::new(),
                return_type: self.extract_return_type(func_def),
                effects: self.extract_effects(func_def),
            }
        }
    }
    
    /// Update binding status for a symbol
    fn update_binding_status(&mut self, symbol: &Symbol, status: BindingStatus) {
        self.symbol_bindings.insert(symbol.clone(), status);
    }
    
    /// Get a function definition by name
    fn get_function_definition(&self, name: &str) -> Option<&ast::FunctionDef> {
        self.function_defs.get(name).copied()
    }
    
    /// Analyze function body to extract constraints from actual operations
    fn analyze_function_body(&mut self, body: &ast::Expr) -> (IRExpression, Vec<IRConstraint>) {
        let mut constraints = Vec::new();
        let ir_expr = self.analyze_expression(body, &mut constraints);
        (ir_expr, constraints)
    }
    
    /// Analyze an expression and extract constraints from operations
    fn analyze_expression(&mut self, expr: &ast::Expr, constraints: &mut Vec<IRConstraint>) -> IRExpression {
        match expr {
            ast::Expr::Var(var) => {
                let symbol = Symbol::from_variable(var);
                self.update_binding_status(&symbol, BindingStatus::Free);
                IRExpression::Symbol(symbol)
            },
            
            ast::Expr::NonConsumptiveVar(var) => {
                let symbol = Symbol::from_variable(var);
                // Non-consumptive variables are typically bound (read-only access)
                self.update_binding_status(&symbol, BindingStatus::Bound);
                IRExpression::Symbol(symbol)
            },
            
            ast::Expr::Int(n) => IRExpression::Literal(Literal::Integer(*n)),
            ast::Expr::Str(s) => IRExpression::Literal(Literal::String(s.clone())),
            ast::Expr::Bool(b) => IRExpression::Literal(Literal::Boolean(*b)),
            ast::Expr::Unit => IRExpression::Literal(Literal::Unit),
            
            ast::Expr::List(exprs) => {
                let elements: Vec<IRExpression> = exprs.iter()
                    .map(|e| self.analyze_expression(e, constraints))
                    .collect();
                IRExpression::List(elements)
            },
            
            ast::Expr::ListCons { head, tail } => {
                let head_ir = self.analyze_expression(head, constraints);
                let tail_ir = self.analyze_expression(tail, constraints);
                
                // Generate a list construction constraint
                let result_symbol = Symbol::temporary(format!("list_cons_{}", self.temp_counter));
                self.temp_counter += 1;
                
                constraints.push(IRConstraint::Unification {
                    left: IRExpression::Symbol(result_symbol.clone()),
                    right: IRExpression::ListCons {
                        head: Box::new(head_ir),
                        tail: Box::new(tail_ir),
                    },
                });
                
                IRExpression::Symbol(result_symbol)
            },
            
            // Binary operations - always generate constraints for now
            ast::Expr::BinOp { op, left, right } => {
                let left_ir = self.analyze_expression(left, constraints);
                let right_ir = self.analyze_expression(right, constraints);
                
                match op {
                    // Arithmetic operations - always use constraints
                    ast::BinOp::Add => {
                        let result_symbol = Symbol::temporary(format!("add_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Arithmetic {
                            operation: ArithmeticOp::Add,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Sub => {
                        let result_symbol = Symbol::temporary(format!("sub_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Arithmetic {
                            operation: ArithmeticOp::Subtract,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Mul => {
                        let result_symbol = Symbol::temporary(format!("mul_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Arithmetic {
                            operation: ArithmeticOp::Multiply,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Div => {
                        let result_symbol = Symbol::temporary(format!("div_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Arithmetic {
                            operation: ArithmeticOp::Divide,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Mod => {
                        let result_symbol = Symbol::temporary(format!("mod_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Arithmetic {
                            operation: ArithmeticOp::Modulo,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    // Relational operations
                    ast::BinOp::Eq => {
                        constraints.push(IRConstraint::Unification {
                            left: left_ir.clone(),
                            right: right_ir.clone(),
                        });
                        
                        // Return a boolean result
                        let result_symbol = Symbol::temporary(format!("eq_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Lt => {
                        constraints.push(IRConstraint::Relational {
                            operation: RelationalOp::LessThan,
                            operands: vec![left_ir, right_ir],
                        });
                        
                        let result_symbol = Symbol::temporary(format!("lt_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Gt => {
                        constraints.push(IRConstraint::Relational {
                            operation: RelationalOp::GreaterThan,
                            operands: vec![left_ir, right_ir],
                        });
                        
                        let result_symbol = Symbol::temporary(format!("gt_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Le => {
                        constraints.push(IRConstraint::Relational {
                            operation: RelationalOp::LessThanOrEqual,
                            operands: vec![left_ir, right_ir],
                        });
                        
                        let result_symbol = Symbol::temporary(format!("le_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Ge => {
                        constraints.push(IRConstraint::Relational {
                            operation: RelationalOp::GreaterThanOrEqual,
                            operands: vec![left_ir, right_ir],
                        });
                        
                        let result_symbol = Symbol::temporary(format!("ge_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::And => {
                        let result_symbol = Symbol::temporary(format!("and_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Logical {
                            operation: LogicalOp::And,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Or => {
                        let result_symbol = Symbol::temporary(format!("or_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Logical {
                            operation: LogicalOp::Or,
                            operands: vec![left_ir, right_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::BinOp::Append => {
                        let result_symbol = Symbol::temporary(format!("append_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Unification {
                            left: IRExpression::Symbol(result_symbol.clone()),
                            right: IRExpression::ListAppend {
                                lists: vec![left_ir, right_ir],
                            },
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    _ => {
                        // For other binary operations, create a generic temporary
                        let result_symbol = Symbol::temporary(format!("binop_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        IRExpression::Symbol(result_symbol)
                    }
                }
            },
            
            // Unary operations
            ast::Expr::UnaryOp { op, expr: inner_expr } => {
                let inner_ir = self.analyze_expression(inner_expr, constraints);
                
                match op {
                    ast::UnaryOp::Not => {
                        let result_symbol = Symbol::temporary(format!("not_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Logical {
                            operation: LogicalOp::Not,
                            operands: vec![inner_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    },
                    
                    ast::UnaryOp::Neg => {
                        let result_symbol = Symbol::temporary(format!("neg_result_{}", self.temp_counter));
                        self.temp_counter += 1;
                        
                        constraints.push(IRConstraint::Arithmetic {
                            operation: ArithmeticOp::Negate,
                            operands: vec![inner_ir],
                            result: IRExpression::Symbol(result_symbol.clone()),
                        });
                        
                        IRExpression::Symbol(result_symbol)
                    }
                }
            },
            
            // Function calls - recursively analyze if we have the function definition
            ast::Expr::Call { func, args } => {
                let args_ir: Vec<IRExpression> = args.iter()
                    .map(|arg| self.analyze_expression(arg, constraints))
                    .collect();
                
                match func.as_ref() {
                    ast::Expr::Var(var) => {
                        let func_name = &var.name;
                        
                        // Check if this is an arithmetic function that can be inlined
                        // Don't inline arithmetic when in unification context - preserve function call structure
                        let inlined_expr = if self.in_unification_context {
                            None
                        } else {
                            self.try_inline_arithmetic_function(func_name, &args_ir)
                        };
                        
                        if let Some(expr) = inlined_expr {
                            // Successfully inlined arithmetic operation
                            expr
                        } else {
                            // Create a function call constraint for non-arithmetic functions
                            let result_symbol = Symbol::temporary(format!("call_result_{}", self.temp_counter));
                            self.temp_counter += 1;
                            
                            constraints.push(IRConstraint::FunctionCall {
                                function: func_name.clone(),
                                arguments: args_ir,
                                result: IRExpression::Symbol(result_symbol.clone()),
                            });
                            
                            // Mark as potentially suspendable if calling user-defined functions
                            if self.get_function_definition(func_name).is_some() {
                                // User-defined functions may suspend for more bindings
                                if self.current_determinism == Determinism::Deterministic {
                                    self.current_determinism = Determinism::Suspendable;
                                }
                            }
                            
                            IRExpression::Symbol(result_symbol)
                        }
                    },
                    _ => {
                        // Complex function expression - analyze it
                        let func_ir = self.analyze_expression(func, constraints);
                        
                        IRExpression::FunctionCall {
                            function: Box::new(func_ir),
                            arguments: args_ir,
                        }
                    }
                }
            },
            
            // C function calls - these are deterministic
            ast::Expr::CCall { module, function, args } => {
                let args_ir: Vec<IRExpression> = args.iter()
                    .map(|arg| self.analyze_expression(arg, constraints))
                    .collect();
                
                IRExpression::CCall {
                    module: module.clone(),
                    function: function.clone(),
                    arguments: args_ir,
                }
            },
            
            // Python function calls - these are deterministic (via Python C API)
            ast::Expr::PythonCall { module, function, args } => {
                let args_ir: Vec<IRExpression> = args.iter()
                    .map(|arg| self.analyze_expression(arg, constraints))
                    .collect();
                
                IRExpression::PythonCall {
                    module: module.clone(),
                    function: function.clone(),
                    arguments: args_ir,
                }
            },
            
            // Let bindings
            ast::Expr::Let { var, value, body } => {
                let value_ir = self.analyze_expression(value, constraints);
                let symbol = Symbol::from_variable(var);
                
                // Mark the variable as bound
                self.update_binding_status(&symbol, BindingStatus::Bound);
                
                // Create a binding constraint
                constraints.push(IRConstraint::Unification {
                    left: IRExpression::Symbol(symbol.clone()),
                    right: value_ir,
                });
                
                // Analyze the body in the context of the binding
                self.analyze_expression(body, constraints)
            },
            
            // Constraint solving
            ast::Expr::LetConstraint { expr, target } => {
                // Set unification context when analyzing the left side (expr)
                let old_context = self.in_unification_context;
                self.in_unification_context = true;
                let expr_ir = self.analyze_expression(expr, constraints);
                self.in_unification_context = old_context;
                
                // Analyze the right side normally
                let target_ir = self.analyze_expression(target, constraints);
                
                // This is an explicit constraint
                constraints.push(IRConstraint::Unification {
                    left: expr_ir,
                    right: target_ir.clone(),
                });
                
                target_ir
            },
            
            // Effect calls - these introduce non-determinism
            ast::Expr::EffectCall { effect, operation, args } => {
                let args_ir: Vec<IRExpression> = args.iter()
                    .map(|arg| self.analyze_expression(arg, constraints))
                    .collect();
                
                // Mark as non-deterministic since it's an effect
                self.current_determinism = Determinism::NonDeterministic;
                
                IRExpression::EffectCall {
                    effect: effect.clone(),
                    operation: operation.clone(),
                    arguments: args_ir,
                }
            },
            
            // Block expressions
            ast::Expr::Block { statements, result } => {
                // Analyze all statements in the block
                for statement in statements {
                    match statement {
                        ast::Statement::Let { var, value } => {
                            // Analyze the value expression and create a binding
                            let value_ir = self.analyze_expression(value, constraints);
                            let var_symbol = Symbol::from_variable(var);
                            
                            // Add unification constraint for let binding
                            constraints.push(IRConstraint::Unification {
                                left: IRExpression::Symbol(var_symbol.clone()),
                                right: value_ir,
                            });
                            
                            // Update binding status
                            self.update_binding_status(&var_symbol, BindingStatus::Bound);
                        },
                        ast::Statement::LetTyped { var, value, .. } => {
                            // Same as Let but with type annotation
                            let value_ir = self.analyze_expression(value, constraints);
                            let var_symbol = Symbol::from_variable(var);
                            
                            constraints.push(IRConstraint::Unification {
                                left: IRExpression::Symbol(var_symbol.clone()),
                                right: value_ir,
                            });
                            
                            self.update_binding_status(&var_symbol, BindingStatus::Bound);
                        },
                        ast::Statement::Expr(expr) => {
                            // Analyze the expression (might be a side effect)
                            self.analyze_expression(expr, constraints);
                        }
                    }
                }
                
                // Then analyze the result expression
                if let Some(result_expr) = result {
                    self.analyze_expression(result_expr, constraints)
                } else {
                    IRExpression::Literal(Literal::Unit)
                }
            },
            
            // Record construction
            ast::Expr::Record { type_name, fields } => {
                let mut ir_fields = Vec::new();
                for (field_name, field_expr) in fields {
                    let field_ir = self.analyze_expression(field_expr, constraints);
                    ir_fields.push((field_name.clone(), field_ir));
                }
                
                IRExpression::Record {
                    type_name: type_name.clone(),
                    fields: ir_fields,
                }
            },
            
            // Field access
            ast::Expr::FieldAccess { expr, field } => {
                let expr_ir = self.analyze_expression(expr, constraints);
                
                IRExpression::FieldAccess {
                    record: Box::new(expr_ir),
                    field: field.clone(),
                }
            },
            
            // Other expressions - implement as needed
            _ => {
                // For now, create a placeholder
                IRExpression::Symbol(Symbol::temporary(format!("placeholder_{}", self.temp_counter)))
            }
        }
    }
    
    /// Analyze if an arithmetic operation can be inlined (deterministic mode)
    fn analyze_arithmetic_mode(&self, left: &IRExpression, right: &IRExpression) -> ArithmeticAnalysis {
        let left_unbound = self.contains_unbound_vars(left);
        let right_unbound = self.contains_unbound_vars(right);
        
        let mut unbound_vars = Vec::new();
        self.collect_unbound_vars(left, &mut unbound_vars);
        self.collect_unbound_vars(right, &mut unbound_vars);
        
        let mode = if left_unbound.is_empty() && right_unbound.is_empty() {
            ArithmeticMode::Deterministic
        } else {
            ArithmeticMode::Constraint
        };
        
        ArithmeticAnalysis {
            mode: mode.clone(),
            unbound_vars,
            can_inline: matches!(mode, ArithmeticMode::Deterministic),
        }
    }
    
    /// Check if an expression contains unbound variables (logic variables)
    fn contains_unbound_vars(&self, expr: &IRExpression) -> Vec<Symbol> {
        let mut unbound_vars = Vec::new();
        self.collect_unbound_vars(expr, &mut unbound_vars);
        unbound_vars
    }
    
    /// Recursively collect unbound variables from an expression
    fn collect_unbound_vars(&self, expr: &IRExpression, vars: &mut Vec<Symbol>) {
        match expr {
            IRExpression::Symbol(symbol) => {
                if symbol.is_logic_var || symbol.is_temporary {
                    // Check if this variable is bound in our current context
                    if !self.symbol_bindings.contains_key(symbol) {
                        vars.push(symbol.clone());
                    }
                }
            },
            IRExpression::InlineArithmetic { left, right, .. } => {
                self.collect_unbound_vars(left, vars);
                self.collect_unbound_vars(right, vars);
            },
            IRExpression::ConstraintArithmetic { operands, .. } => {
                for operand in operands {
                    self.collect_unbound_vars(operand, vars);
                }
            },
            IRExpression::FunctionCall { function, arguments } => {
                self.collect_unbound_vars(function, vars);
                for arg in arguments {
                    self.collect_unbound_vars(arg, vars);
                }
            },
            IRExpression::List(elements) => {
                for elem in elements {
                    self.collect_unbound_vars(elem, vars);
                }
            },
            IRExpression::ListCons { head, tail } => {
                self.collect_unbound_vars(head, vars);
                self.collect_unbound_vars(tail, vars);
            },
            IRExpression::Record { fields, .. } => {
                for (_, field_expr) in fields {
                    self.collect_unbound_vars(field_expr, vars);
                }
            },
            IRExpression::FieldAccess { record, .. } => {
                self.collect_unbound_vars(record, vars);
            },
            // Literals don't contain variables
            IRExpression::Literal(_) => {},
            // Other expression types
            _ => {},
        }
    }
    
    /// Try to inline an arithmetic function call by analyzing the function body
    fn try_inline_arithmetic_function(&mut self, func_name: &str, args: &[IRExpression]) -> Option<IRExpression> {
        // Look up the function definition
        let func_def = self.get_function_definition(func_name)?;
        
        // Analyze the function body to see if it's a simple arithmetic operation
        if let Some(first_clause) = func_def.clauses.first() {
            if let Some(arithmetic_op) = self.extract_arithmetic_operation(&first_clause.body, args.len()) {
                // This function performs a simple arithmetic operation
                
                // Perform mode analysis at the call site
                match args.len() {
                    2 => {
                        let left = &args[0];
                        let right = &args[1];
                        let analysis = self.analyze_arithmetic_mode(left, right);
                        
                        match analysis.mode {
                            ArithmeticMode::Deterministic => {
                                // All arguments are known - inline the arithmetic
                                Some(IRExpression::InlineArithmetic {
                                    operation: arithmetic_op,
                                    left: Box::new(left.clone()),
                                    right: Box::new(right.clone()),
                                })
                            },
                            ArithmeticMode::Constraint => {
                                // Some arguments are unbound - create constraint arithmetic
                                let result_symbol = Symbol::temporary(format!("{}_result_{}", func_name, self.temp_counter));
                                self.temp_counter += 1;
                                
                                Some(IRExpression::ConstraintArithmetic {
                                    operation: arithmetic_op,
                                    operands: vec![left.clone(), right.clone()],
                                    result: result_symbol,
                                })
                            }
                        }
                    },
                    1 if arithmetic_op.is_unary() => {
                        // Unary operation
                        let operand = &args[0];
                        let unbound_vars = self.contains_unbound_vars(operand);
                        
                        if unbound_vars.is_empty() {
                            // Deterministic unary operation
                            Some(IRExpression::InlineArithmetic {
                                operation: arithmetic_op,
                                left: Box::new(operand.clone()),
                                right: Box::new(IRExpression::Literal(Literal::Unit)), // Placeholder for unary
                            })
                        } else {
                            // Constraint-based unary operation
                            let result_symbol = Symbol::temporary(format!("{}_result_{}", func_name, self.temp_counter));
                            self.temp_counter += 1;
                            
                            Some(IRExpression::ConstraintArithmetic {
                                operation: arithmetic_op,
                                operands: vec![operand.clone()],
                                result: result_symbol,
                            })
                        }
                    },
                    _ => None, // Unsupported arity for arithmetic
                }
            } else {
                None // Not an arithmetic function
            }
        } else {
            None // No function body
        }
    }
    
    /// Extract arithmetic operation from a function body expression
    fn extract_arithmetic_operation(&self, body: &ast::Expr, expected_arity: usize) -> Option<ArithmeticOp> {
        match body {
            // Direct binary operations
            ast::Expr::BinOp { op, left, right } => {
                // Check if this is a simple arithmetic operation on parameters
                if expected_arity == 2 && self.is_parameter_reference(left) && self.is_parameter_reference(right) {
                    match op {
                        ast::BinOp::Add => Some(ArithmeticOp::Add),
                        ast::BinOp::Sub => Some(ArithmeticOp::Subtract),
                        ast::BinOp::Mul => Some(ArithmeticOp::Multiply),
                        ast::BinOp::Div => Some(ArithmeticOp::Divide),
                        ast::BinOp::Mod => Some(ArithmeticOp::Modulo),
                        _ => None,
                    }
                } else {
                    None
                }
            },
            // Direct unary operations
            ast::Expr::UnaryOp { op, expr } => {
                if expected_arity == 1 && self.is_parameter_reference(expr) {
                    match op {
                        ast::UnaryOp::Neg => Some(ArithmeticOp::Negate),
                        _ => None,
                    }
                } else {
                    None
                }
            },
            // Look through let bindings and other wrappers
            ast::Expr::Let { body, .. } => self.extract_arithmetic_operation(body, expected_arity),
            ast::Expr::Block { result: Some(result), .. } => self.extract_arithmetic_operation(result, expected_arity),
            _ => None,
        }
    }
    
    /// Check if an expression is a simple parameter reference
    fn is_parameter_reference(&self, expr: &ast::Expr) -> bool {
        match expr {
            ast::Expr::Var(_) => true,
            ast::Expr::NonConsumptiveVar(_) => true,
            _ => false,
        }
    }
    
    /// Analyze an arithmetic operation and decide between inline and constraint modes
    fn analyze_arithmetic_operation(
        &mut self, 
        op: ArithmeticOp, 
        left: IRExpression, 
        right: IRExpression, 
        constraints: &mut Vec<IRConstraint>
    ) -> IRExpression {
        let analysis = self.analyze_arithmetic_mode(&left, &right);
        
        match analysis.mode {
            ArithmeticMode::Deterministic => {
                // All operands are known - generate inline arithmetic
                IRExpression::InlineArithmetic {
                    operation: op,
                    left: Box::new(left),
                    right: Box::new(right),
                }
            },
            ArithmeticMode::Constraint => {
                // Some operands are unbound - generate constraint
                let result_symbol = Symbol::temporary(format!("{}_result_{}", 
                    match op {
                        ArithmeticOp::Add => "add",
                        ArithmeticOp::Subtract => "sub", 
                        ArithmeticOp::Multiply => "mul",
                        ArithmeticOp::Divide => "div",
                        ArithmeticOp::Modulo => "mod",
                        ArithmeticOp::Negate => "neg",
                    }, self.temp_counter));
                self.temp_counter += 1;
                
                IRExpression::ConstraintArithmetic {
                    operation: op.clone(),
                    operands: vec![left, right],
                    result: result_symbol.clone(),
                }
            }
        }
    }
}

/// Pretty printing for IR expressions
impl std::fmt::Display for IRExpression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            IRExpression::Symbol(symbol) => write!(f, "{}", symbol.name),
            IRExpression::Literal(lit) => match lit {
                Literal::Integer(n) => write!(f, "{}", n),
                Literal::String(s) => write!(f, "\"{}\"", s),
                Literal::Boolean(b) => write!(f, "{}", b),
                Literal::Unit => write!(f, "()"),
            },
            IRExpression::List(elements) => {
                write!(f, "[")?;
                for (i, elem) in elements.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{}", elem)?;
                }
                write!(f, "]")
            },
            IRExpression::ListCons { head, tail } => {
                write!(f, "[{}|{}]", head, tail)
            },
            IRExpression::FunctionCall { function, arguments } => {
                write!(f, "{}(", function)?;
                for (i, arg) in arguments.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{}", arg)?;
                }
                write!(f, ")")
            },
            IRExpression::CCall { module, function, arguments } => {
                write!(f, "C.{}.{}(", module, function)?;
                for (i, arg) in arguments.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{}", arg)?;
                }
                write!(f, ")")
            },
            IRExpression::PythonCall { module, function, arguments } => {
                write!(f, "Py.{}.{}(", module, function)?;
                for (i, arg) in arguments.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{}", arg)?;
                }
                write!(f, ")")
            },
            IRExpression::InlineArithmetic { operation, left, right } => {
                write!(f, "({} {} {})", left, operation.to_c_op(), right)
            },
            IRExpression::ConstraintArithmetic { operation, operands, result } => {
                if operands.len() == 2 {
                    write!(f, "constraint({} {} {} = {})", operands[0], operation.to_c_op(), operands[1], result.name)
                } else if operands.len() == 1 && operation.is_unary() {
                    write!(f, "constraint({}{} = {})", operation.to_c_op(), operands[0], result.name)
                } else {
                    write!(f, "constraint({:?} {:?} = {})", operation, operands, result.name)
                }
            },
            _ => write!(f, "<complex-expr>"),
        }
    }
}

/// Pretty printing for constraints
impl std::fmt::Display for IRConstraint {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            IRConstraint::Arithmetic { operation, operands, result } => {
                match operation {
                    ArithmeticOp::Add => write!(f, "{} = {} + {}", result, operands[0], operands[1]),
                    ArithmeticOp::Subtract => write!(f, "{} = {} - {}", result, operands[0], operands[1]),
                    ArithmeticOp::Multiply => write!(f, "{} = {} * {}", result, operands[0], operands[1]),
                    ArithmeticOp::Divide => write!(f, "{} = {} / {}", result, operands[0], operands[1]),
                    ArithmeticOp::Modulo => write!(f, "{} = {} % {}", result, operands[0], operands[1]),
                    ArithmeticOp::Negate => write!(f, "{} = -{}", result, operands[0]),
                }
            },
            IRConstraint::Relational { operation, operands } => {
                let op_str = match operation {
                    RelationalOp::Equal => "=",
                    RelationalOp::NotEqual => "≠",
                    RelationalOp::LessThan => "<",
                    RelationalOp::LessThanOrEqual => "≤",
                    RelationalOp::GreaterThan => ">",
                    RelationalOp::GreaterThanOrEqual => "≥",
                };
                write!(f, "{} {} {}", operands[0], op_str, operands[1])
            },
            IRConstraint::Logical { operation, operands, result } => {
                match operation {
                    LogicalOp::And => write!(f, "{} = {} ∧ {}", result, operands[0], operands[1]),
                    LogicalOp::Or => write!(f, "{} = {} ∨ {}", result, operands[0], operands[1]),
                    LogicalOp::Not => write!(f, "{} = ¬{}", result, operands[0]),
                }
            },
            IRConstraint::Unification { left, right } => {
                write!(f, "{} = {}", left, right)
            },
            IRConstraint::FunctionCall { function, arguments, result } => {
                write!(f, "{} = {}(", result, function)?;
                for (i, arg) in arguments.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{}", arg)?;
                }
                write!(f, ")")
            },
            IRConstraint::TypeConstraint { symbol, expected_type } => {
                write!(f, "{} :: {:?}", symbol.name, expected_type)
            },
        }
    }
}

use std::fmt;

impl fmt::Display for IRProgram {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "=== FLINT INTERMEDIATE REPRESENTATION ===")?;
        writeln!(f)?;
        
        // Global symbol table
        writeln!(f, "SYMBOL TABLE:")?;
        for (name, symbol) in &self.symbol_table {
            writeln!(f, "  {} -> {}", name, symbol)?;
        }
        writeln!(f)?;
        
        // Global constraints
        if !self.global_constraints.is_empty() {
            writeln!(f, "GLOBAL CONSTRAINTS:")?;
            for constraint in &self.global_constraints {
                writeln!(f, "  {}", constraint)?;
            }
            writeln!(f)?;
        }
        
        // Functions
        writeln!(f, "FUNCTIONS:")?;
        for function in &self.functions {
            writeln!(f, "{}", function)?;
        }
        
        Ok(())
    }
}

impl fmt::Display for IRFunction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "function {}:", self.name)?;
        
        // Parameters
        write!(f, "  parameters: ")?;
        for (i, param) in self.parameters.iter().enumerate() {
            if i > 0 { write!(f, ", ")?; }
            write!(f, "{}", param)?;
        }
        writeln!(f)?;
        
        // Return type
        writeln!(f, "  return type: {:?}", self.return_type)?;
        
        // Effects
        if !self.effects.is_empty() {
            write!(f, "  effects: ")?;
            for (i, effect) in self.effects.iter().enumerate() {
                if i > 0 { write!(f, ", ")?; }
                write!(f, "{}", effect)?;
            }
            writeln!(f)?;
        }
        
        // Determinism
        writeln!(f, "  determinism: {:?}", self.determinism)?;
        
        // Body
        writeln!(f, "  body: {}", self.body)?;
        
        // Constraints
        if !self.constraints.is_empty() {
            writeln!(f, "  constraints:")?;
            for constraint in &self.constraints {
                writeln!(f, "    {}", constraint)?;
            }
        }
        
        // Binding analysis
        if !self.binding_analysis.is_empty() {
            writeln!(f, "  variable bindings:")?;
            for (symbol, status) in &self.binding_analysis {
                writeln!(f, "    {} -> {:?}", symbol, status)?;
            }
        }
        
        writeln!(f)
    }
}

impl fmt::Display for Symbol {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_temporary {
            write!(f, "%{}", self.name)
        } else if self.is_logic_var {
            write!(f, "${}", self.name)
        } else {
            write!(f, "{}", self.name)
        }
    }
}

impl<'a> IRBuilder<'a> {
    /// Extract return type from function definition
    fn extract_return_type(&self, func_def: &ast::FunctionDef) -> IRType {
        // Look up function signature in the program
        for (name, type_sig) in self.program.function_signatures() {
            if name == &func_def.name {
                if let ast::FlintType::Function { result, .. } = type_sig {
                    return self.ast_type_to_ir_type(result);
                }
            }
        }
        
        // If no signature found, try to infer from function definition
        if let Some(ref type_signature) = func_def.type_signature {
            if let ast::FlintType::Function { result, .. } = type_signature {
                return self.ast_type_to_ir_type(result);
            } else {
                return self.ast_type_to_ir_type(type_signature);
            }
        }
        
        IRType::Unknown
    }
    
    /// Extract effects from function definition
    fn extract_effects(&self, func_def: &ast::FunctionDef) -> Vec<String> {
        // Look up function signature in the program  
        for (name, type_sig) in self.program.function_signatures() {
            if name == &func_def.name {
                if let ast::FlintType::Function { effects, .. } = type_sig {
                    return effects.clone();
                }
            }
        }
        
        // If no signature found, try to use function definition's type signature
        if let Some(ast::FlintType::Function { effects, .. }) = &func_def.type_signature {
            effects.clone()
        } else {
            Vec::new()
        }
    }
    
    /// Convert AST type to IR type
    fn ast_type_to_ir_type(&self, ast_type: &ast::FlintType) -> IRType {
        match ast_type {
            ast::FlintType::Int32 => IRType::Integer,
            ast::FlintType::String => IRType::String,
            ast::FlintType::Bool => IRType::Boolean,
            ast::FlintType::Unit => IRType::Unit,
            ast::FlintType::List(elem_type) => {
                IRType::List(Box::new(self.ast_type_to_ir_type(elem_type)))
            },
            ast::FlintType::Function { params, result, effects } => {
                let param_types = params.iter().map(|p| self.ast_type_to_ir_type(p)).collect();
                IRType::Function {
                    params: param_types,
                    result: Box::new(self.ast_type_to_ir_type(result)),
                }
            },
            ast::FlintType::Named(name) => IRType::Variable(name.clone()),
            ast::FlintType::TypeVar(name) => IRType::Variable(name.clone()),
            _ => IRType::Unknown,
        }
    }
}
