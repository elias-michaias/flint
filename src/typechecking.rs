use crate::ast::*;
use crate::diagnostic::{Diagnostic, SourceLocation};
use std::collections::HashMap;

/// Type checking errors
#[derive(Debug, thiserror::Error)]
pub enum TypeCheckError {
    #[error("Type checking error")]
    Diagnostic(Diagnostic),
}

impl TypeCheckError {
    pub fn type_mismatch(expected: FlintType, found: FlintType, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!(
            "Type mismatch: expected {}, found {}",
            type_to_string(&expected),
            type_to_string(&found)
        ))
        .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn undefined_variable(var_name: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Undefined variable: {}", var_name))
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn undefined_function(func_name: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Undefined function: {}", func_name))
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn invalid_operation(op: &str, left_type: &FlintType, right_type: &FlintType, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!(
            "Invalid operation '{}' between {} and {}",
            op,
            type_to_string(left_type),
            type_to_string(right_type)
        ))
        .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn invalid_call(func_type: &FlintType, arg_types: &[FlintType], location: SourceLocation) -> Self {
        let arg_types_str = arg_types.iter()
            .map(type_to_string)
            .collect::<Vec<_>>()
            .join(", ");
        let diagnostic = Diagnostic::error(format!(
            "Invalid function call: expected function type, found {}. Arguments: ({})",
            type_to_string(func_type),
            arg_types_str
        ))
        .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn variable_already_consumed(var_name: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Variable '{}' has already been consumed and cannot be used again", var_name))
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn variable_not_consumed(var_name: &str, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Variable '{}' must be consumed before going out of scope", var_name))
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
}

/// Type environment for variable and function types
#[derive(Debug, Clone)]
pub struct TypeEnvironment {
    pub variables: HashMap<String, FlintType>,
    pub functions: HashMap<String, FlintType>,
    pub c_imports: HashMap<String, String>, // module name -> header file
}

impl TypeEnvironment {
    pub fn new() -> Self {
        let mut env = Self {
            variables: HashMap::new(),
            functions: HashMap::new(),
            c_imports: HashMap::new(),
        };
        
        // Add built-in C functions from standard headers
        env.add_builtin_c_functions();
        env
    }
    
    fn add_builtin_c_functions(&mut self) {
        // We skip detailed type checking for C functions
        // The C compiler will handle that for us
        // This method is kept for potential future extensions
    }
    
    pub fn add_variable(&mut self, name: String, var_type: FlintType) {
        self.variables.insert(name, var_type);
    }
    
    pub fn add_function(&mut self, name: String, func_type: FlintType) {
        self.functions.insert(name, func_type);
    }
    
    pub fn add_c_import(&mut self, module: String, header: String) {
        self.c_imports.insert(module, header);
    }
    
    pub fn get_variable(&self, name: &str) -> Option<&FlintType> {
        self.variables.get(name)
    }
    
    pub fn get_function(&self, name: &str) -> Option<&FlintType> {
        self.functions.get(name)
    }
    
    pub fn extend(&self) -> Self {
        self.clone()
    }
}

/// Type checker for Flint programs
pub struct TypeChecker {
    env: TypeEnvironment,
    current_location: Option<SourceLocation>,
    type_var_counter: usize,
    consumed_variables: std::collections::HashSet<String>,
    filename: String,
    source_text: String,
}

impl TypeChecker {
    pub fn new() -> Self {
        Self {
            env: TypeEnvironment::new(),
            current_location: None,
            type_var_counter: 0,
            consumed_variables: std::collections::HashSet::new(),
            filename: "unknown".to_string(),
            source_text: String::new(),
        }
    }
    
    pub fn with_source(mut self, filename: String, source_text: String) -> Self {
        self.filename = filename;
        self.source_text = source_text;
        self
    }
    
    pub fn fresh_type_var(&mut self) -> FlintType {
        let var_name = format!("T{}", self.type_var_counter);
        self.type_var_counter += 1;
        FlintType::TypeVar(var_name)
    }
    
    /// Get a default location for error reporting when specific location is not available
    fn default_location(&self) -> SourceLocation {
        SourceLocation::new(self.filename.clone(), 1, 1, 1)
    }
    
    /// Type check a complete program
    pub fn check_program(&mut self, program: &Program) -> Result<TypeEnvironment, TypeCheckError> {
        // First pass: collect all declarations and build environment
        for decl in &program.declarations {
            self.collect_declaration(decl)?;
        }
        
        // Second pass: type check all function bodies
        for decl in &program.declarations {
            if let Declaration::FunctionDef(func) = decl {
                self.check_function_definition(func)?;
            }
        }
        
        Ok(self.env.clone())
    }
    
    /// Collect type information from declarations
    fn collect_declaration(&mut self, decl: &Declaration) -> Result<(), TypeCheckError> {
        match decl {
            Declaration::TypeDef(_) => {
                // TODO: Handle type definitions
                Ok(())
            }
            
            Declaration::FunctionSig { name, type_sig } => {
                self.env.add_function(name.clone(), type_sig.clone());
                Ok(())
            }
            
            Declaration::FunctionDef(func) => {
                // Infer function type from its definition if not explicitly given
                if let Some(type_sig) = &func.type_signature {
                    self.env.add_function(func.name.clone(), type_sig.clone());
                } else {
                    // Check if this function already has a signature from a previous declaration
                    if !self.env.functions.contains_key(&func.name) {
                        // TODO: Implement function type inference
                        // For now, create a placeholder
                        let placeholder_type = FlintType::Function {
                            params: vec![],
                            result: Box::new(FlintType::Unit),
                            effects: vec![],
                        };
                        self.env.add_function(func.name.clone(), placeholder_type);
                    }
                    // If function already has a signature, keep using that type
                }
                Ok(())
            }
            
            Declaration::CImport(import) => {
                self.env.add_c_import(import.alias.clone(), import.header_file.clone());
                Ok(())
            }
            
            Declaration::EffectDecl(_) => {
                // TODO: Handle effect declarations
                Ok(())
            }
            
            Declaration::EffectHandler(_) => {
                // TODO: Handle effect handlers
                Ok(())
            }
            
            Declaration::Main(_) => {
                // Main declaration doesn't affect type environment
                Ok(())
            }
        }
    }
    
    /// Type check a function definition
    fn check_function_definition(&mut self, func: &FunctionDef) -> Result<(), TypeCheckError> {
        // Get the function's declared type if available
        let func_type = self.env.get_function(&func.name).cloned();
        
        // Save current consumed variables and start fresh for this function
        let saved_consumed = std::mem::replace(&mut self.consumed_variables, std::collections::HashSet::new());
        
        for clause in &func.clauses {
            self.check_function_clause(clause, func_type.as_ref())?;
        }
        
        // Restore previous consumed variables (though this might not be necessary)
        self.consumed_variables = saved_consumed;
        
        Ok(())
    }
    
    /// Type check a function clause
    fn check_function_clause(&mut self, clause: &FunctionClause, func_type: Option<&FlintType>) -> Result<(), TypeCheckError> {
        // Create new environment for this clause
        let mut clause_env = self.env.extend();
        
        // Add pattern variables to environment using declared types if available
        if let Some(FlintType::Function { params, .. }) = func_type {
            if params.len() == clause.patterns.len() {
                for (pattern, param_type) in clause.patterns.iter().zip(params.iter()) {
                    self.collect_pattern_variables_with_type(pattern, param_type, &mut clause_env)?;
                }
            } else {
                // Pattern count mismatch - for now, fall back to fresh variables
                for pattern in &clause.patterns {
                    self.collect_pattern_variables(pattern, &mut clause_env)?;
                }
            }
        } else {
            // No function type available - use fresh variables
            for pattern in &clause.patterns {
                self.collect_pattern_variables(pattern, &mut clause_env)?;
            }
        }
        
        // Type check the body with the extended environment
        let old_env = std::mem::replace(&mut self.env, clause_env);
        let _body_type = self.infer_expression_type(&clause.body)?;
        self.env = old_env;
        
        // TODO: Check that body type matches expected return type
        
        Ok(())
    }
    
    /// Collect variables from patterns and add them to environment
    fn collect_pattern_variables(&mut self, pattern: &Pattern, env: &mut TypeEnvironment) -> Result<(), TypeCheckError> {
        match pattern {
            Pattern::Var(var) => {
                // For now, assign a fresh type variable
                let var_type = self.fresh_type_var();
                env.add_variable(var.name.clone(), var_type);
                Ok(())
            }
            
            Pattern::Wildcard => Ok(()),
            
            Pattern::Int(_) => Ok(()),
            Pattern::Str(_) => Ok(()),
            Pattern::Bool(_) => Ok(()),
            Pattern::Unit => Ok(()),
            Pattern::EmptyList => Ok(()),
            
            Pattern::List(patterns) => {
                for pat in patterns {
                    self.collect_pattern_variables(pat, env)?;
                }
                Ok(())
            }
            
            Pattern::ListCons { head, tail } => {
                self.collect_pattern_variables(head, env)?;
                self.collect_pattern_variables(tail, env)?;
                Ok(())
            }
            
            Pattern::Record { .. } => {
                // TODO: Handle record patterns
                Ok(())
            }
        }
    }
    
    /// Collect variables from patterns with explicit types
    fn collect_pattern_variables_with_type(&mut self, pattern: &Pattern, pattern_type: &FlintType, env: &mut TypeEnvironment) -> Result<(), TypeCheckError> {
        match pattern {
            Pattern::Var(var) => {
                env.add_variable(var.name.clone(), pattern_type.clone());
                Ok(())
            }
            
            Pattern::Wildcard => Ok(()),
            
            Pattern::Int(_) => Ok(()),
            Pattern::Str(_) => Ok(()),
            Pattern::Bool(_) => Ok(()),
            Pattern::Unit => Ok(()),
            Pattern::EmptyList => Ok(()),
            
            Pattern::List(patterns) => {
                // For list patterns, all elements should have the same type
                if let FlintType::List(element_type) = pattern_type {
                    for pat in patterns {
                        self.collect_pattern_variables_with_type(pat, element_type, env)?;
                    }
                } else {
                    // Type mismatch - fall back to inference
                    for pat in patterns {
                        self.collect_pattern_variables(pat, env)?;
                    }
                }
                Ok(())
            }
            
            Pattern::ListCons { head, tail } => {
                if let FlintType::List(element_type) = pattern_type {
                    self.collect_pattern_variables_with_type(head, element_type, env)?;
                    self.collect_pattern_variables_with_type(tail, pattern_type, env)?;
                } else {
                    // Type mismatch - fall back to inference
                    self.collect_pattern_variables(head, env)?;
                    self.collect_pattern_variables(tail, env)?;
                }
                Ok(())
            }
            
            Pattern::Record { .. } => {
                // TODO: Handle record patterns
                Ok(())
            }
        }
    }
    
    /// Infer the type of an expression
    pub fn infer_expression_type(&mut self, expr: &Expr) -> Result<FlintType, TypeCheckError> {
        match expr {
            Expr::Var(var) => {
                // First check if it's a function
                if let Some(func_type) = self.env.get_function(&var.name) {
                    // Functions are not consumed like variables
                    return Ok(func_type.clone());
                }
                
                // Check if variable has already been consumed
                if self.consumed_variables.contains(&var.name) {
                    let location = if let Some(ref loc) = var.location {
                        loc.clone()
                    } else {
                        self.default_location()
                    };
                    return Err(TypeCheckError::variable_already_consumed(&var.name, location));
                }
                
                if let Some(var_type) = self.env.get_variable(&var.name) {
                    // Mark variable as consumed
                    self.consumed_variables.insert(var.name.clone());
                    Ok(var_type.clone())
                } else {
                    let location = if let Some(ref loc) = var.location {
                        loc.clone()
                    } else {
                        self.default_location()
                    };
                    Err(TypeCheckError::undefined_variable(&var.name, location))
                }
            }
            
            Expr::NonConsumptiveVar(var) => {
                // Non-consumptive variables are not consumed, just copied
                if let Some(var_type) = self.env.get_variable(&var.name) {
                    // Do NOT mark variable as consumed - this is a copy operation
                    Ok(var_type.clone())
                } else {
                    let location = if let Some(ref loc) = var.location {
                        loc.clone()
                    } else {
                        self.default_location()
                    };
                    Err(TypeCheckError::undefined_variable(&var.name, location))
                }
            }
            
            Expr::Int(_) => Ok(FlintType::Int32),
            Expr::Str(_) => Ok(FlintType::String),
            Expr::Bool(_) => Ok(FlintType::Bool),
            Expr::Unit => Ok(FlintType::Unit),
            
            Expr::List(elements) => {
                if elements.is_empty() {
                    // Empty list - use type variable for element type
                    Ok(FlintType::List(Box::new(self.fresh_type_var())))
                } else {
                    // Infer type from first element, check others match
                    let element_type = self.infer_expression_type(&elements[0])?;
                    
                    for element in &elements[1..] {
                        let elem_type = self.infer_expression_type(element)?;
                        if !self.types_compatible(&element_type, &elem_type) {
                            let location = self.default_location();
                            return Err(TypeCheckError::type_mismatch(element_type, elem_type, location));
                        }
                    }
                    
                    Ok(FlintType::List(Box::new(element_type)))
                }
            }
            
            Expr::ListCons { head, tail } => {
                let head_type = self.infer_expression_type(head)?;
                let tail_type = self.infer_expression_type(tail)?;
                
                // Tail should be a list of the same type as head
                let expected_tail_type = FlintType::List(Box::new(head_type.clone()));
                if !self.types_compatible(&expected_tail_type, &tail_type) {
                    let location = self.default_location();
                    return Err(TypeCheckError::type_mismatch(expected_tail_type, tail_type, location));
                }
                
                Ok(FlintType::List(Box::new(head_type)))
            }
            
            Expr::BinOp { op, left, right } => {
                let left_type = self.infer_expression_type(left)?;
                let right_type = self.infer_expression_type(right)?;
                
                self.check_binary_operation(op, &left_type, &right_type)
            }
            
            Expr::Call { func, args } => {
                let func_type = self.infer_expression_type(func)?;
                let arg_types: Result<Vec<_>, _> = args.iter()
                    .map(|arg| self.infer_expression_type(arg))
                    .collect();
                let arg_types = arg_types?;
                
                self.check_function_call(&func_type, &arg_types)
            }
            
            Expr::CCall { module: _, function: _, args } => {
                // Skip detailed type checking for C calls - let the C compiler handle it
                // Just check that the arguments are well-typed
                for arg in args {
                    let _ = self.infer_expression_type(arg)?;
                }
                
                // Assume C functions return int by default
                // In the future, we could parse C headers or use clang for proper type checking
                Ok(FlintType::Int32)
            }
            
            Expr::Let { var, value, body } => {
                let value_type = self.infer_expression_type(value)?;
                
                // Add variable to environment
                let mut extended_env = self.env.extend();
                extended_env.add_variable(var.name.clone(), value_type);
                
                // Type check body with extended environment
                let old_env = std::mem::replace(&mut self.env, extended_env);
                let body_type = self.infer_expression_type(body)?;
                self.env = old_env;
                
                Ok(body_type)
            }
            
            Expr::LetTyped { var: _, var_type, value } => {
                let value_type = self.infer_expression_type(value)?;
                
                // Check that value type matches declared type
                if !self.types_compatible(var_type, &value_type) {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    return Err(TypeCheckError::type_mismatch(var_type.clone(), value_type, location));
                }
                
                // LetTyped is a statement, not an expression, so it doesn't have a meaningful return type
                // But we'll return Unit for consistency
                Ok(FlintType::Unit)
            }
            
            Expr::Block { statements, result } => {
                // Type check all statements
                for statement in statements {
                    self.check_statement(statement)?;
                }
                
                // Type of block is type of result expression, or Unit if no result
                if let Some(result_expr) = result {
                    self.infer_expression_type(result_expr)
                } else {
                    Ok(FlintType::Unit)
                }
            }
            
            _ => {
                // For unimplemented expression types, return a type variable
                Ok(self.fresh_type_var())
            }
        }
    }
    
    /// Check a statement for type correctness
    fn check_statement(&mut self, statement: &Statement) -> Result<(), TypeCheckError> {
        match statement {
            Statement::LetTyped { var, var_type, value } => {
                let value_type = self.infer_expression_type(value)?;
                
                // Check that value type matches declared type
                if !self.types_compatible(var_type, &value_type) {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    return Err(TypeCheckError::type_mismatch(var_type.clone(), value_type, location));
                }
                
                // Add variable to environment
                self.env.add_variable(var.name.clone(), var_type.clone());
                Ok(())
            }
            
            Statement::Let { var, value } => {
                // Infer the type from the value
                let value_type = self.infer_expression_type(value)?;
                
                // Add variable to environment with inferred type
                self.env.add_variable(var.name.clone(), value_type);
                Ok(())
            }
            
            Statement::Expr(expr) => {
                self.infer_expression_type(expr)?;
                Ok(())
            }
        }
    }
    
    /// Check if a binary operation is valid for the given types
    fn check_binary_operation(&self, op: &BinOp, left_type: &FlintType, right_type: &FlintType) -> Result<FlintType, TypeCheckError> {
        match op {
            BinOp::Add | BinOp::Sub | BinOp::Mul | BinOp::Div | BinOp::Mod => {
                // Arithmetic operations require numeric types
                if self.is_numeric_type(left_type) && self.is_numeric_type(right_type) {
                    // For now, assume all numeric operations return the same type as operands
                    // TODO: Implement proper numeric type coercion
                    if self.types_compatible(left_type, right_type) {
                        Ok(left_type.clone())
                    } else {
                        let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                        Err(TypeCheckError::invalid_operation(&format!("{:?}", op), left_type, right_type, location))
                    }
                } else {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    Err(TypeCheckError::invalid_operation(&format!("{:?}", op), left_type, right_type, location))
                }
            }
            
            BinOp::Eq | BinOp::Ne => {
                // Equality operations require compatible types
                if self.types_compatible(left_type, right_type) {
                    Ok(FlintType::Bool)
                } else {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    Err(TypeCheckError::invalid_operation(&format!("{:?}", op), left_type, right_type, location))
                }
            }
            
            BinOp::Lt | BinOp::Le | BinOp::Gt | BinOp::Ge => {
                // Comparison operations require orderable types
                if self.is_orderable_type(left_type) && self.is_orderable_type(right_type) && 
                   self.types_compatible(left_type, right_type) {
                    Ok(FlintType::Bool)
                } else {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    Err(TypeCheckError::invalid_operation(&format!("{:?}", op), left_type, right_type, location))
                }
            }
            
            BinOp::And | BinOp::Or => {
                // Logical operations require boolean types
                if matches!(left_type, FlintType::Bool) && matches!(right_type, FlintType::Bool) {
                    Ok(FlintType::Bool)
                } else {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    Err(TypeCheckError::invalid_operation(&format!("{:?}", op), left_type, right_type, location))
                }
            }
            
            BinOp::Append => {
                // List append or string concatenation
                match (left_type, right_type) {
                    (FlintType::List(elem_type1), FlintType::List(elem_type2)) => {
                        if self.types_compatible(elem_type1, elem_type2) {
                            Ok(left_type.clone())
                        } else {
                            let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                            Err(TypeCheckError::invalid_operation("append", left_type, right_type, location))
                        }
                    }
                    (FlintType::String, FlintType::String) => {
                        Ok(FlintType::String)
                    }
                    _ => {
                        let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                        Err(TypeCheckError::invalid_operation("append", left_type, right_type, location))
                    }
                }
            }
        }
    }
    
    /// Check if a function call is valid
    fn check_function_call(&self, func_type: &FlintType, arg_types: &[FlintType]) -> Result<FlintType, TypeCheckError> {
        match func_type {
            FlintType::Function { params, result, .. } => {
                // Check parameter count and types
                if params.len() != arg_types.len() {
                    let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                    return Err(TypeCheckError::invalid_call(func_type, arg_types, location));
                }
                
                for (param_type, arg_type) in params.iter().zip(arg_types.iter()) {
                    if !self.types_compatible(param_type, arg_type) {
                        let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                        return Err(TypeCheckError::invalid_call(func_type, arg_types, location));
                    }
                }
                
                Ok((**result).clone())
            }
            
            _ => {
                let location = SourceLocation::new("unknown".to_string(), 0, 0, 0);
                Err(TypeCheckError::invalid_call(func_type, arg_types, location))
            }
        }
    }
    
    /// Check if two types are compatible
    fn types_compatible(&self, type1: &FlintType, type2: &FlintType) -> bool {
        match (type1, type2) {
            (FlintType::Int32, FlintType::Int32) => true,
            (FlintType::String, FlintType::String) => true,
            (FlintType::Bool, FlintType::Bool) => true,
            (FlintType::Unit, FlintType::Unit) => true,
            
            (FlintType::List(elem1), FlintType::List(elem2)) => {
                self.types_compatible(elem1, elem2)
            }
            
            (FlintType::Function { params: p1, result: r1, .. }, 
             FlintType::Function { params: p2, result: r2, .. }) => {
                p1.len() == p2.len() &&
                p1.iter().zip(p2.iter()).all(|(t1, t2)| self.types_compatible(t1, t2)) &&
                self.types_compatible(r1, r2)
            }
            
            (FlintType::Named(n1), FlintType::Named(n2)) => n1 == n2,
            
            // Type variables are compatible with anything for now
            (FlintType::TypeVar(_), _) => true,
            (_, FlintType::TypeVar(_)) => true,
            
            _ => false,
        }
    }
    
    /// Check if a type is numeric
    fn is_numeric_type(&self, typ: &FlintType) -> bool {
        matches!(typ, FlintType::Int32)
    }
    
    /// Check if a type is orderable
    fn is_orderable_type(&self, typ: &FlintType) -> bool {
        matches!(typ, FlintType::Int32 | FlintType::String)
    }
}

/// Convert a FlintType to a readable string
fn type_to_string(typ: &FlintType) -> String {
    match typ {
        FlintType::Int32 => "i32".to_string(),
        FlintType::String => "Str".to_string(),
        FlintType::Bool => "Bool".to_string(),
        FlintType::Unit => "()".to_string(),
        FlintType::List(elem_type) => format!("List<{}>", type_to_string(elem_type)),
        FlintType::Function { params, result, effects } => {
            let params_str = params.iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            let effects_str = if effects.is_empty() {
                String::new()
            } else {
                format!(" using {}", effects.join(", "))
            };
            format!("({}) -> {}{}", params_str, type_to_string(result), effects_str)
        }
        FlintType::Record { fields } => {
            let fields_str = fields.iter()
                .map(|(name, typ)| format!("{}: {}", name, type_to_string(typ)))
                .collect::<Vec<_>>()
                .join(", ");
            format!("{{ {} }}", fields_str)
        }
        FlintType::Named(name) => name.clone(),
        FlintType::TypeVar(name) => name.clone(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::FlintLexer;
    use crate::parser::FlintParser;
    
    #[test]
    fn test_basic_type_inference() {
        let mut checker = TypeChecker::new();
        
        // Test integer literal
        let int_expr = Expr::Int(42);
        let int_type = checker.infer_expression_type(&int_expr).unwrap();
        assert!(matches!(int_type, FlintType::Int32));
        
        // Test string literal
        let str_expr = Expr::Str("hello".to_string());
        let str_type = checker.infer_expression_type(&str_expr).unwrap();
        assert!(matches!(str_type, FlintType::String));
        
        // Test boolean literal
        let bool_expr = Expr::Bool(true);
        let bool_type = checker.infer_expression_type(&bool_expr).unwrap();
        assert!(matches!(bool_type, FlintType::Bool));
    }
    
    #[test]
    fn test_binary_operations() {
        let mut checker = TypeChecker::new();
        
        // Test addition
        let add_expr = Expr::BinOp {
            op: BinOp::Add,
            left: Box::new(Expr::Int(1)),
            right: Box::new(Expr::Int(2)),
        };
        let add_type = checker.infer_expression_type(&add_expr).unwrap();
        assert!(matches!(add_type, FlintType::Int32));
        
        // Test comparison
        let cmp_expr = Expr::BinOp {
            op: BinOp::Lt,
            left: Box::new(Expr::Int(1)),
            right: Box::new(Expr::Int(2)),
        };
        let cmp_type = checker.infer_expression_type(&cmp_expr).unwrap();
        assert!(matches!(cmp_type, FlintType::Bool));
    }
    
    #[test]
    fn test_type_errors() {
        let mut checker = TypeChecker::new();
        
        // Test type mismatch in addition
        let bad_add = Expr::BinOp {
            op: BinOp::Add,
            left: Box::new(Expr::Int(1)),
            right: Box::new(Expr::Str("hello".to_string())),
        };
        assert!(checker.infer_expression_type(&bad_add).is_err());
    }
}
