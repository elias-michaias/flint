// Code generation for Flint functional logic language
use crate::ast::*;
use crate::diagnostic::{Diagnostic, SourceLocation};
use crate::package::PackageManager;
use std::collections::{HashMap, HashSet};
use std::fmt::Write;

#[derive(Debug, thiserror::Error)]
pub enum CodegenError {
    #[error("Code generation error")]
    Diagnostic(Diagnostic),
    
    #[error("IO error: {0}")]
    IoError(#[from] std::fmt::Error),
}

impl CodegenError {
    pub fn unsupported_feature(feature: &str) -> Self {
        let diagnostic = Diagnostic::error(format!("Unsupported feature: {}", feature))
            .with_help("This feature is not yet implemented in the code generator".to_string());
        Self::Diagnostic(diagnostic)
    }
    
    pub fn unsupported_feature_at(feature: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Unsupported feature: {}", feature))
            .with_location(location)
            .with_help("This feature is not yet implemented in the code generator".to_string());
        Self::Diagnostic(diagnostic)
    }
    
    pub fn invalid_ast(message: &str) -> Self {
        let diagnostic = Diagnostic::error(format!("Invalid AST: {}", message))
            .with_help("The AST structure is malformed or contains invalid data".to_string());
        Self::Diagnostic(diagnostic)
    }
    
    pub fn invalid_ast_at(message: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Invalid AST: {}", message))
            .with_location(location)
            .with_help("The AST structure is malformed or contains invalid data".to_string());
        Self::Diagnostic(diagnostic)
    }
    
    pub fn type_error(message: &str) -> Self {
        let diagnostic = Diagnostic::error(format!("Type error: {}", message))
            .with_help("There is a type mismatch or invalid type usage".to_string());
        Self::Diagnostic(diagnostic)
    }
    
    pub fn type_error_at(message: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Type error: {}", message))
            .with_location(location)
            .with_help("There is a type mismatch or invalid type usage".to_string());
        Self::Diagnostic(diagnostic)
    }
}

// Metadata for resource consumption tracking
#[derive(Debug, Clone)]
struct ConsumptionMetadata {
    resource_name: String,
    consumption_point: String,
    is_optional: bool,
    is_persistent: bool,
    estimated_size: usize,
}

// Information about functions for analysis
#[derive(Debug, Clone)]
struct FunctionInfo {
    name: String,
    is_deterministic: bool,
    has_logic_vars: bool,
    effects: Vec<String>,
}

// Information about effect handlers
#[derive(Debug, Clone)]
struct EffectHandlerInfo {
    effect_name: String,
    handler_name: String,
    operation_functions: HashMap<String, String>,
}

// Compilation context for expressions
#[derive(Debug, Clone)]
struct CompilationContext {
    in_logic_context: bool,
    effect_handlers: Vec<String>,
    filename: String,
    source: String,
}

impl CompilationContext {
    fn new() -> Self {
        Self {
            in_logic_context: false,
            effect_handlers: Vec::new(),
            filename: "<unknown>".to_string(),
            source: String::new(),
        }
    }
    
    fn with_source(filename: String, source: String) -> Self {
        Self {
            in_logic_context: false,
            effect_handlers: Vec::new(),
            filename,
            source,
        }
    }
}

/// Code generator for Flint functional logic language
pub struct CodeGenerator {
    output: String,
    var_counter: usize,
    label_counter: usize,
    consumption_metadata: Vec<ConsumptionMetadata>,
    functions: HashMap<String, FunctionInfo>,
    effect_handlers: HashMap<String, EffectHandlerInfo>,
    declared_variables: HashSet<String>,
    // Track total occurrences of each variable across all constraints
    variable_occurrences: HashMap<String, usize>,
    // Track current occurrence number for each variable (for linearity)
    current_variable_occurrence: HashMap<String, usize>,
    defined_functions: HashSet<String>,  // Track functions that need registration
    // Track variables consumed in the current constraint for immediate freeing
    consumed_variables_in_constraint: Vec<String>,
    debug: bool,
    package_manager: Option<*const PackageManager>,
}

impl CodeGenerator {
    /// Create a new code generator
    pub fn new() -> Self {
        Self {
            output: String::new(),
            var_counter: 0,
            label_counter: 0,
            consumption_metadata: Vec::new(),
            functions: HashMap::new(),
            effect_handlers: HashMap::new(),
            declared_variables: HashSet::new(),
            variable_occurrences: HashMap::new(),
            current_variable_occurrence: HashMap::new(),
            defined_functions: HashSet::new(),
            consumed_variables_in_constraint: Vec::new(),
            debug: false,
            package_manager: None,
        }
    }

    /// Create a new code generator with a package manager
    pub fn with_package_manager(mut self, package_manager: &PackageManager) -> Self {
        self.package_manager = Some(package_manager as *const PackageManager);
        self.consumed_variables_in_constraint = Vec::new();
        self
    }

    /// Enable debug output
    pub fn with_debug(mut self, debug: bool) -> Self {
        self.debug = debug;
        self
    }

    fn fresh_var(&mut self) -> String {
        let var = format!("v{}", self.var_counter);
        self.var_counter += 1;
        var
    }

    fn fresh_label(&mut self) -> String {
        let label = format!("L{}", self.label_counter);
        self.label_counter += 1;
        label
    }

    fn fresh_var_with_prefix(&mut self, prefix: &str) -> String {
        let var = format!("{}_{}", prefix, self.var_counter);
        self.var_counter += 1;
        var
    }

    /// Convert a Flint variable to a C variable name, handling linearity
    fn var_to_c_name(&mut self, var: &Variable) -> String {
        let var_name = if var.is_logic_var {
            // For logic variables, just use the name without the $ prefix
            var.name.clone()
        } else {
            format!("var_{}", var.name)
        };
        
        // Track variable occurrences for linearity
        let occurrence_count = self.variable_occurrences.entry(var_name.clone()).or_insert(0);
        *occurrence_count += 1;
        
        var_name
    }
    
    /// Check if this is the first occurrence of a variable (binding/initialization)
    fn is_first_occurrence(&self, var: &Variable) -> bool {
        let var_name = if var.is_logic_var {
            var.name.clone()
        } else {
            format!("var_{}", var.name)
        };
        
        self.variable_occurrences.get(&var_name).map_or(true, |&count| count == 0)
    }
    
    /// Generate variable access with proper consumption handling
    fn generate_variable_access(&mut self, var: &Variable, is_non_consumptive: bool) -> String {
        let var_name = self.var_to_c_name(var);
        
        // Call_result variables are temporary containers and should not be consumed
        if var_name.starts_with("call_result_") {
            return var_name;
        }
        
        if is_non_consumptive {
            // Non-consumptive access with ~ - create a copy for sharing
            format!("flint_copy_for_sharing({})", var_name)
        } else {
            // Track which occurrence this is for the variable
            let current_occurrence = self.current_variable_occurrence.entry(var_name.clone()).or_insert(0);
            *current_occurrence += 1;
            
            if *current_occurrence == 1 {
                // First occurrence - this is initialization/binding, not consumption
                var_name
            } else {
                // Subsequent occurrence - this consumes the value
                // Track this variable for immediate freeing after the constraint
                self.consumed_variables_in_constraint.push(var_name.clone());
                format!("flint_consume_value({}, LINEAR_OP_VARIABLE_USE)", var_name)
            }
        }
    }

    /// Generate freeing calls for variables consumed in the current constraint
    fn generate_consumed_variable_freeing(&mut self, is_main_function: bool) -> Result<(), CodegenError> {
        // Only free call_result variables, not user variables like a, z, etc.
        if is_main_function && !self.consumed_variables_in_constraint.is_empty() {
            writeln!(self.output, "    // Free call_result variables after use")?;
            for var_name in &self.consumed_variables_in_constraint {
                if var_name.starts_with("call_result_") {
                    writeln!(self.output, "    flint_free_value({});", var_name)?;
                }
                // Don't free user variables (a, z, as, bs, etc.) as they may be used later
            }
        }
        // Always clear the consumed variables list
        self.consumed_variables_in_constraint.clear();
        Ok(())
    }

    /// Add consumption metadata for a resource
    fn add_consumption_metadata(&mut self, resource_name: String, point: String, is_optional: bool, is_persistent: bool) {
        self.consumption_metadata.push(ConsumptionMetadata {
            resource_name,
            consumption_point: point,
            is_optional,
            is_persistent,
            estimated_size: 64, // Default estimate
        });
    }

    /// Check if an expression contains logic variables
    fn expression_contains_logic_vars(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Var(var) => var.is_logic_var,
            Expr::NonConsumptiveVar(var) => var.is_logic_var,
            Expr::Int(_) | Expr::Str(_) | Expr::Bool(_) | Expr::Unit => false,
            Expr::List(elements) => elements.iter().any(|e| self.expression_contains_logic_vars(e)),
            Expr::ListCons { head, tail } => {
                self.expression_contains_logic_vars(head) || self.expression_contains_logic_vars(tail)
            }
            Expr::BinOp { left, right, .. } => {
                self.expression_contains_logic_vars(left) || self.expression_contains_logic_vars(right)
            }
            Expr::Call { func, args } => {
                self.expression_contains_logic_vars(func) || 
                args.iter().any(|arg| self.expression_contains_logic_vars(arg))
            }
            Expr::Let { value, body, .. } => {
                self.expression_contains_logic_vars(value) || self.expression_contains_logic_vars(body)
            }
            Expr::LetConstraint { expr, target } => {
                self.expression_contains_logic_vars(expr) || self.expression_contains_logic_vars(target)
            }
            Expr::Block { statements, result } => {
                statements.iter().any(|stmt| self.statement_contains_logic_vars(stmt)) ||
                result.as_ref().map_or(false, |r| self.expression_contains_logic_vars(r))
            }
            Expr::CCall { args, .. } => {
                args.iter().any(|arg| self.expression_contains_logic_vars(arg))
            }
            Expr::PythonCall { args, .. } => {
                args.iter().any(|arg| self.expression_contains_logic_vars(arg))
            }
            _ => false,
        }
    }

    /// Check if a statement contains logic variables
    fn statement_contains_logic_vars(&self, stmt: &Statement) -> bool {
        match stmt {
            Statement::Let { value, .. } => self.expression_contains_logic_vars(value),
            Statement::LetTyped { value, .. } => self.expression_contains_logic_vars(value),
            Statement::Expr(expr) => self.expression_contains_logic_vars(expr),
        }
    }

    /// Check if this is a simple logic variable access (not an expression)
    fn is_simple_logic_var_access(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Var(var) => var.is_logic_var,
            Expr::NonConsumptiveVar(var) => var.is_logic_var,
            _ => false,
        }
    }

    /// Generate a suspension for an expression with logic variables
    fn generate_suspension(&mut self, expr: &Expr, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        // Collect all logic variables that this expression depends on
        let mut deps = HashSet::new();
        self.collect_logic_vars_in_expr(expr, &mut deps);
        
        // For now, create a simple thunk that can be forced later
        // We'll generate code that creates a suspension value
        let suspension_var = self.fresh_var_with_prefix("susp");
        
        // Generate the computation as a lambda-like structure
        // For BinOp expressions, create a specialized suspension
        match expr {
            Expr::BinOp { op, left, right } => {
                let left_code = if self.expression_contains_logic_vars(left) {
                    self.var_to_c_name(&match left.as_ref() {
                        Expr::Var(v) => v.clone(),
                        _ => return Err(CodegenError::unsupported_feature("complex left operand in suspension")),
                    })
                } else {
                    self.generate_expr_immediate(left, ctx)?
                };
                
                let right_code = if self.expression_contains_logic_vars(right) {
                    self.var_to_c_name(&match right.as_ref() {
                        Expr::Var(v) => v.clone(),
                        _ => return Err(CodegenError::unsupported_feature("complex right operand in suspension")),
                    })
                } else {
                    self.generate_expr_immediate(right, ctx)?
                };
                
                let op_name = match op {
                    BinOp::Add => "add",
                    BinOp::Sub => "sub",
                    BinOp::Mul => "mul",
                    BinOp::Div => "div",
                    _ => "unknown",
                };
                
                Ok(format!("flint_create_arithmetic_suspension(\"{}\", {}, {})", op_name, left_code, right_code))
            }
            
            Expr::Call { func, args } => {
                let func_name = match func.as_ref() {
                    Expr::Var(var) => var.name.clone(),
                    _ => return Err(CodegenError::unsupported_feature("complex function in suspension")),
                };
                
                let arg_codes: Result<Vec<_>, _> = args.iter()
                    .map(|arg| {
                        if self.expression_contains_logic_vars(arg) {
                            match arg {
                                Expr::Var(v) => Ok(self.var_to_c_name(v)),
                                _ => Err(CodegenError::unsupported_feature("complex argument in suspension")),
                            }
                        } else {
                            self.generate_expr_immediate(arg, ctx)
                        }
                    })
                    .collect();
                let arg_codes = arg_codes?;
                
                Ok(format!("flint_create_function_call_suspension(\"{}\", {})", 
                          func_name, 
                          if arg_codes.is_empty() { 
                              "NULL, 0".to_string() 
                          } else { 
                              format!("(Value*[]){{{}}}, {}", arg_codes.join(", "), arg_codes.len())
                          }))
            }
            
            _ => {
                // For other expression types, create a generic suspension
                Ok(format!("flint_create_generic_suspension()"))
            }
        }
    }

    /// Generate immediate code for expressions (without suspension)
    fn generate_expr_immediate(&mut self, expr: &Expr, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        // This generates expressions assuming all variables are bound
        match expr {
            Expr::Var(var) => {
                if var.is_logic_var {
                    Ok(format!("flint_force_value({})", self.var_to_c_name(var)))
                } else {
                    Ok(self.var_to_c_name(var))
                }
            }
            
            Expr::NonConsumptiveVar(var) => {
                if var.is_logic_var {
                    Ok(format!("flint_force_value({})", self.var_to_c_name(var)))
                } else {
                    Ok(self.var_to_c_name(var))
                }
            }
            
            Expr::Int(n) => {
                Ok(format!("flint_create_integer({})", n))
            }
            
            Expr::BinOp { op, left, right } => {
                let left_code = self.generate_expr_immediate(left, ctx)?;
                let right_code = self.generate_expr_immediate(right, ctx)?;
                
                match op {
                    BinOp::Add => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer + flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Sub => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer - flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Mul => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer * flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Div => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer / flint_deref({})->data.integer)", left_code, right_code))
                    }
                    _ => {
                        // For other operations, return an error for now
                        Err(CodegenError::unsupported_feature("complex binary operation in immediate generation"))
                    }
                }
            }
            
            _ => {
                // For other expression types, return an error for now
                Err(CodegenError::unsupported_feature("complex expression in immediate generation"))
            }
        }
    }

    /// Main expression generation function with lazy evaluation support
    fn generate_expr(&mut self, expr: &Expr, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        // Handle LetConstraint first, before suspension logic
        if let Expr::LetConstraint { expr: constraint_expr, target } = expr {
            println!("[DEBUG] Processing LetConstraint in generate_expr: expr={:?}, target={:?}", constraint_expr, target);
            // Enhanced constraint solving code generation
            // Instead of evaluating the expression with unbound variables,
            // we analyze the structure and set up proper constraints
            
            if let Some(constraint_code) = self.try_generate_smart_constraint(constraint_expr, target, ctx)? {
                return Ok(constraint_code);
            } else {
                // Fallback to runtime constraint solving
                let expr_code = self.generate_expr(constraint_expr, ctx)?;
                let target_code = self.generate_expr(target, ctx)?;
                return Ok(format!("(flint_solve_constraint({}, {}))", expr_code, target_code));
            }
        }
        
        // Check if this expression contains logic variables and should be suspended
        if self.expression_contains_logic_vars(expr) && !self.is_simple_logic_var_access(expr) {
            // Create a suspension for this expression
            return self.generate_suspension(expr, ctx);
        }

        match expr {
            Expr::Var(var) => {
                // Generate variable access with consumption tracking
                Ok(self.generate_variable_access(var, false))
            }
            
            Expr::NonConsumptiveVar(var) => {
                // Non-consumptive variables are marked with ~, no consumption
                Ok(self.generate_variable_access(var, true))
            }
            
            Expr::Int(n) => {
                // For constraint contexts, always create a Value object
                Ok(format!("flint_create_integer({})", n))
            }
            
            Expr::Str(s) => {
                Ok(format!("\"{}\"", s.replace("\"", "\\\"")))
            }
            
            Expr::Bool(b) => {
                Ok(if *b { "true".to_string() } else { "false".to_string() })
            }
            
            Expr::Unit => {
                Ok("((void)0)".to_string())
            }
            
            Expr::List(elements) => {
                let element_codes: Result<Vec<_>, _> = elements.iter()
                    .map(|e| self.generate_expr(e, ctx))
                    .collect();
                let element_codes = element_codes?;
                
                Ok(format!("flint_create_list({})", element_codes.join(", ")))
            }
            
            Expr::ListCons { head, tail } => {
                let head_code = self.generate_expr(head, ctx)?;
                let tail_code = self.generate_expr(tail, ctx)?;
                Ok(format!("flint_list_cons({}, {})", head_code, tail_code))
            }
            
            Expr::CCall { module, function, args } => {
                self.generate_c_call_expression(module, function, args)
            }
            
            Expr::PythonCall { module, function, args } => {
                self.generate_python_call_expression(module, function, args)
            }
            
            Expr::Block { statements, result } => {
                self.generate_block_expression(statements, result.as_deref(), ctx)
            }
            
            Expr::BinOp { op, left, right } => {
                // Generate both operands as Value* objects
                let left_code = self.generate_expr(left, ctx)?;
                let right_code = self.generate_expr(right, ctx)?;
                
                // For arithmetic operations, we need to handle Value* objects properly
                match op {
                    BinOp::Add => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer + flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Sub => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer - flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Mul => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer * flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Div => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer / flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Eq => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer == flint_deref({})->data.integer ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Ne => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer != flint_deref({})->data.integer ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Lt => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer < flint_deref({})->data.integer ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Le => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer <= flint_deref({})->data.integer ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Gt => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer > flint_deref({})->data.integer ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Ge => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer >= flint_deref({})->data.integer ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::And => {
                        Ok(format!("flint_create_integer((flint_deref({})->data.integer && flint_deref({})->data.integer) ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Or => {
                        Ok(format!("flint_create_integer((flint_deref({})->data.integer || flint_deref({})->data.integer) ? 1 : 0)", left_code, right_code))
                    }
                    BinOp::Mod => {
                        Ok(format!("flint_create_integer(flint_deref({})->data.integer % flint_deref({})->data.integer)", left_code, right_code))
                    }
                    BinOp::Append => {
                        // For now, treat append as string concatenation (simplified)
                        Ok(format!("flint_create_string(strcat((char*)flint_deref({}), (char*)flint_deref({})))", left_code, right_code))
                    }
                }
            }
            
            Expr::Call { func, args } => {
                // Generate arguments
                let arg_codes: Result<Vec<_>, _> = args.iter()
                    .map(|arg| self.generate_expr(arg, ctx))
                    .collect();
                let arg_codes = arg_codes?;
                
                // Handle function name specially - don't add var_ prefix
                let func_name = match func.as_ref() {
                    Expr::Var(var) => {
                        // For function calls, use the raw function name without var_ prefix
                        var.name.clone()
                    }
                    _ => {
                        // For more complex function expressions, generate normally
                        self.generate_expr(func, ctx)?
                    }
                };
                
                // Generate function call
                Ok(format!("{}({})", func_name, arg_codes.join(", ")))
            }
            
            Expr::Let { var, value, body } => {
                // Generate let binding
                let value_code = self.generate_expr(value, ctx)?;
                let body_code = self.generate_expr(body, ctx)?;
                let var_name = self.var_to_c_name(var);
                
                if var.is_logic_var {
                    // For logic variables, create a logical variable and bind it to the value
                    Ok(format!("({{ Value* {} = flint_create_logical_var(true); flint_unify({}, {}, flint_get_global_env()); {}; }})", 
                              var_name, var_name, value_code, body_code))
                } else {
                    // For regular variables, use void* type
                    Ok(format!("({{ void* {} = {}; {}; }})", var_name, value_code, body_code))
                }
            }
            
            _ => {
                // For unsupported expression types, provide error
                Err(CodegenError::unsupported_feature(
                    &format!("expression type {}", self.expr_type_name(expr))
                ))
            }
        }
    }

    /// Collect all logic variables referenced in an expression (implementation for missing method)
    fn collect_logic_vars_in_expr(&mut self, expr: &Expr, vars: &mut HashSet<String>) {
        match expr {
            Expr::Var(var) if var.is_logic_var => {
                vars.insert(var.name.clone());
            }
            Expr::NonConsumptiveVar(var) if var.is_logic_var => {
                vars.insert(var.name.clone());
            }
            Expr::List(elements) => {
                for element in elements {
                    self.collect_logic_vars_in_expr(element, vars);
                }
            }
            Expr::ListCons { head, tail } => {
                self.collect_logic_vars_in_expr(head, vars);
                self.collect_logic_vars_in_expr(tail, vars);
            }
            Expr::BinOp { left, right, .. } => {
                self.collect_logic_vars_in_expr(left, vars);
                self.collect_logic_vars_in_expr(right, vars);
            }
            Expr::Call { func, args } => {
                self.collect_logic_vars_in_expr(func, vars);
                for arg in args {
                    self.collect_logic_vars_in_expr(arg, vars);
                }
            }
            Expr::Let { value, body, .. } => {
                self.collect_logic_vars_in_expr(value, vars);
                self.collect_logic_vars_in_expr(body, vars);
            }
            Expr::LetConstraint { expr, target } => {
                self.collect_logic_vars_in_expr(expr, vars);
                self.collect_logic_vars_in_expr(target, vars);
            }
            Expr::Block { statements, result } => {
                for stmt in statements {
                    self.collect_logic_vars_in_statement(stmt, vars);
                }
                if let Some(result_expr) = result {
                    self.collect_logic_vars_in_expr(result_expr, vars);
                }
            }
            Expr::CCall { args, .. } => {
                for arg in args {
                    self.collect_logic_vars_in_expr(arg, vars);
                }
            }
            Expr::PythonCall { args, .. } => {
                for arg in args {
                    self.collect_logic_vars_in_expr(arg, vars);
                }
            }
            _ => {}
        }
    }

    /// Collect logic variables from statements (implementation for missing method)
    fn collect_logic_vars_in_statement(&mut self, statement: &Statement, vars: &mut HashSet<String>) {
        match statement {
            Statement::LetTyped { var, value, .. } => {
                if var.is_logic_var {
                    vars.insert(self.var_to_c_name(var));
                }
                self.collect_logic_vars_in_expr(value, vars);
            }
            Statement::Let { var, value } => {
                if var.is_logic_var {
                    vars.insert(self.var_to_c_name(var));
                }
                self.collect_logic_vars_in_expr(value, vars);
            }
            Statement::Expr(expr) => {
                self.collect_logic_vars_in_expr(expr, vars);
            }
        }
    }

    /// Basic C call expression generation (placeholder)
    fn generate_c_call_expression(&mut self, module: &str, function: &str, args: &[Expr]) -> Result<String, CodegenError> {
        // Generate arguments
        let arg_codes: Result<Vec<_>, _> = args.iter()
            .map(|arg| self.generate_expr(arg, &mut CompilationContext::new()))
            .collect();
        let mut arg_codes = arg_codes?;
        
        // Special handling for printf - cast void* arguments back to appropriate types based on format
        if function == "printf" && !arg_codes.is_empty() {
            // For now, assume non-string arguments to printf should extract integers from Value* objects
            // This is a simplification - a real implementation would parse the format string
            for i in 1..arg_codes.len() {
                if !arg_codes[i].starts_with("\"") { // Not a string literal
                    // Check if this is a logic variable that needs to be forced and extracted
                    if arg_codes[i].contains("flint_force_value") {
                        // Already forced, just extract the integer
                        arg_codes[i] = format!("(int)flint_deref({})->data.integer", arg_codes[i]);
                    } else {
                        // Extract integer value from Value* objects for printf, forcing if needed
                        arg_codes[i] = format!("(int)flint_deref(flint_force_value({}))->data.integer", arg_codes[i]);
                    }
                }
            }
        }
        
        // Simply use the function name directly - C.stdio.printf becomes printf
        Ok(format!("{}({})", function, arg_codes.join(", ")))
    }

    /// Generate Python function call expression
    fn generate_python_call_expression(&mut self, module: &str, function: &str, args: &[Expr]) -> Result<String, CodegenError> {
        // Generate arguments and collect their types for conversion
        let mut arg_conversions = Vec::new();
        let mut arg_names = Vec::new();
        
        for (i, arg) in args.iter().enumerate() {
            let arg_name = format!("py_arg_{}", i);
            let arg_code = self.generate_expr(arg, &mut CompilationContext::new())?;
            
            // Generate conversion from Flint type to PyObject*
            let conversion = match self.infer_expr_type(arg) {
                Some("int") | Some("i32") | Some("i64") => {
                    format!("PyObject* {} = PyLong_FromLong({});", arg_name, arg_code)
                },
                Some("float") | Some("f32") | Some("f64") | Some("double") => {
                    format!("PyObject* {} = PyFloat_FromDouble({});", arg_name, arg_code)
                },
                Some("str") | Some("string") | Some("char*") => {
                    format!("PyObject* {} = PyUnicode_FromString({});", arg_name, arg_code)
                },
                Some("bool") => {
                    format!("PyObject* {} = PyBool_FromLong({} ? 1 : 0);", arg_name, arg_code)
                },
                _ => {
                    // Default: assume it's already a PyObject* or convert via string representation
                    format!("PyObject* {} = PyLong_FromLong({});", arg_name, arg_code)
                }
            };
            
            arg_conversions.push(conversion);
            arg_names.push(arg_name);
        }
        
        // Generate the Python call using C API
        let call_code = format!(r#"({{
    // Convert arguments to Python objects
    {}
    
    // Import the module
    PyObject* module_obj = PyImport_ImportModule("{}");
    if (!module_obj) {{
        PyErr_Print();
        // Cleanup arguments
        {}
        return 0; // or appropriate error value
    }}
    
    // Get the function from the module
    PyObject* func = PyObject_GetAttrString(module_obj, "{}");
    if (!func || !PyCallable_Check(func)) {{
        PyErr_Print();
        Py_DECREF(module_obj);
        {}
        return 0; // or appropriate error value
    }}
    
    // Create tuple of arguments
    PyObject* args_tuple = PyTuple_New({});
    {}
    
    // Call the function
    PyObject* result = PyObject_CallObject(func, args_tuple);
    
    // Cleanup
    Py_DECREF(args_tuple);
    Py_DECREF(func);
    Py_DECREF(module_obj);
    {}
    
    if (!result) {{
        PyErr_Print();
        return 0; // or appropriate error value
    }}
    
    // Convert result back to C type (this will be type-specific)
    // For now, assume integer result
    long c_result = PyLong_AsLong(result);
    Py_DECREF(result);
    c_result;
}})"#,
            arg_conversions.join("\n    "),
            module,
            arg_names.iter().map(|name| format!("Py_DECREF({});", name)).collect::<Vec<_>>().join("\n        "),
            function,
            arg_names.iter().map(|name| format!("Py_DECREF({});", name)).collect::<Vec<_>>().join("\n        "),
            arg_names.len(),
            arg_names.iter().enumerate().map(|(i, name)| 
                format!("PyTuple_SetItem(args_tuple, {}, {});", i, name)
            ).collect::<Vec<_>>().join("\n    "),
            arg_names.iter().map(|name| format!("// {} already transferred to tuple", name)).collect::<Vec<_>>().join("\n    ")
        );
        
        Ok(call_code)
    }

    /// Basic block expression generation (placeholder)
    fn generate_block_expression(&mut self, statements: &[Statement], result: Option<&Expr>, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        let mut code = String::new();
        
        // Collect all logic variables used in this block
        let mut used_logic_vars = HashSet::new();
        for statement in statements {
            self.collect_logic_vars_in_statement(statement, &mut used_logic_vars);
        }
        if let Some(result_expr) = result {
            self.collect_logic_vars_in_expr(result_expr, &mut used_logic_vars);
        }
        
        // Declare any logic variables that haven't been declared yet
        for var_name in &used_logic_vars {
            if !self.declared_variables.contains(var_name) {
                write!(code, "    void* {} = flint_create_logical_var(true);\n", var_name)?;
                self.declared_variables.insert(var_name.clone());
            }
        }
        
        // Generate statements
        for statement in statements {
            let stmt_code = self.generate_statement(statement, ctx)?;
            write!(code, "    {};\n", stmt_code)?;
        }
        
        // Generate result expression if present
        if let Some(result_expr) = result {
            let result_code = self.generate_expr(result_expr, ctx)?;
            write!(code, "    return {};\n", result_code)?;
        }
        
        Ok(format!("({{\n{}\n}})", code.trim_end()))
    }

    /// Smart constraint generation for common patterns
    fn try_generate_smart_constraint(&mut self, expr: &Expr, target: &Expr, ctx: &CompilationContext) -> Result<Option<String>, CodegenError> {
        println!("[DEBUG] Smart constraint called with expr: {:?}, target: {:?}", expr, target);
        
        // Check for function call patterns like function($x) = value (general case)
        if let Expr::Call { func, args } = expr {
            if let Expr::Var(func_var) = func.as_ref() {
                if args.len() == 1 {
                    if let Expr::Var(arg_var) = &args[0] {
                        if arg_var.is_logic_var {
                            println!("[DEBUG] Found function constraint pattern: {}($x) = value", func_var.name);
                            // This is function($x) = target
                            // We need to:
                            // 1. Declare the logic variable
                            // 2. Set up a constraint that function($x) = target
                            // 3. Let the runtime solver figure out the solution using the function registry
                            
                            let target_code = self.generate_expr(target, &mut ctx.clone())?;
                            let var_name = self.var_to_c_name(arg_var);
                            
                            // Generate constraint setup code without hardcoding any solution
                            return Ok(Some(format!(
                                "Value* {} = flint_create_unbound_variable(flint_next_var_id()); flint_solve_function_constraint_runtime(\"{}\", {}, {}, flint_get_global_env());",
                                var_name, func_var.name, var_name, target_code
                            )));
                        }
                    }
                }
            }
        }
        
        // For other function call patterns
        if let Expr::Call { func, args } = expr {
            if let Expr::Var(func_var) = func.as_ref() {
                // Look for any logic variables in the arguments
                for (i, arg) in args.iter().enumerate() {
                    if let Expr::Var(arg_var) = arg {
                        if arg_var.is_logic_var {
                            println!("[DEBUG] Found function call constraint: {}($x) = target", func_var.name);
                            let target_code = self.generate_expr(target, &mut ctx.clone())?;
                            let var_name = self.var_to_c_name(arg_var);
                            
                            // Generate constraint setup for any function with logic variables
                            return Ok(Some(format!(
                                "Value* {} = flint_create_unbound_variable(flint_next_var_id()); flint_solve_function_constraint_runtime(\"{}\", {}, {}, flint_get_global_env());",
                                var_name, func_var.name, var_name, target_code
                            )));
                        }
                    }
                }
            }
        }
        
        // Fallback to runtime constraint solving for other patterns
        println!("[DEBUG] Using fallback constraint solving");
        let expr_code = self.generate_expr(expr, &mut ctx.clone())?;
        let target_code = self.generate_expr(target, &mut ctx.clone())?;
        
        // Generate a constraint solving call
        Ok(Some(format!("flint_solve_constraint({}, {})", expr_code, target_code)))
    }

    /// Get expression type name for debugging
    fn expr_type_name(&self, expr: &Expr) -> &'static str {
        match expr {
            Expr::Var(_) => "Var",
            Expr::NonConsumptiveVar(_) => "NonConsumptiveVar",
            Expr::Int(_) => "Int",
            Expr::Str(_) => "Str",
            Expr::Bool(_) => "Bool",
            Expr::Unit => "Unit",
            Expr::List(_) => "List",
            Expr::ListCons { .. } => "ListCons",
            Expr::Call { .. } => "Call",
            Expr::CCall { .. } => "CCall",
            Expr::PythonCall { .. } => "PythonCall",
            Expr::Let { .. } => "Let",
            Expr::LetTyped { .. } => "LetTyped",
            Expr::LetConstraint { .. } => "LetConstraint",
            Expr::Block { .. } => "Block",
            Expr::Lambda { .. } => "Lambda",
            Expr::Record { .. } => "Record",
            Expr::FieldAccess { .. } => "FieldAccess",
            Expr::Match { .. } => "Match",
            Expr::EffectCall { .. } => "EffectCall",
            Expr::Handle { .. } => "Handle",
            Expr::BinOp { .. } => "BinOp",
            Expr::UnaryOp { .. } => "UnaryOp",
        }
    }

    /// Statement generation 
    fn generate_statement(&mut self, statement: &Statement, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        match statement {
            Statement::LetTyped { var, var_type, value } => {
                let var_name = self.var_to_c_name(var);
                
                if var.is_logic_var {
                    // For logic variables, create a logical variable and unify it with the value
                    let value_code = self.generate_expr(value, ctx)?;
                    
                    if !self.declared_variables.contains(&var_name) {
                        self.declared_variables.insert(var_name.clone());
                        Ok(format!("Value* {} = flint_create_logical_var(true); flint_unify({}, {}, flint_get_global_env());", 
                                   var_name, var_name, value_code))
                    } else {
                        // Variable already declared, just unify
                        Ok(format!("flint_unify({}, {}, flint_get_global_env());", 
                                   var_name, value_code))
                    }
                } else {
                    // For regular variables, use the explicit type
                    let c_type = self.flint_type_to_c_type(var_type);
                    let value_code = self.generate_expr(value, ctx)?;
                    
                    if !self.declared_variables.contains(&var_name) {
                        self.declared_variables.insert(var_name.clone());
                        Ok(format!("{} {} = {};", c_type, var_name, value_code))
                    } else {
                        // Variable already declared, just assign
                        Ok(format!("{} = {};", var_name, value_code))
                    }
                }
            }
            Statement::Let { var, value } => {
                let var_name = self.var_to_c_name(var);
                
                if var.is_logic_var {
                    // For logic variables, create a logical variable and unify it with the value
                    let value_code = self.generate_expr(value, ctx)?;
                    
                    if !self.declared_variables.contains(&var_name) {
                        self.declared_variables.insert(var_name.clone());
                        Ok(format!("Value* {} = flint_create_logical_var(true); flint_unify({}, {}, flint_get_global_env());", 
                                   var_name, var_name, value_code))
                    } else {
                        // Variable already declared, just unify
                        Ok(format!("flint_unify({}, {}, flint_get_global_env());", 
                                   var_name, value_code))
                    }
                } else {
                    // For regular variables, just assign the value
                    let c_type = self.infer_c_type_from_expr(value)?;
                    let value_code = self.generate_expr(value, ctx)?;
                    
                    if !self.declared_variables.contains(&var_name) {
                        self.declared_variables.insert(var_name.clone());
                        Ok(format!("{} {} = {};", c_type, var_name, value_code))
                    } else {
                        // Variable already declared, just assign
                        Ok(format!("{} = {};", var_name, value_code))
                    }
                }
            }
            Statement::Expr(expr) => {
                println!("[DEBUG] Processing statement expression: {:?}", expr);
                let expr_code = self.generate_expr(expr, ctx)?;
                Ok(format!("{}", expr_code))
            }
        }
    }

    /// Infer C type from expression
    fn infer_c_type_from_expr(&self, _expr: &Expr) -> Result<String, CodegenError> {
        // All Flint values use void* as the universal type for now
        Ok("void*".to_string())
    }

    /// Convert Flint type to C type
    fn flint_type_to_c_type(&self, flint_type: &FlintType) -> String {
        match flint_type {
            FlintType::Int32 => "int32_t".to_string(),
            FlintType::String => "char*".to_string(),
            FlintType::Bool => "bool".to_string(),
            FlintType::Unit => "void".to_string(),
            FlintType::List(element_type) => {
                format!("flint_list_t* /* {} */", 
                       self.flint_type_to_c_type(element_type))
            }
            FlintType::Function { .. } => "function_ptr_t".to_string(),
            FlintType::Record { .. } => "void*".to_string(), // Will be cast to specific type
            FlintType::Named(name) => format!("{}_t", name),
            FlintType::TypeVar(name) => format!("/* {} */ void*", name),
        }
    }

    /// Placeholder for remaining functions - basic generation
    pub fn generate(&mut self, program: &Program) -> Result<String, CodegenError> {
        self.generate_with_context(program, "<unknown>", "")
    }
    
    /// Generate C code for a complete program with source context
    pub fn generate_with_context(&mut self, program: &Program, filename: &str, source: &str) -> Result<String, CodegenError> {
        self.output.clear();
        
        let mut ctx = CompilationContext::with_source(filename.to_string(), source.to_string());
        
        // Build IR for analysis
        let mut ir_builder = crate::ir::IRBuilder::new(program);
        let ir_program = ir_builder.build();
        
        // Generate C header includes
        self.generate_header()?;
        
        // Process function definitions using IR
        for ir_function in &ir_program.functions {
            self.generate_function_from_ir(ir_function, program, &mut ctx)?;
        }
        
        // Find main function and generate main
        let main_ir_func = ir_program.functions.iter().find(|f| f.name == "main" || f.name == "test");
        if let Some(main_func) = main_ir_func {
            self.generate_main_function_from_ir(main_func, &ir_program)?;
        } else {
            self.generate_default_main()?;
        }
        
        Ok(self.output.clone())
    }

    /// Generate C headers
    fn generate_header(&mut self) -> Result<(), CodegenError> {
        writeln!(self.output, "// Generated by Flint compiler")?;
        writeln!(self.output, "#include <stdio.h>")?;
        writeln!(self.output, "#include <stdlib.h>")?;
        writeln!(self.output, "#include <string.h>")?;
        writeln!(self.output, "#include <stdbool.h>")?;
        writeln!(self.output, "#include <stdint.h>")?;
        writeln!(self.output, "#include \"../runtime/runtime.h\"")?;
        
        // Include Python.h if we have Python packages
        let has_python_packages = if let Some(package_manager_ptr) = self.package_manager {
            let package_manager = unsafe { &*package_manager_ptr };
            !package_manager.packages.is_empty()
        } else {
            false
        };
        
        if has_python_packages {
            writeln!(self.output, "#include <Python.h>")?;
        }
        
        writeln!(self.output)?;
        Ok(())
    }

    /// Generate function definition
    fn generate_function_definition(&mut self, function: &FunctionDef, ctx: &mut CompilationContext) -> Result<(), CodegenError> {
        // Handle main function specially - rename it to avoid conflict with C main
        let function_name = if function.name == "main" {
            "flint_main".to_string()
        } else {
            function.name.clone()
        };
        
        // Generate function signature
        let return_type = if let Some(ref type_sig) = function.type_signature {
            self.flint_type_to_c_type(type_sig)
        } else if function.name == "main" {
            "int".to_string()  // main should always return int
        } else {
            // Default return type for user-defined functions is Value*
            "Value*".to_string()
        };
        
        // Generate parameters from the first clause patterns
        let mut params = String::new();
        if let Some(clause) = function.clauses.first() {
            for (i, pattern) in clause.patterns.iter().enumerate() {
                if i > 0 {
                    params.push_str(", ");
                }
                match pattern {
                    Pattern::Var(var) if var.is_logic_var => {
                        let var_name = self.var_to_c_name(var);
                        params.push_str(&format!("Value* {}", var_name));
                        // Declare this variable in the function scope
                        self.declared_variables.insert(var_name);
                    }
                    Pattern::Var(var) => {
                        let var_name = self.var_to_c_name(var);
                        params.push_str(&format!("Value* {}", var_name));
                        self.declared_variables.insert(var_name);
                    }
                    _ => {
                        params.push_str("Value* arg");
                    }
                }
            }
        }
        
        writeln!(self.output, "{} {}({}) {{", return_type, function_name, params)?;
        
        // Generate function body (for now, use first clause)
        if let Some(clause) = function.clauses.first() {
            // self.generate_expression_statement(&clause.body, ctx)?;
            writeln!(self.output, "    // Legacy method - function body generation not yet implemented")?;
        }
        
        // If this is main, add return 0
        if function.name == "main" {
            writeln!(self.output, "    return 0;")?;
        } else {
            // For non-main functions, no hardcoded fallback - the function body should handle returns
            // The generated function body is responsible for proper return values
        }
        
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        // Register non-main functions in the function registry for constraint solving
        if function.name != "main" {
            self.defined_functions.insert(function.name.clone());
            writeln!(self.output, "// Register {} in function registry", function.name)?;
            writeln!(self.output, "void register_{}() {{", function.name)?;
            writeln!(self.output, "    flint_register_function(\"{}\", {});", function.name, function_name)?;
            writeln!(self.output, "}}")?;
            writeln!(self.output)?;
        }
        
        Ok(())
    }

    /// Generate function definition from IR
    fn generate_function_from_ir(&mut self, ir_function: &crate::ir::IRFunction, program: &Program, ctx: &mut CompilationContext) -> Result<(), CodegenError> {
        // Clear declared variables for this function
        self.declared_variables.clear();
        self.variable_occurrences.clear();
        self.current_variable_occurrence.clear();
        
        // Handle main function specially - rename it to avoid conflict with C main
        let function_name = if ir_function.name == "main" {
            "flint_main".to_string()
        } else {
            ir_function.name.clone()
        };
        
        // Generate function signature based on IR information
        let return_type = match &ir_function.return_type {
            crate::ir::IRType::Integer => "Value*".to_string(), // Always use Value* for runtime
            crate::ir::IRType::Unit => if ir_function.name == "main" { "int".to_string() } else { "Value*".to_string() },
            _ => if ir_function.name == "main" { "int".to_string() } else { "Value*".to_string() }
        };
        
        // Generate parameters from IR
        let mut params = String::new();
        for (i, param) in ir_function.parameters.iter().enumerate() {
            if i > 0 {
                params.push_str(", ");
            }
            params.push_str(&format!("Value* {}", param.name));
            self.declared_variables.insert(param.name.clone());
        }
        
        writeln!(self.output, "{} {}({}) {{", return_type, function_name, params)?;
        
        // Collect all variables from constraints and declare them at the top
        self.collect_and_declare_constraint_variables(ir_function)?;
        
        // Generate function body based on IR
        let is_main_function = ir_function.name == "main";
        self.generate_ir_expression_as_statement(&ir_function.body, ir_function, is_main_function)?;
        
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        // Generate function registration for non-main functions
        if ir_function.name != "main" {
            writeln!(self.output, "// Register {} in function registry", ir_function.name)?;
            writeln!(self.output, "void register_{}() {{", ir_function.name)?;
            
            // Use correct function signature for registration
            match ir_function.parameters.len() {
                0 => {
                    writeln!(self.output, "    flint_register_function_0(\"{}\", {});", ir_function.name, function_name)?;
                }
                1 => {
                    writeln!(self.output, "    flint_register_function(\"{}\", {});", ir_function.name, function_name)?;
                }
                2 => {
                    writeln!(self.output, "    flint_register_function_2(\"{}\", {});", ir_function.name, function_name)?;
                }
                _ => {
                    writeln!(self.output, "    flint_register_function_n(\"{}\", {}, {});", ir_function.name, function_name, ir_function.parameters.len())?;
                }
            }
            
            writeln!(self.output, "}}")?;
            writeln!(self.output)?;
        }
        
        Ok(())
    }

    /// Generate main function from IR  
    fn generate_main_function_from_ir(&mut self, ir_function: &crate::ir::IRFunction, ir_program: &crate::ir::IRProgram) -> Result<(), CodegenError> {
        writeln!(self.output, "int main(void) {{")?;
        writeln!(self.output, "    // Initialize Flint runtime")?;
        writeln!(self.output, "    flint_init_runtime();")?;
        writeln!(self.output)?;
        
        // Initialize Python if we have Python packages
        let has_python_packages = if let Some(package_manager_ptr) = self.package_manager {
            let package_manager = unsafe { &*package_manager_ptr };
            !package_manager.packages.is_empty()
        } else {
            false
        };
        
        if has_python_packages {
            writeln!(self.output, "    // Initialize Python interpreter")?;
            writeln!(self.output, "    Py_Initialize();")?;
            writeln!(self.output, "    if (!Py_IsInitialized()) {{")?;
            writeln!(self.output, "        fprintf(stderr, \"Failed to initialize Python\\n\");")?;
            writeln!(self.output, "        return 1;")?;
            writeln!(self.output, "    }}")?;
            writeln!(self.output)?;
            
            // Add the package directories to Python path and activate venv
            if let Some(package_manager_ptr) = self.package_manager {
                let package_manager = unsafe { &*package_manager_ptr };
                writeln!(self.output, "    // Add venv site-packages and package directories to Python path")?;
                
                // Add the venv site-packages directory first
                if let Some((_, package)) = package_manager.packages.iter().next() {
                    let venv_site_packages = package.install_path.parent()
                        .and_then(|p| p.parent())
                        .map(|p| p.join("venv/lib/python3.13/site-packages"))
                        .unwrap_or_else(|| std::path::Path::new(".flint/lib/python/venv/lib/python3.13/site-packages").to_path_buf());
                    writeln!(self.output, "    PyRun_SimpleString(\"import sys; sys.path.insert(0, '{}')\");", venv_site_packages.display())?;
                }
                
                // Add each package directory
                for (_, package) in &package_manager.packages {
                    let package_dir = package.install_path.display();
                    writeln!(self.output, "    PyRun_SimpleString(\"import sys; sys.path.insert(0, '{}')\");", package_dir)?;
                }
                writeln!(self.output)?;
            }
        }
        
        // Register all user-defined functions (except main)
        writeln!(self.output, "    // Register user-defined functions")?;
        for func in &ir_program.functions {
            if func.name != "main" {
                writeln!(self.output, "    register_{}();", func.name)?;
            }
        }
        writeln!(self.output)?;
        
        // Call flint_main and handle its return value
        writeln!(self.output, "    // Call main function")?;
        writeln!(self.output, "    int result = flint_main();")?;
        writeln!(self.output)?;
        
        if has_python_packages {
            writeln!(self.output, "    // Finalize Python interpreter")?;
            writeln!(self.output, "    Py_Finalize();")?;
        }
        
        writeln!(self.output, "    // Cleanup runtime")?;
        writeln!(self.output, "    flint_cleanup_runtime();")?;
        writeln!(self.output, "    return result;")?;
        writeln!(self.output, "}}")?;
        
        Ok(())
    }

    /// Generate default main function
    fn generate_default_main(&mut self) -> Result<(), CodegenError> {
        writeln!(self.output, "int main(void) {{")?;
        writeln!(self.output, "    // Initialize Flint runtime")?;
        writeln!(self.output, "    flint_init_runtime();")?;
        writeln!(self.output)?;
        
        // Initialize Python if we have Python packages
        let has_python_packages = if let Some(package_manager_ptr) = self.package_manager {
            let package_manager = unsafe { &*package_manager_ptr };
            !package_manager.packages.is_empty()
        } else {
            false
        };
        
        if has_python_packages {
            writeln!(self.output, "    // Initialize Python interpreter")?;
            writeln!(self.output, "    Py_Initialize();")?;
            writeln!(self.output, "    if (!Py_IsInitialized()) {{")?;
            writeln!(self.output, "        fprintf(stderr, \"Failed to initialize Python\\n\");")?;
            writeln!(self.output, "        return 1;")?;
            writeln!(self.output, "    }}")?;
            writeln!(self.output)?;
        }
        
        writeln!(self.output, "    printf(\"No main function found.\\n\");")?;
        writeln!(self.output)?;
        
        if has_python_packages {
            writeln!(self.output, "    // Finalize Python interpreter")?;
            writeln!(self.output, "    Py_Finalize();")?;
        }
        
        writeln!(self.output, "    // Cleanup runtime")?;
        writeln!(self.output, "    flint_cleanup_runtime();")?;
        writeln!(self.output, "    return 1;")?;
        writeln!(self.output, "}}")?;
        Ok(())
    }

    /// Convert IR type to C type
    fn ir_type_to_c_type(&self, ir_type: &crate::ir::IRType) -> String {
        match ir_type {
            crate::ir::IRType::Integer => "int".to_string(),
            crate::ir::IRType::String => "char*".to_string(),
            crate::ir::IRType::Boolean => "bool".to_string(),
            crate::ir::IRType::Unit => "void".to_string(),
            crate::ir::IRType::List(_) => "Value*".to_string(),
            crate::ir::IRType::Function { .. } => "Value*".to_string(),
            crate::ir::IRType::Variable(_) => "Value*".to_string(),
            crate::ir::IRType::Unknown => "Value*".to_string(),
        }
    }

    /// Generate IR expression as a C statement
    fn generate_ir_expression_as_statement(&mut self, expr: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction, is_main_function: bool) -> Result<(), CodegenError> {
        // First, generate code for all constraints in the function
        self.generate_ir_constraints(&ir_function.constraints, ir_function, is_main_function)?;
        
        // Then generate the main body expression
        match expr {
            crate::ir::IRExpression::Symbol(symbol) => {
                if ir_function.name == "main" {
                    // For main function, just return success after executing constraints
                    writeln!(self.output, "    return 0;")?;
                } else {
                    // For arithmetic functions, generate implementation using constraints
                    self.generate_arithmetic_function_body(symbol, ir_function)?;
                }
            }
            crate::ir::IRExpression::CCall { module, function, arguments } => {
                // Generate C function call
                self.generate_c_call(module, function, arguments, ir_function)?;
            },
            crate::ir::IRExpression::PythonCall { module, function, arguments } => {
                // Generate Python function call (via Python C API)
                self.generate_python_call(module, function, arguments, ir_function)?;
            },
            _ => {
                writeln!(self.output, "    // Unsupported IR expression: {:?}", expr)?;
                if ir_function.name == "main" {
                    writeln!(self.output, "    return 0;")?;
                } else {
                    writeln!(self.output, "    return NULL;")?;
                }
            }
        }
        Ok(())
    }

    /// Collect all variables from constraints and declare them at the top of the function
    fn collect_and_declare_constraint_variables(&mut self, ir_function: &crate::ir::IRFunction) -> Result<(), CodegenError> {
        let mut variables_to_declare = std::collections::HashSet::new();
        
        // First pass: collect all variables and count their occurrences
        self.analyze_variable_occurrences(ir_function)?;
        
        // Collect variables from all constraints
        for constraint in &ir_function.constraints {
            match constraint {
                crate::ir::IRConstraint::Unification { left, right } => {
                    self.collect_variables_from_expression(left, &mut variables_to_declare);
                    self.collect_variables_from_expression(right, &mut variables_to_declare);
                },
                crate::ir::IRConstraint::Arithmetic { operation: _, operands, result } => {
                    for operand in operands {
                        self.collect_variables_from_expression(operand, &mut variables_to_declare);
                    }
                    self.collect_variables_from_expression(result, &mut variables_to_declare);
                },
                crate::ir::IRConstraint::FunctionCall { function: _, arguments, result } => {
                    for arg in arguments {
                        self.collect_variables_from_expression(arg, &mut variables_to_declare);
                    }
                    self.collect_variables_from_expression(result, &mut variables_to_declare);
                },
                _ => {}
            }
        }
        
        // Declare and initialize all collected variables that aren't already declared (parameters)
        for var_name in variables_to_declare {
            if !self.declared_variables.contains(&var_name) {
                writeln!(self.output, "    Value* {} = flint_create_logical_var(true);", var_name)?;
                self.declared_variables.insert(var_name);
            }
        }
        
        Ok(())
    }

    /// Analyze variable occurrences across all constraints to enable linearity tracking
    fn analyze_variable_occurrences(&mut self, ir_function: &crate::ir::IRFunction) -> Result<(), CodegenError> {
        self.variable_occurrences.clear();
        
        // Count occurrences of each variable
        for constraint in &ir_function.constraints {
            match constraint {
                crate::ir::IRConstraint::Unification { left, right } => {
                    self.count_variable_occurrences_in_expression(left);
                    self.count_variable_occurrences_in_expression(right);
                },
                crate::ir::IRConstraint::Arithmetic { operation: _, operands, result } => {
                    for operand in operands {
                        self.count_variable_occurrences_in_expression(operand);
                    }
                    self.count_variable_occurrences_in_expression(result);
                },
                crate::ir::IRConstraint::FunctionCall { function: _, arguments, result } => {
                    for arg in arguments {
                        self.count_variable_occurrences_in_expression(arg);
                    }
                    self.count_variable_occurrences_in_expression(result);
                },
                _ => {}
            }
        }
        
        Ok(())
    }

    /// Helper to count variable occurrences in an IR expression
    fn count_variable_occurrences_in_expression(&mut self, expr: &crate::ir::IRExpression) {
        match expr {
            crate::ir::IRExpression::Symbol(symbol) => {
                // Skip ~ prefixed variables (non-consumptive)
                let var_name = if symbol.name.starts_with("~") {
                    symbol.name.trim_start_matches('~').to_string()
                } else {
                    symbol.name.clone()
                };
                *self.variable_occurrences.entry(var_name).or_insert(0) += 1;
            },
            crate::ir::IRExpression::InlineArithmetic { operation: _, left, right } => {
                self.count_variable_occurrences_in_expression(left);
                self.count_variable_occurrences_in_expression(right);
            },
            crate::ir::IRExpression::FunctionCall { function, arguments } => {
                self.count_variable_occurrences_in_expression(function);
                for arg in arguments {
                    self.count_variable_occurrences_in_expression(arg);
                }
            },
            crate::ir::IRExpression::PythonCall { module: _, function: _, arguments } => {
                for arg in arguments {
                    self.count_variable_occurrences_in_expression(arg);
                }
            },
            // Literals don't contain variables
            _ => {}
        }
    }
    
    /// Helper to collect variable names from an IR expression
    fn collect_variables_from_expression(&self, expr: &crate::ir::IRExpression, variables: &mut std::collections::HashSet<String>) {
        match expr {
            crate::ir::IRExpression::Symbol(symbol) => {
                variables.insert(symbol.name.clone());
            },
            crate::ir::IRExpression::InlineArithmetic { operation: _, left, right } => {
                self.collect_variables_from_expression(left, variables);
                self.collect_variables_from_expression(right, variables);
            },
            crate::ir::IRExpression::FunctionCall { function, arguments } => {
                self.collect_variables_from_expression(function, variables);
                for arg in arguments {
                    self.collect_variables_from_expression(arg, variables);
                }
            },
            // Literals don't contain variables
            _ => {}
        }
    }

    /// Generate arithmetic function body using constraints and unification
    fn generate_arithmetic_function_body(&mut self, result_symbol: &crate::ir::Symbol, ir_function: &crate::ir::IRFunction) -> Result<(), CodegenError> {
        // Find the constraint that defines this result symbol
        for constraint in &ir_function.constraints {
            if let crate::ir::IRConstraint::Arithmetic { operation, operands, result } = constraint {
                if let crate::ir::IRExpression::Symbol(sym) = result {
                    if sym.name == result_symbol.name {
                        // Generate constraint-based unification function
                        return self.generate_constraint_unification_function(operation, operands, ir_function);
                    }
                }
            }
        }
        
        // No constraint found - generate a simple return
        writeln!(self.output, "    return flint_create_integer(0); // No constraint found")?;
        Ok(())
    }
    
    /// Generate a function that uses constraint solving and unification
    fn generate_constraint_unification_function(
        &mut self, 
        operation: &crate::ir::ArithmeticOp, 
        operands: &[crate::ir::IRExpression],
        ir_function: &crate::ir::IRFunction
    ) -> Result<(), CodegenError> {
        if operands.len() != 2 || !matches!(operation, 
            crate::ir::ArithmeticOp::Add | 
            crate::ir::ArithmeticOp::Subtract | 
            crate::ir::ArithmeticOp::Multiply | 
            crate::ir::ArithmeticOp::Divide | 
            crate::ir::ArithmeticOp::Modulo) {
            writeln!(self.output, "    return NULL; // Unsupported operation")?;
            return Ok(());
        }
        
        // Generate constraint solver for binary arithmetic operation
        let op_name = match operation {
            crate::ir::ArithmeticOp::Add => "add",
            crate::ir::ArithmeticOp::Subtract => "subtract", 
            crate::ir::ArithmeticOp::Multiply => "multiply",
            crate::ir::ArithmeticOp::Divide => "divide",
            crate::ir::ArithmeticOp::Modulo => "modulo",
            crate::ir::ArithmeticOp::Negate => {
                writeln!(self.output, "    return NULL; // Negate is unary, not binary")?;
                return Ok(());
            }
        };
        
        // Generate proper constraint-based unification
        writeln!(self.output, "    // Create result as logic variable")?;
        writeln!(self.output, "    Value* result = flint_create_logical_var(false);")?;
        writeln!(self.output)?;
        writeln!(self.output, "    // Try to solve the constraint using the runtime")?;
        
        let left_code = self.generate_ir_expression_value(&operands[0], ir_function)?;
        let right_code = self.generate_ir_expression_value(&operands[1], ir_function)?;
        
        writeln!(self.output, "    flint_solve_arithmetic_constraint({}, {}, result, \"{}\");", left_code, right_code, op_name)?;
        writeln!(self.output, "    // Always return the result variable, even if constraint couldn't be solved immediately")?;
        writeln!(self.output, "    return result;")?;
        
        Ok(())
    }

    /// Generate unary arithmetic operation
    fn generate_unary_arithmetic(&mut self, operands: &[crate::ir::IRExpression], _result: &crate::ir::IRExpression) -> Result<(), CodegenError> {
        if let Some(operand) = operands.first() {
            let operand_expr = self.ir_expression_to_c_value(operand)?;
            writeln!(self.output, "    int operand_val = {};", operand_expr)?;
            writeln!(self.output, "    int result_val = -operand_val;")?;
            writeln!(self.output, "    return flint_create_integer(result_val);")?;
        } else {
            writeln!(self.output, "    return flint_create_integer(0);")?;
        }
        Ok(())
    }

    /// Generate C function call
    fn generate_c_call(&mut self, _module: &str, function: &str, arguments: &[crate::ir::IRExpression], ir_function: &crate::ir::IRFunction) -> Result<(), CodegenError> {
        // Generate the C call
        write!(self.output, "    {}(", function)?;
        for (i, arg) in arguments.iter().enumerate() {
            if i > 0 {
                write!(self.output, ", ")?;
            }
            match arg {
                crate::ir::IRExpression::Literal(crate::ir::Literal::String(s)) => {
                    write!(self.output, "\"{}\"", s)?;
                }
                crate::ir::IRExpression::Symbol(symbol) => {
                    // For C calls, convert Value* to appropriate C type
                    write!(self.output, "flint_value_to_int({})", symbol.name)?;
                }
                _ => {
                    write!(self.output, "{}", self.ir_expression_to_c_value(arg)?)?;
                }
            }
        }
        writeln!(self.output, ");")?;
        
        if ir_function.name == "main" {
            writeln!(self.output, "    return 0;")?;
        }
        
        Ok(())
    }

    /// Convert IR expression to C value expression
    fn ir_expression_to_c_value(&self, expr: &crate::ir::IRExpression) -> Result<String, CodegenError> {
        match expr {
            crate::ir::IRExpression::Symbol(symbol) => {
                Ok(format!("flint_value_to_int({})", symbol.name))
            }
            crate::ir::IRExpression::Literal(lit) => {
                match lit {
                    crate::ir::Literal::Integer(n) => Ok(format!("flint_create_integer({})", n)),
                    crate::ir::Literal::String(s) => Ok(format!("flint_create_string(\"{}\")", s)),
                    crate::ir::Literal::Boolean(b) => Ok(format!("flint_create_boolean({})", if *b { "true" } else { "false" })),
                    crate::ir::Literal::Unit => Ok("flint_create_unit()".to_string()),
                }
            }
            _ => Err(CodegenError::unsupported_feature("Complex IR expression in value context"))
        }
    }
    
    /// Generate C code for IR constraints
    fn generate_ir_constraints(&mut self, constraints: &[crate::ir::IRConstraint], ir_function: &crate::ir::IRFunction, is_main_function: bool) -> Result<(), CodegenError> {
        for constraint in constraints {
            match constraint {
                crate::ir::IRConstraint::Unification { left, right } => {
                    // Generate unification constraint
                    self.generate_unification_constraint(left, right, ir_function, is_main_function)?;
                },
                crate::ir::IRConstraint::Arithmetic { operation, operands, result } => {
                    // Generate arithmetic constraint
                    self.generate_arithmetic_constraint(operation, operands, result, ir_function, is_main_function)?;
                },
                crate::ir::IRConstraint::FunctionCall { function, arguments, result } => {
                    // Generate function call constraint
                    self.generate_function_call_constraint(function, arguments, result, ir_function, is_main_function)?;
                },
                _ => {
                    // Skip other constraint types for now
                    writeln!(self.output, "    // Unsupported constraint: {:?}", constraint)?;
                }
            }
        }
        Ok(())
    }
    
    /// Generate C code for unification constraint
    fn generate_unification_constraint(&mut self, left: &crate::ir::IRExpression, right: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction, is_main_function: bool) -> Result<(), CodegenError> {
        // Clear consumed variables tracker for this constraint
        self.consumed_variables_in_constraint.clear();
        
        match (left, right) {
            (crate::ir::IRExpression::Symbol(var_symbol), value_expr) => {
                // This is a variable assignment: var = expression
                let var_name = &var_symbol.name;
                
                // Generate the right-hand side value
                let value_code = self.generate_ir_expression_value(value_expr, ir_function)?;
                
                // Generate unification
                writeln!(self.output, "    flint_unify({}, {}, flint_get_global_env());", var_name, value_code)?;
                
                // If this is a call_result temporary variable, free it immediately after unification
                if var_name.starts_with("call_result_") {
                    writeln!(self.output, "    // Free temporary call_result variable after use")?;
                    writeln!(self.output, "    flint_free_value({});", var_name)?;
                }
            },
            _ => {
                // General unification between two expressions
                let left_code = self.generate_ir_expression_value(left, ir_function)?;
                let right_code = self.generate_ir_expression_value(right, ir_function)?;
                writeln!(self.output, "    flint_unify({}, {}, flint_get_global_env());", left_code, right_code)?;
            }
        }
        
        // Generate freeing calls for any variables consumed in this constraint
        self.generate_consumed_variable_freeing(is_main_function)?;
        
        Ok(())
    }
    
    /// Generate C code for arithmetic constraint
    fn generate_arithmetic_constraint(&mut self, operation: &crate::ir::ArithmeticOp, operands: &[crate::ir::IRExpression], result: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction, is_main_function: bool) -> Result<(), CodegenError> {
        // Clear consumed variables tracker for this constraint
        self.consumed_variables_in_constraint.clear();
        
        if operands.len() == 2 {
            let left_code = self.generate_ir_expression_value(&operands[0], ir_function)?;
            let right_code = self.generate_ir_expression_value(&operands[1], ir_function)?;
            let result_code = self.generate_ir_expression_value(result, ir_function)?;
            
            let op_str = operation.to_c_op();
            writeln!(self.output, "    // Arithmetic constraint: {} = {} {} {}", 
                     result_code, left_code, op_str, right_code)?;
            
            // Don't generate direct assignment - let the constraint solver handle it
            // The constraint variables are already declared at the top of the function
        }
        
        // Generate freeing calls for any variables consumed in this constraint
        self.generate_consumed_variable_freeing(is_main_function)?;
        
        Ok(())
    }
    
    /// Generate C code for function call constraint
    fn generate_function_call_constraint(&mut self, function: &str, arguments: &[crate::ir::IRExpression], result: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction, is_main_function: bool) -> Result<(), CodegenError> {
        eprintln!("DEBUG: generate_function_call_constraint called with function = '{}'", function);
        
        // Clear consumed variables tracker for this constraint
        self.consumed_variables_in_constraint.clear();
        
        // Check if this is a Python function call (format: module_function)
        if let Some(underscore_pos) = function.find('_') {
            let module = &function[..underscore_pos];
            let func_name = &function[underscore_pos + 1..];
            
            eprintln!("DEBUG: Found underscore, module = '{}', func_name = '{}'", module, func_name);
            
            // Check if this module is a Python package
            if let Some(package_manager_ptr) = self.package_manager {
                let package_manager = unsafe { &*package_manager_ptr };
                if package_manager.packages.contains_key(module) {
                    eprintln!("DEBUG: Module '{}' is a Python package, using Python C API", module);
                    // This is a Python function call - use our Python C API integration
                    self.generate_python_function_call_constraint(module, func_name, arguments, result, ir_function, is_main_function)?;
                    return Ok(());
                }
            }
        }
        
        eprintln!("DEBUG: Using regular function call for '{}'", function);
        
        // Generate regular function call constraint
        let mut arg_codes = Vec::new();
        for arg in arguments {
            arg_codes.push(self.generate_ir_expression_value(arg, ir_function)?);
        }
        
        let result_code = self.generate_ir_expression_value(result, ir_function)?;
        
        writeln!(self.output, "    // Function call constraint: {} = {}({})", result_code, function, arg_codes.join(", "))?;
        writeln!(self.output, "    flint_unify({}, {}({}), flint_get_global_env());", result_code, function, arg_codes.join(", "))?;
        
        // Generate freeing calls for any variables consumed in this constraint
        self.generate_consumed_variable_freeing(is_main_function)?;
        
        Ok(())
    }
    
    /// Generate IR expression as a C value (helper for constraints)
    fn generate_ir_expression_value(&mut self, expr: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction) -> Result<String, CodegenError> {
        match expr {
            crate::ir::IRExpression::Symbol(symbol) => {
                // Return the symbol name directly, as it represents a Value*
                Ok(symbol.name.clone())
            },
            crate::ir::IRExpression::Literal(lit) => {
                match lit {
                    crate::ir::Literal::Integer(n) => Ok(format!("flint_create_integer({})", n)),
                    crate::ir::Literal::String(s) => Ok(format!("flint_create_string(\"{}\")", s)),
                    crate::ir::Literal::Boolean(b) => Ok(format!("flint_create_boolean({})", if *b { "true" } else { "false" })),
                    crate::ir::Literal::Unit => Ok("flint_create_unit()".to_string()),
                }
            },
            crate::ir::IRExpression::InlineArithmetic { operation, left, right } => {
                let left_code = self.generate_ir_expression_value(left, ir_function)?;
                let right_code = self.generate_ir_expression_value(right, ir_function)?;
                Ok(format!("flint_create_integer(flint_value_to_int({}) {} flint_value_to_int({}))", left_code, operation.to_c_op(), right_code))
            },
            crate::ir::IRExpression::FunctionCall { function, arguments } => {
                let func_code = self.generate_ir_expression_value(function, ir_function)?;
                let mut arg_codes = Vec::new();
                for arg in arguments {
                    arg_codes.push(self.generate_ir_expression_value(arg, ir_function)?);
                }
                Ok(format!("{}({})", func_code, arg_codes.join(", ")))
            },
            crate::ir::IRExpression::PythonCall { module, function, arguments } => {
                // Generate Python function call using C API
                let mut arg_conversions = Vec::new();
                let mut arg_names = Vec::new();
                
                for (i, arg) in arguments.iter().enumerate() {
                    let arg_name = format!("py_arg_{}", i);
                    let arg_code = self.generate_ir_expression_c_value(arg, ir_function)?;
                    
                    let conversion = format!("PyObject* {} = PyLong_FromLong({});", arg_name, arg_code);
                    arg_conversions.push(conversion);
                    arg_names.push(arg_name);
                }
                
                // Generate the Python call using C API as a compound statement
                let call_code = format!("({{
    // Convert arguments to Python objects
    {}
    
    // Import the module
    PyObject* module_obj = PyImport_ImportModule(\"{}\");
    if (!module_obj) {{
        PyErr_Print();
        // Cleanup arguments
        {}
        return 0; // Error in inline expression
    }}
    
    // Get the function from the module  
    PyObject* func = PyObject_GetAttrString(module_obj, \"{}\");
    if (!func || !PyCallable_Check(func)) {{
        PyErr_Print();
        Py_DECREF(module_obj);
        {}
        return 0; // Error in inline expression
    }}
    
    // Create tuple of arguments
    PyObject* args_tuple = PyTuple_New({});
    {}
    
    // Call the function
    PyObject* py_result = PyObject_CallObject(func, args_tuple);
    
    // Cleanup
    Py_DECREF(args_tuple);
    Py_DECREF(func);
    Py_DECREF(module_obj);
    {}
    
    if (!py_result) {{
        PyErr_Print();
        return 0; // Error in inline expression
    }}
    
    // Convert result back to C and create Flint integer
    long c_result = PyLong_AsLong(py_result);
    Py_DECREF(py_result);
    flint_create_integer(c_result);
}})",
                    arg_conversions.join("\n    "),
                    module,
                    arg_names.iter().map(|name| format!("Py_DECREF({});", name)).collect::<Vec<_>>().join("\n        "),
                    function,
                    arg_names.iter().map(|name| format!("Py_DECREF({});", name)).collect::<Vec<_>>().join("\n        "),
                    arg_names.len(),
                    arg_names.iter().enumerate().map(|(i, name)| 
                        format!("PyTuple_SetItem(args_tuple, {}, {}); // steals reference", i, name)
                    ).collect::<Vec<_>>().join("\n    "),
                    arg_names.iter().map(|name| format!("// {} reference transferred to tuple", name)).collect::<Vec<_>>().join("\n    ")
                );
                
                Ok(call_code)
            },
            _ => Err(CodegenError::unsupported_feature("Complex IR expression in value generation"))
        }
    }

    /// Infer the type of an expression for Python conversion
    fn infer_expr_type(&self, expr: &Expr) -> Option<&'static str> {
        match expr {
            Expr::Int(_) => Some("int"),
            Expr::Str(_) => Some("string"),
            Expr::Bool(_) => Some("bool"),
            Expr::Var(_) => {
                // For variables, we'd need type information from the type checker
                // For now, assume int as default
                Some("int")
            },
            Expr::BinOp { .. } => {
                // Binary operations typically result in numeric types
                Some("int")
            },
            Expr::CCall { .. } => {
                // C calls return whatever the C function returns
                Some("int")
            },
            _ => None,
        }
    }

    fn generate_python_call(&mut self, module: &str, function: &str, arguments: &[crate::ir::IRExpression], ir_function: &crate::ir::IRFunction) -> Result<(), CodegenError> {
        // Generate Python function call via Python C API
        // The naming convention is: {module}_{function}
        let wrapper_function = format!("{}_{}", module, function);
        
        if ir_function.name == "main" {
            // For main function, we need to generate the call that assigns to variables
            write!(self.output, "    int result = {}(", wrapper_function)?;
            
            for (i, arg) in arguments.iter().enumerate() {
                if i > 0 {
                    write!(self.output, ", ")?;
                }
                let arg_code = self.generate_ir_expression_value(arg, ir_function)?;
                write!(self.output, "flint_value_to_int({})", arg_code)?;
            }
            
            writeln!(self.output, ");")?;
            writeln!(self.output, "    printf(\"Result: %d\\n\", result);")?;
        } else {
            // For other functions, generate assignment or expression
            write!(self.output, "{}(", wrapper_function)?;
            
            for (i, arg) in arguments.iter().enumerate() {
                if i > 0 {
                    write!(self.output, ", ")?;
                }
                let arg_code = self.generate_ir_expression_value(arg, ir_function)?;
                write!(self.output, "flint_value_to_int({})", arg_code)?;
            }
            
            write!(self.output, ")")?;
        }
        
        Ok(())
    }
    
    /// Generate Python function call constraint using C API
    fn generate_python_function_call_constraint(&mut self, module: &str, function: &str, arguments: &[crate::ir::IRExpression], result: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction, is_main_function: bool) -> Result<(), CodegenError> {
        // Convert IR arguments to C arguments for Python C API
        let mut arg_conversions = Vec::new();
        let mut arg_names = Vec::new();
        
        for (i, arg) in arguments.iter().enumerate() {
            let arg_name = format!("py_arg_{}", i);
            let arg_code = self.generate_ir_expression_c_value(arg, ir_function)?;
            
            // For now, assume integers - we can extend this later
            let conversion = format!("PyObject* {} = PyLong_FromLong({});", arg_name, arg_code);
            arg_conversions.push(conversion);
            arg_names.push(arg_name);
        }
        
        let result_code = self.generate_ir_expression_value(result, ir_function)?;
        
        // Generate the Python call using C API with proper error handling
        writeln!(self.output, "    // Python function call: {} = {}.{}({})", result_code, module, function, arg_names.join(", "))?;
        writeln!(self.output, "    {{")?;
        
        // Convert arguments to Python objects
        for conversion in &arg_conversions {
            writeln!(self.output, "        {};", conversion)?;
        }
        
        writeln!(self.output, "        ")?;
        writeln!(self.output, "        // Import the module")?;
        writeln!(self.output, "        PyObject* module_obj = PyImport_ImportModule(\"{}\");", module)?;
        writeln!(self.output, "        if (!module_obj) {{")?;
        writeln!(self.output, "            PyErr_Print();")?;
        
        // Cleanup arguments on error
        for arg_name in &arg_names {
            writeln!(self.output, "            Py_DECREF({});", arg_name)?;
        }
        writeln!(self.output, "            return -1; // Error")?;
        writeln!(self.output, "        }}")?;
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        // Get the function from the module")?;
        writeln!(self.output, "        PyObject* func = PyObject_GetAttrString(module_obj, \"{}\");", function)?;
        writeln!(self.output, "        if (!func || !PyCallable_Check(func)) {{")?;
        writeln!(self.output, "            PyErr_Print();")?;
        writeln!(self.output, "            Py_DECREF(module_obj);")?;
        for arg_name in &arg_names {
            writeln!(self.output, "            Py_DECREF({});", arg_name)?;
        }
        writeln!(self.output, "            return -1; // Error")?;
        writeln!(self.output, "        }}")?;
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        // Create tuple of arguments")?;
        writeln!(self.output, "        PyObject* args_tuple = PyTuple_New({});", arg_names.len())?;
        for (i, arg_name) in arg_names.iter().enumerate() {
            writeln!(self.output, "        PyTuple_SetItem(args_tuple, {}, {}); // steals reference", i, arg_name)?;
        }
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        // Call the function")?;
        writeln!(self.output, "        PyObject* py_result = PyObject_CallObject(func, args_tuple);")?;
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        // Cleanup")?;
        writeln!(self.output, "        Py_DECREF(args_tuple);")?;
        writeln!(self.output, "        Py_DECREF(func);")?;
        writeln!(self.output, "        Py_DECREF(module_obj);")?;
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        if (!py_result) {{")?;
        writeln!(self.output, "            PyErr_Print();")?;
        writeln!(self.output, "            return -1; // Error")?;
        writeln!(self.output, "        }}")?;
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        // Convert result back to C type and create Flint value")?;
        writeln!(self.output, "        long c_result = PyLong_AsLong(py_result);")?;
        writeln!(self.output, "        Py_DECREF(py_result);")?;
        writeln!(self.output, "        ")?;
        
        writeln!(self.output, "        // Unify result directly without extra conversion")?;
        writeln!(self.output, "        flint_unify({}, flint_create_integer(c_result), flint_get_global_env());", result_code)?;
        writeln!(self.output, "    }}")?;
        
        // Generate freeing calls for any variables consumed in this constraint
        self.generate_consumed_variable_freeing(is_main_function)?;
        
        Ok(())
    }
    
    /// Generate IR expression as a raw C value (not wrapped in Flint Value)
    fn generate_ir_expression_c_value(&mut self, expr: &crate::ir::IRExpression, ir_function: &crate::ir::IRFunction) -> Result<String, CodegenError> {
        match expr {
            crate::ir::IRExpression::Symbol(symbol) => {
                // For variables, extract the value from the Flint Value
                Ok(format!("flint_value_to_int({})", symbol.name))
            },
            crate::ir::IRExpression::Literal(lit) => {
                match lit {
                    crate::ir::Literal::Integer(n) => Ok(n.to_string()),
                    crate::ir::Literal::String(s) => Ok(format!("\"{}\"", s)),
                    crate::ir::Literal::Boolean(b) => Ok(if *b { "1".to_string() } else { "0".to_string() }),
                    crate::ir::Literal::Unit => Ok("0".to_string()),
                }
            },
            crate::ir::IRExpression::InlineArithmetic { operation, left, right } => {
                let left_code = self.generate_ir_expression_c_value(left, ir_function)?;
                let right_code = self.generate_ir_expression_c_value(right, ir_function)?;
                Ok(format!("({} {} {})", left_code, operation.to_c_op(), right_code))
            },
            _ => {
                // For other expressions, fall back to generating the full Flint value and extracting
                let full_expr = self.generate_ir_expression_value(expr, ir_function)?;
                Ok(format!("flint_value_to_int({})", full_expr))
            }
        }
    }
}

impl std::fmt::Display for BinOp {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let op_str = match self {
            BinOp::Add => "+",
            BinOp::Sub => "-",
            BinOp::Mul => "*",
            BinOp::Div => "/",
            BinOp::Mod => "%",
            BinOp::Eq => "==",
            BinOp::Ne => "!=",
            BinOp::Lt => "<",
            BinOp::Le => "<=",
            BinOp::Gt => ">",
            BinOp::Ge => ">=",
            BinOp::And => "&&",
            BinOp::Or => "||",
            BinOp::Append => "+", // Use + for string/list concatenation in C
        };
        write!(f, "{}", op_str)
    }
}
