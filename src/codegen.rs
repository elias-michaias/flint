// Code generation for Flint functional logic language
use crate::ast::*;
use crate::diagnostic::{Diagnostic, SourceLocation};
use std::collections::HashMap;
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
    debug: bool,
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
            debug: false,
        }
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

    /// Convert a Flint variable to a C variable name
    fn var_to_c_name(&self, var: &Variable) -> String {
        if var.is_logic_var {
            format!("logic_{}", var.name)
        } else {
            format!("var_{}", var.name)
        }
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

    /// Generate C code for a complete program
    pub fn generate(&mut self, program: &Program) -> Result<String, CodegenError> {
        self.generate_with_context(program, "<unknown>", "")
    }
    
    /// Generate C code for a complete program with source context
    pub fn generate_with_context(&mut self, program: &Program, filename: &str, source: &str) -> Result<String, CodegenError> {
        self.output.clear();
        
        let mut ctx = CompilationContext::with_source(filename.to_string(), source.to_string());
        
        // Generate C header includes
        self.generate_header()?;
        
        // Process C imports first
        for import in program.c_imports() {
            self.process_c_import(import)?;
        }
        
        // Generate type definitions
        for typedef in program.types() {
            self.generate_type_def(typedef, &ctx)?;
        }
        
        // Generate effect declarations
        for effect in program.effects() {
            self.generate_effect_decl(effect, &ctx)?;
        }
        
        // Generate effect handlers
        for handler in program.handlers() {
            self.generate_effect_handler(handler, &mut ctx)?;
        }
        
        // Generate function declarations first (skip main function - it's handled specially)
        for func in program.functions() {
            if func.name != "main" {
                self.generate_function_declaration(func, &ctx)?;
            }
        }
        
        // Generate function definitions (skip main function - it's handled specially)
        for func in program.functions() {
            if func.name != "main" {
                self.generate_function_definition(func, &mut ctx)?;
            }
        }
        
        // Generate main function
        if let Some(main_func_name) = program.main_function() {
            self.generate_main_function_with_body(program, main_func_name, &ctx)?;
        } else if let Some(main_func) = program.functions().iter().find(|f| f.name == "main") {
            // If there's a function named "main", embed its body in the C main
            self.generate_main_function_with_body(program, "main", &ctx)?;
        } else {
            self.generate_default_main()?;
        }
        
        // Generate runtime functions
        self.generate_runtime_functions()?;
        
        Ok(self.output.clone())
    }

    fn generate_header(&mut self) -> Result<(), CodegenError> {
        writeln!(self.output, "// Generated by Flint compiler")?;
        writeln!(self.output, "#include <stdio.h>")?;
        writeln!(self.output, "#include <stdlib.h>")?;
        writeln!(self.output, "#include <string.h>")?;
        writeln!(self.output, "#include <stdbool.h>")?;
        writeln!(self.output, "#include <stdint.h>")?;
        writeln!(self.output, "#include \"../runtime/runtime.h\"")?;
        writeln!(self.output)?;
        
        // Forward declarations for common types
        writeln!(self.output, "// Type definitions")?;
        writeln!(self.output, "typedef struct flint_list flint_list_t;")?;
        writeln!(self.output, "typedef struct flint_record flint_record_t;")?;
        writeln!(self.output, "typedef void* flint_value_t;")?;
        writeln!(self.output, "typedef void (*function_ptr_t)(void);")?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn process_c_import(&mut self, import: &CImport) -> Result<(), CodegenError> {
        // List of headers that are already included by default
        let default_headers = ["stdio.h", "stdlib.h", "string.h", "stdbool.h", "stdint.h"];
        
        if default_headers.contains(&import.header_file.as_str()) {
            writeln!(self.output, "// C Import: {} as {} (already included)", import.header_file, import.alias)?;
        } else {
            writeln!(self.output, "// C Import: {} as {}", import.header_file, import.alias)?;
            writeln!(self.output, "#include \"{}\"", import.header_file)?;
        }
        writeln!(self.output)?;
        Ok(())
    }

    fn generate_type_def(&mut self, typedef: &TypeDef, _ctx: &CompilationContext) -> Result<(), CodegenError> {
        match typedef {
            TypeDef::Record { name, fields } => {
                writeln!(self.output, "// Record type: {}", name)?;
                writeln!(self.output, "typedef struct {}_struct {{", name)?;
                
                for (field_name, field_type) in fields.iter() {
                    writeln!(self.output, "    {} {};", 
                           self.flint_type_to_c_type(field_type), field_name)?;
                }
                
                writeln!(self.output, "}} {}_t;", name)?;
                
                // Generate constructor function
                writeln!(self.output, "{}_t* create_{}(", name, name)?;
                for (i, (field_name, field_type)) in fields.iter().enumerate() {
                    if i > 0 { write!(self.output, ", ")?; }
                    write!(self.output, "{} {}", 
                           self.flint_type_to_c_type(field_type), field_name)?;
                }
                writeln!(self.output, ") {{")?;
                writeln!(self.output, "    {}_t* record = malloc(sizeof({}_t));", name, name)?;
                writeln!(self.output, "    if (!record) {{")?;
                writeln!(self.output, "        flint_runtime_error(\"Out of memory\");")?;
                writeln!(self.output, "    }}")?;
                
                for (field_name, _) in fields.iter() {
                    writeln!(self.output, "    record->{} = {};", field_name, field_name)?;
                }
                
                writeln!(self.output, "    return record;")?;
                writeln!(self.output, "}}")?;
                writeln!(self.output)?;
            }
            
            TypeDef::Alias { name, type_expr } => {
                writeln!(self.output, "// Type alias: {} = {:?}", name, type_expr)?;
                writeln!(self.output, "typedef {} {}_t;", 
                       self.flint_type_to_c_type(type_expr), name)?;
                writeln!(self.output)?;
            }
            
            TypeDef::Enum { name, variants } => {
                writeln!(self.output, "// Enum type: {}", name)?;
                writeln!(self.output, "typedef enum {{")?;
                for (i, variant) in variants.iter().enumerate() {
                    writeln!(self.output, "    {}_{} = {},", name.to_uppercase(), variant.name.to_uppercase(), i)?;
                }
                writeln!(self.output, "}} {}_t;", name)?;
                writeln!(self.output)?;
            }
        }
        Ok(())
    }

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

    fn generate_effect_decl(&mut self, effect: &EffectDecl, _ctx: &CompilationContext) -> Result<(), CodegenError> {
        writeln!(self.output, "// Effect declaration: {}", effect.name)?;
        writeln!(self.output, "typedef struct {}_effect {{", effect.name)?;
        writeln!(self.output, "    char* name;")?;
        
        // Generate operation function pointers based on signature
        for operation in &effect.operations {
            // For now, generate simple function pointers - would need to parse signature properly
            writeln!(self.output, "    void* (*{})(); // TODO: parse signature {:?}", 
                   operation.name, operation.signature)?;
        }
        
        writeln!(self.output, "}} {}_effect_t;", effect.name)?;
        writeln!(self.output)?;
        
        // Generate effect instance
        writeln!(self.output, "extern {}_effect_t {}_instance;", effect.name, effect.name)?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn generate_effect_handler(&mut self, handler: &EffectHandler, ctx: &mut CompilationContext) -> Result<(), CodegenError> {
        writeln!(self.output, "// Effect handler: {} handles {}", handler.handler_name, handler.effect_name)?;
        
        let mut operation_functions = HashMap::new();
        
        // Generate operation implementations
        for impl_def in &handler.implementations {
            let func_name = format!("{}_{}_impl", handler.handler_name, impl_def.operation_name);
            operation_functions.insert(impl_def.operation_name.clone(), func_name.clone());
            
            // Generate a simple function for now - return type would need proper parsing
            write!(self.output, "void* {}(", func_name)?;
            for (i, param) in impl_def.parameters.iter().enumerate() {
                if i > 0 { write!(self.output, ", ")?; }
                write!(self.output, "void* {}", self.var_to_c_name(param))?;
            }
            writeln!(self.output, ") {{")?;
            
            let mut local_ctx = ctx.clone();
            local_ctx.effect_handlers.push(handler.handler_name.clone());
            
            let body_code = self.generate_expr(&impl_def.body, &mut local_ctx)?;
            writeln!(self.output, "    return {};", body_code)?;
            writeln!(self.output, "}}")?;
            writeln!(self.output)?;
        }
        
        // Store handler info for later use
        self.effect_handlers.insert(handler.effect_name.clone(), EffectHandlerInfo {
            effect_name: handler.effect_name.clone(),
            handler_name: handler.handler_name.clone(),
            operation_functions,
        });
        
        Ok(())
    }

    fn generate_function_declaration(&mut self, func: &FunctionDef, _ctx: &CompilationContext) -> Result<(), CodegenError> {
        // Determine if this is a Unit -> Unit function (like main)
        let is_unit_to_unit = func.type_signature.as_ref()
            .map(|t| matches!(t, FlintType::Function { params, result, .. } 
                if params.is_empty() && matches!(**result, FlintType::Unit)))
            .unwrap_or(false);
        
        if is_unit_to_unit {
            // Generate void function with no parameters
            writeln!(self.output, "void {}();", func.name)?;
        } else {
            // Generate function with parameters
            write!(self.output, "void* {}(", func.name)?;
            
            // Generate parameters based on the first clause patterns
            if let Some(first_clause) = func.clauses.first() {
                for (i, _pattern) in first_clause.patterns.iter().enumerate() {
                    if i > 0 { write!(self.output, ", ")?; }
                    write!(self.output, "void* param_{}", i)?;
                }
            }
            
            writeln!(self.output, ");")?;
        }
        
        // Store function info for analysis
        self.functions.insert(func.name.clone(), FunctionInfo {
            name: func.name.clone(),
            is_deterministic: self.is_function_deterministic(func),
            has_logic_vars: self.function_has_logic_vars(func),
            effects: self.extract_function_effects(func),
        });
        
        Ok(())
    }

    fn generate_function_definition(&mut self, func: &FunctionDef, ctx: &mut CompilationContext) -> Result<(), CodegenError> {
        writeln!(self.output, "// Function: {}", func.name)?;
        
        // Determine if this is a Unit -> Unit function (like main)
        let is_unit_to_unit = func.type_signature.as_ref()
            .map(|t| matches!(t, FlintType::Function { params, result, .. } 
                if params.is_empty() && matches!(**result, FlintType::Unit)))
            .unwrap_or(false);
        
        if is_unit_to_unit {
            // Generate void function with no parameters
            writeln!(self.output, "void {}() {{", func.name)?;
        } else {
            // Generate function with parameters
            write!(self.output, "void* {}(", func.name)?;
            
            // Generate parameters based on the first clause patterns
            if let Some(first_clause) = func.clauses.first() {
                for (i, _pattern) in first_clause.patterns.iter().enumerate() {
                    if i > 0 { write!(self.output, ", ")?; }
                    write!(self.output, "void* param_{}", i)?;
                }
            }
            
            writeln!(self.output, ") {{")?;
        }
        
        // Generate function body - handle multiple clauses with pattern matching
        let mut local_ctx = ctx.clone();
        
        // Handle logic variables if present
        if self.function_has_logic_vars(func) {
            writeln!(self.output, "    // Logic variable initialization")?;
            writeln!(self.output, "    flint_init_logic_vars();")?;
        }
        
        // Handle effects if present
        if self.function_has_effects(func) {
            writeln!(self.output, "    // Effect handling setup")?;
            local_ctx.effect_handlers = self.extract_function_effects(func);
        }
        
        // Generate clause matching
        for (clause_idx, clause) in func.clauses.iter().enumerate() {
            writeln!(self.output, "    // Clause {}", clause_idx + 1)?;
            
            // Generate pattern matching for this clause
            let match_condition = self.generate_pattern_matching(&clause.patterns, &mut local_ctx)?;
            
            if !match_condition.is_empty() {
                writeln!(self.output, "    if ({}) {{", match_condition)?;
            } else {
                writeln!(self.output, "    // Always matches")?;
                writeln!(self.output, "    {{")?;
            }
            
            // Generate clause body
            let body_code = self.generate_expr(&clause.body, &mut local_ctx)?;
            
            if is_unit_to_unit {
                // For void functions, execute the expression but don't return its value
                writeln!(self.output, "        {};", body_code)?;
                writeln!(self.output, "        return;")?;
            } else {
                writeln!(self.output, "        return {};", body_code)?;
            }
            writeln!(self.output, "    }}")?;
        }
        
        if is_unit_to_unit {
            writeln!(self.output, "    // No clause matched")?;
        } else {
            writeln!(self.output, "    return NULL; // No clause matched")?;
        }
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn generate_pattern_matching(&mut self, patterns: &[Pattern], ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        let mut code = String::new();
        
        for (i, pattern) in patterns.iter().enumerate() {
            if i > 0 {
                code.push_str(" && ");
            }
            let param_name = format!("param_{}", i);
            code.push_str(&self.generate_single_pattern_match(pattern, &param_name, ctx)?);
        }
        
        Ok(code)
    }

    fn generate_single_pattern_match(&mut self, pattern: &Pattern, param_name: &str, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        match pattern {
            Pattern::Var(var) => {
                if var.is_logic_var {
                    Ok(format!("flint_unify({}, {})", self.var_to_c_name(var), param_name))
                } else {
                    Ok(format!("({} = {})", self.var_to_c_name(var), param_name))
                }
            }
            
            Pattern::Wildcard => {
                Ok("true".to_string())
            }
            
            Pattern::Int(n) => {
                Ok(format!("({} == {})", param_name, n))
            }
            
            Pattern::Str(s) => {
                Ok(format!("(strcmp({}, \"{}\") == 0)", param_name, s))
            }
            
            Pattern::Bool(b) => {
                Ok(format!("({} == {})", param_name, if *b { "true" } else { "false" }))
            }
            
            Pattern::Unit => {
                Ok("true".to_string()) // Unit always matches
            }
            
            Pattern::EmptyList => {
                Ok(format!("flint_list_is_empty({})", param_name))
            }
            
            Pattern::ListCons { head, tail } => {
                let head_var = self.fresh_var();
                let tail_var = self.fresh_var();
                let head_match = self.generate_single_pattern_match(head, &head_var, ctx)?;
                let tail_match = self.generate_single_pattern_match(tail, &tail_var, ctx)?;
                
                Ok(format!(
                    "(!flint_list_is_empty({}) && \
                     ({} = flint_list_head({}), {}) && \
                     ({} = flint_list_tail({}), {}))",
                    param_name,
                    head_var, param_name, head_match,
                    tail_var, param_name, tail_match
                ))
            }
            
            Pattern::List(patterns) => {
                let mut conditions = vec![
                    format!("flint_list_length({}) == {}", param_name, patterns.len())
                ];
                
                for (i, pattern) in patterns.iter().enumerate() {
                    let element_var = format!("flint_list_get({}, {})", param_name, i);
                    conditions.push(self.generate_single_pattern_match(pattern, &element_var, ctx)?);
                }
                
                Ok(format!("({})", conditions.join(" && ")))
            }
            
            Pattern::Record { type_name, fields } => {
                let mut conditions = Vec::new();
                
                if let Some(type_name) = type_name {
                    conditions.push(format!("flint_record_type_check({}, \"{}\")", param_name, type_name));
                }
                
                for (field_name, pattern) in fields {
                    let field_var = format!("flint_record_get({}, \"{}\")", param_name, field_name);
                    conditions.push(self.generate_single_pattern_match(pattern, &field_var, ctx)?);
                }
                
                if conditions.is_empty() {
                    Ok("true".to_string())
                } else {
                    Ok(format!("({})", conditions.join(" && ")))
                }
            }
        }
    }

    fn generate_expr(&mut self, expr: &Expr, ctx: &mut CompilationContext) -> Result<String, CodegenError> {
        match expr {
            Expr::Var(var) => {
                if ctx.in_logic_context && var.is_logic_var {
                    Ok(format!("flint_deref_logic_var({})", self.var_to_c_name(var)))
                } else {
                    Ok(self.var_to_c_name(var))
                }
            }
            
            Expr::Int(n) => {
                Ok(n.to_string())
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
            
            _ => {
                // For unsupported expression types, provide error
                Err(CodegenError::unsupported_feature(
                    &format!("expression type {}", self.expr_type_name(expr))
                ))
            }
        }
    }

    fn generate_c_call_expression(&mut self, module: &str, function: &str, args: &[Expr]) -> Result<String, CodegenError> {
        // Generate arguments
        let arg_codes: Result<Vec<_>, _> = args.iter()
            .map(|arg| self.generate_expr(arg, &mut CompilationContext::new()))
            .collect();
        let arg_codes = arg_codes?;
        
        // Simply use the function name directly - C.stdio.printf becomes printf
        Ok(format!("{}({})", function, arg_codes.join(", ")))
    }

    fn expr_type_name(&self, expr: &Expr) -> &'static str {
        match expr {
            Expr::Var(_) => "Var",
            Expr::Int(_) => "Int",
            Expr::Str(_) => "Str",
            Expr::Bool(_) => "Bool",
            Expr::Unit => "Unit",
            Expr::List(_) => "List",
            Expr::ListCons { .. } => "ListCons",
            Expr::Call { .. } => "Call",
            Expr::CCall { .. } => "CCall",
            Expr::Let { .. } => "Let",
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

    fn function_has_logic_vars(&self, func: &FunctionDef) -> bool {
        // Check if any clause patterns contain logic variables
        func.clauses.iter().any(|clause| {
            clause.patterns.iter().any(|pattern| self.pattern_has_logic_vars(pattern))
        })
    }

    fn pattern_has_logic_vars(&self, pattern: &Pattern) -> bool {
        match pattern {
            Pattern::Var(var) => var.is_logic_var,
            Pattern::ListCons { head, tail } => {
                self.pattern_has_logic_vars(head) || self.pattern_has_logic_vars(tail)
            }
            Pattern::List(patterns) => {
                patterns.iter().any(|p| self.pattern_has_logic_vars(p))
            }
            Pattern::Record { fields, .. } => {
                fields.iter().any(|(_, p)| self.pattern_has_logic_vars(p))
            }
            _ => false,
        }
    }

    fn is_function_deterministic(&self, func: &FunctionDef) -> bool {
        !self.function_has_effects(func) && !self.function_has_logic_vars(func)
    }

    fn function_has_effects(&self, func: &FunctionDef) -> bool {
        // Check if any clause body uses effects
        func.clauses.iter().any(|clause| self.expr_has_effects(&clause.body))
    }

    fn expr_has_effects(&self, expr: &Expr) -> bool {
        match expr {
            Expr::EffectCall { .. } => true,
            Expr::Handle { .. } => true,
            Expr::CCall { .. } => true, // C calls are effectful
            Expr::Call { func: _, args } => {
                // Check if function is known to have effects
                args.iter().any(|arg| self.expr_has_effects(arg))
            }
            Expr::Let { value, body, .. } => {
                self.expr_has_effects(value) || self.expr_has_effects(body)
            }
            Expr::BinOp { left, right, .. } => {
                self.expr_has_effects(left) || self.expr_has_effects(right)
            }
            _ => false,
        }
    }

    fn extract_function_effects(&self, _func: &FunctionDef) -> Vec<String> {
        // TODO: Analyze function body for effect usage
        Vec::new()
    }

    fn generate_main_function_with_body(&mut self, program: &Program, main_func_name: &str, ctx: &CompilationContext) -> Result<(), CodegenError> {
        writeln!(self.output, "int main(void) {{")?;
        writeln!(self.output, "    // Initialize Flint runtime")?;
        writeln!(self.output, "    flint_init_runtime();")?;
        writeln!(self.output)?;
        
        // Find the main function and embed its body
        if let Some(main_func) = program.functions().iter().find(|f| f.name == main_func_name) {
            writeln!(self.output, "    // Flint main function body")?;
            
            // Generate the main function body directly here
            for clause in &main_func.clauses {
                self.generate_clause_body_for_main(clause, ctx)?;
            }
        } else {
            writeln!(self.output, "    printf(\"Main function '{}' not found\\n\");", main_func_name)?;
        }
        
        writeln!(self.output)?;
        writeln!(self.output, "    // Cleanup runtime")?;
        writeln!(self.output, "    flint_cleanup_runtime();")?;
        writeln!(self.output, "    return 0;")?;
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn generate_clause_body_for_main(&mut self, clause: &FunctionClause, ctx: &CompilationContext) -> Result<(), CodegenError> {
        // For main function, we don't need pattern matching complexity
        // Just execute the body directly
        writeln!(self.output, "    {{")?;
        self.generate_expression_statement(&clause.body, ctx)?;
        writeln!(self.output, "    }}")?;
        Ok(())
    }

    fn generate_expression_statement(&mut self, expr: &Expr, ctx: &CompilationContext) -> Result<(), CodegenError> {
        match expr {
            Expr::CCall { module, function, args } => {
                // Handle C calls as statements (no return value used)
                write!(self.output, "        ")?;
                let call_code = self.generate_c_call_expression(module, function, args)?;
                write!(self.output, "{}", call_code)?;
                writeln!(self.output, ";")?;
            }
            _ => {
                // For other expressions, generate as normal but don't use the result
                write!(self.output, "        ")?;
                let expr_code = self.generate_expr(expr, &mut ctx.clone())?;
                write!(self.output, "{}", expr_code)?;
                writeln!(self.output, ";")?;
            }
        }
        Ok(())
    }

    fn generate_main_function(&mut self, main_func_name: &str) -> Result<(), CodegenError> {
        writeln!(self.output, "int main(void) {{")?;
        writeln!(self.output, "    // Initialize Flint runtime")?;
        writeln!(self.output, "    flint_init_runtime();")?;
        writeln!(self.output)?;
        
        writeln!(self.output, "    // Call main function")?;
        writeln!(self.output, "    {}();", main_func_name)?;
        writeln!(self.output)?;
        
        writeln!(self.output, "    // Cleanup runtime")?;
        writeln!(self.output, "    flint_cleanup_runtime();")?;
        writeln!(self.output, "    return 0;")?;
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn generate_default_main(&mut self) -> Result<(), CodegenError> {
        writeln!(self.output, "int main(void) {{")?;
        writeln!(self.output, "    // Initialize Flint runtime")?;
        writeln!(self.output, "    flint_init_runtime();")?;
        writeln!(self.output)?;
        
        writeln!(self.output, "    printf(\"No main function defined\\n\");")?;
        writeln!(self.output)?;
        
        writeln!(self.output, "    // Cleanup runtime")?;
        writeln!(self.output, "    flint_cleanup_runtime();")?;
        writeln!(self.output, "    return 0;")?;
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn generate_runtime_functions(&mut self) -> Result<(), CodegenError> {
        writeln!(self.output, "// Runtime helper functions")?;
        writeln!(self.output, "void flint_runtime_error(const char* message) {{")?;
        writeln!(self.output, "    fprintf(stderr, \"Flint runtime error: %s\\n\", message);")?;
        writeln!(self.output, "    exit(1);")?;
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fresh_var_generation() {
        let mut codegen = CodeGenerator::new();
        assert_eq!(codegen.fresh_var(), "v0");
        assert_eq!(codegen.fresh_var(), "v1");
        assert_eq!(codegen.fresh_var_with_prefix("test"), "test_2");
    }

    #[test]
    fn test_var_to_c_name() {
        let codegen = CodeGenerator::new();
        let logic_var = Variable { name: "X".to_string(), is_logic_var: true };
        let regular_var = Variable { name: "x".to_string(), is_logic_var: false };
        
        assert_eq!(codegen.var_to_c_name(&logic_var), "logic_X");
        assert_eq!(codegen.var_to_c_name(&regular_var), "var_x");
    }

    #[test]
    fn test_flint_type_to_c_type() {
        let codegen = CodeGenerator::new();
        assert_eq!(codegen.flint_type_to_c_type(&FlintType::Int32), "int32_t");
        assert_eq!(codegen.flint_type_to_c_type(&FlintType::String), "char*");
        assert_eq!(codegen.flint_type_to_c_type(&FlintType::Bool), "bool");
        assert_eq!(codegen.flint_type_to_c_type(&FlintType::Unit), "void");
    }
}
