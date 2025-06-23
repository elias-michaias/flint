use crate::ast::*;
use crate::lexer::{Token, TokenSpan};
use crate::diagnostic::{Diagnostic, SourceLocation};

#[derive(Debug)]
pub struct FlintParser {
    tokens: Vec<TokenSpan>,
    current: usize,
    filename: String,
}

#[derive(Debug, thiserror::Error)]
pub enum ParseError {
    #[error("Parse error")]
    Diagnostic(Diagnostic),
}

impl ParseError {
    pub fn unexpected_token(expected: String, found: Token, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Unexpected token: expected {}, found {:?}", expected, found))
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn unexpected_eof(location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error("Unexpected end of input".to_string())
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
    
    pub fn invalid_syntax(message: String, location: SourceLocation) -> Self {
        let diagnostic = Diagnostic::error(format!("Invalid syntax: {}", message))
            .with_location(location);
        Self::Diagnostic(diagnostic)
    }
}

impl FlintParser {
    pub fn new(tokens: Vec<TokenSpan>) -> Self {
        Self::new_with_filename(tokens, "input".to_string())
    }
    
    pub fn new_with_filename(tokens: Vec<TokenSpan>, filename: String) -> Self {
        Self { tokens, current: 0, filename }
    }
    
    fn current_token(&self) -> &Token {
        self.tokens.get(self.current).map(|ts| &ts.token).unwrap_or(&Token::Eof)
    }
    
    fn current_location(&self) -> SourceLocation {
        if let Some(token_span) = self.tokens.get(self.current) {
            SourceLocation::new(self.filename.clone(), token_span.line, token_span.column, token_span.length)
        } else {
            // End of file location
            let last_token = self.tokens.last();
            if let Some(last) = last_token {
                SourceLocation::new(self.filename.clone(), last.line, last.column + last.length, 1)
            } else {
                SourceLocation::new(self.filename.clone(), 1, 1, 1)
            }
        }
    }
    
    fn advance(&mut self) -> &Token {
        if self.current < self.tokens.len() {
            self.current += 1;
        }
        self.current_token()
    }
    
    fn peek_token(&self) -> Option<&Token> {
        self.tokens.get(self.current + 1).map(|ts| &ts.token)
    }
    
    fn expect(&mut self, expected: Token) -> Result<(), ParseError> {
        let current = self.current_token().clone();
        if std::mem::discriminant(&current) == std::mem::discriminant(&expected) {
            self.advance();
            Ok(())
        } else {
            let location = self.current_location();
            Err(ParseError::unexpected_token(format!("{:?}", expected), current, location))
        }
    }
    
    /// Parse a complete program
    pub fn parse_program(&mut self) -> Result<Program, ParseError> {
        let mut declarations = Vec::new();
        
        while !matches!(self.current_token(), Token::Eof) {
            declarations.push(self.parse_declaration()?);
        }
        
        Ok(Program { declarations })
    }
    
    /// Parse a top-level declaration
    fn parse_declaration(&mut self) -> Result<Declaration, ParseError> {
        match self.current_token() {
            Token::Import => self.parse_c_import(),
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();
                
                match self.current_token() {
                    Token::DoubleColon => {
                        self.advance();
                        
                        // Check if this is a type definition, effect, handler, or function
                        match self.current_token() {
                            Token::Record => self.parse_record_type_def(name),
                            Token::Effect => self.parse_effect_decl(name),
                            Token::Handler => self.parse_effect_handler(name),
                            _ => {
                                // Could be function signature or definition
                                if self.is_type_expression() {
                                    // Function type signature
                                    let type_sig = self.parse_type()?;
                                    Ok(Declaration::FunctionSig { name, type_sig })
                                } else {
                                    // Function definition with pattern
                                    let func_def = self.parse_function_def(name)?;
                                    Ok(Declaration::FunctionDef(func_def))
                                }
                            }
                        }
                    }
                    _ => {
                        let location = self.current_location();
                        Err(ParseError::invalid_syntax(
                            "Expected '::' after identifier in declaration".to_string(),
                            location
                        ))
                    }
                }
            }
            Token::Main => {
                // "main" can be used as a regular identifier for function declarations
                // or as a special main declaration like "main :: functionName"
                let name = "main".to_string();
                self.advance();
                
                match self.current_token() {
                    Token::DoubleColon => {
                        self.advance();
                        
                        // Check if this is a type signature or a main declaration
                        if self.is_type_expression() {
                            // Function type signature: main :: () -> ()
                            let type_sig = self.parse_type()?;
                            Ok(Declaration::FunctionSig { name, type_sig })
                        } else if let Token::LeftParen = self.current_token() {
                            // Function definition: main :: () => { ... }
                            let func_def = self.parse_function_def(name)?;
                            Ok(Declaration::FunctionDef(func_def))
                        } else if let Token::Identifier(main_func) = self.current_token() {
                            // Main declaration: main :: functionName
                            let main_func = main_func.clone();
                            self.advance();
                            Ok(Declaration::Main(main_func))
                        } else {
                            let location = self.current_location();
                            Err(ParseError::invalid_syntax("Expected type signature, function definition, or function name after 'main ::'".to_string(), location))
                        }
                    }
                    _ => {
                        let location = self.current_location();
                        Err(ParseError::invalid_syntax(
                            "Expected '::' after 'main'".to_string(),
                            location
                        ))
                    }
                }
            }
            _ => {
                let location = self.current_location();
                Err(ParseError::unexpected_token(
                    "declaration".to_string(),
                    self.current_token().clone(),
                    location
                ))
            }
        }
    }
    
    /// Parse C import: import C "header.h" as Module
    fn parse_c_import(&mut self) -> Result<Declaration, ParseError> {
        self.expect(Token::Import)?;
        self.expect(Token::Identifier("C".to_string()))?;
        
        if let Token::String(header_file) = self.current_token() {
            let header_file = header_file.clone();
            self.advance();
            self.expect(Token::As)?;
            
            if let Token::Identifier(alias) = self.current_token() {
                let alias = alias.clone();
                self.advance();
                
                Ok(Declaration::CImport(CImport { header_file, alias }))
            } else {
                Err(ParseError::invalid_syntax("Expected module alias after 'as'".to_string(), self.current_location()))
            }
        } else {
            Err(ParseError::invalid_syntax("Expected string literal for header file".to_string(), self.current_location()))
        }
    }
    
    /// Parse record type definition: User :: record { name: str, age: i32 }
    fn parse_record_type_def(&mut self, name: String) -> Result<Declaration, ParseError> {
        self.expect(Token::Record)?;
        self.expect(Token::LeftBrace)?;
        
        let mut fields = Vec::new();
        
        while !matches!(self.current_token(), Token::RightBrace) {
            if let Token::Identifier(field_name) = self.current_token() {
                let field_name = field_name.clone();
                self.advance();
                self.expect(Token::Colon)?;
                let field_type = self.parse_type()?;
                fields.push((field_name, field_type));
                
                // Optional comma
                if matches!(self.current_token(), Token::Comma) {
                    self.advance();
                }
            } else {
                return Err(ParseError::invalid_syntax("Expected field name".to_string(), self.current_location()));
            }
        }
        
        self.expect(Token::RightBrace)?;
        
        Ok(Declaration::TypeDef(TypeDef::Record { name, fields }))
    }
    
    /// Parse effect declaration: API :: effect { operations... }
    fn parse_effect_decl(&mut self, name: String) -> Result<Declaration, ParseError> {
        self.expect(Token::Effect)?;
        self.expect(Token::LeftBrace)?;
        
        let mut operations = Vec::new();
        
        while !matches!(self.current_token(), Token::RightBrace) {
            if let Token::Identifier(op_name) = self.current_token() {
                let op_name = op_name.clone();
                self.advance();
                self.expect(Token::DoubleColon)?;
                let signature = self.parse_type()?;
                operations.push(EffectOperation { name: op_name, signature });
                
                // Optional comma
                if matches!(self.current_token(), Token::Comma) {
                    self.advance();
                }
            } else {
                return Err(ParseError::invalid_syntax("Expected operation name".to_string(), self.current_location()));
            }
        }
        
        self.expect(Token::RightBrace)?;
        
        Ok(Declaration::EffectDecl(EffectDecl { name, operations }))
    }
    
    /// Parse effect handler: APIImpl :: handler for API { implementations... }
    fn parse_effect_handler(&mut self, handler_name: String) -> Result<Declaration, ParseError> {
        self.expect(Token::Handler)?;
        self.expect(Token::For)?;
        
        if let Token::Identifier(effect_name) = self.current_token() {
            let effect_name = effect_name.clone();
            self.advance();
            self.expect(Token::LeftBrace)?;
            
            let mut implementations = Vec::new();
            
            while !matches!(self.current_token(), Token::RightBrace) {
                if let Token::Identifier(op_name) = self.current_token() {
                    let op_name = op_name.clone();
                    self.advance();
                    self.expect(Token::DoubleColon)?;
                    
                    // Parse parameters
                    let mut parameters = Vec::new();
                    self.expect(Token::LeftParen)?;
                    
                    while !matches!(self.current_token(), Token::RightParen) {
                        let param = self.parse_variable()?;
                        parameters.push(param);
                        
                        if matches!(self.current_token(), Token::Comma) {
                            self.advance();
                        }
                    }
                    
                    self.expect(Token::RightParen)?;
                    self.expect(Token::FatArrow)?;
                    
                    let body = self.parse_expr()?;
                    
                    implementations.push(HandlerImpl {
                        operation_name: op_name,
                        parameters,
                        body,
                    });
                    
                    // Optional comma
                    if matches!(self.current_token(), Token::Comma) {
                        self.advance();
                    }
                } else {
                    return Err(ParseError::invalid_syntax("Expected operation name in handler".to_string(), self.current_location()));
                }
            }
            
            self.expect(Token::RightBrace)?;
            
            Ok(Declaration::EffectHandler(EffectHandler {
                effect_name,
                handler_name,
                implementations,
            }))
        } else {
            Err(ParseError::invalid_syntax("Expected effect name after 'for'".to_string(), self.current_location()))
        }
    }
    
    /// Parse function definition with clauses
    fn parse_function_def(&mut self, name: String) -> Result<FunctionDef, ParseError> {
        let mut clauses = Vec::new();
        
        // Parse first clause
        let first_clause = self.parse_function_clause()?;
        clauses.push(first_clause);
        
        // Parse additional clauses (if any)
        while matches!(self.current_token(), Token::Identifier(n) if n == &name) {
            self.advance(); // consume function name
            self.expect(Token::DoubleColon)?;
            let clause = self.parse_function_clause()?;
            clauses.push(clause);
        }
        
        Ok(FunctionDef {
            name,
            type_signature: None, // Will be inferred or set separately
            clauses,
        })
    }
    
    /// Parse a single function clause: (patterns...) => body
    fn parse_function_clause(&mut self) -> Result<FunctionClause, ParseError> {
        let mut patterns = Vec::new();
        
        // Parse patterns
        self.expect(Token::LeftParen)?;
        
        while !matches!(self.current_token(), Token::RightParen) {
            let pattern = self.parse_pattern()?;
            patterns.push(pattern);
            
            if matches!(self.current_token(), Token::Comma) {
                self.advance();
            }
        }
        
        self.expect(Token::RightParen)?;
        self.expect(Token::FatArrow)?;
        
        let body = self.parse_expr()?;
        
        Ok(FunctionClause {
            patterns,
            guard: None, // TODO: Add guard support
            body,
        })
    }
    
    /// Parse a pattern
    fn parse_pattern(&mut self) -> Result<Pattern, ParseError> {
        match self.current_token() {
            Token::LogicVar(name) => {
                let name = name.clone();
                let location = self.current_location();
                self.advance();
                Ok(Pattern::Var(Variable::new_logic(name).with_location(location)))
            }
            Token::Underscore => {
                self.advance();
                Ok(Pattern::Wildcard)
            }
            Token::Integer(n) => {
                let n = *n;
                self.advance();
                Ok(Pattern::Int(n))
            }
            Token::String(s) => {
                let s = s.clone();
                self.advance();
                Ok(Pattern::Str(s))
            }
            Token::True => {
                self.advance();
                Ok(Pattern::Bool(true))
            }
            Token::False => {
                self.advance();
                Ok(Pattern::Bool(false))
            }
            Token::LeftBracket => {
                self.advance();
                
                if matches!(self.current_token(), Token::RightBracket) {
                    self.advance();
                    Ok(Pattern::EmptyList)
                } else {
                    // Parse list pattern or list cons
                    let mut elements = Vec::new();
                    let mut has_tail = false;
                    
                    while !matches!(self.current_token(), Token::RightBracket) {
                        let pattern = self.parse_pattern()?;
                        elements.push(pattern);
                        
                        if matches!(self.current_token(), Token::Pipe) {
                            self.advance();
                            let tail_pattern = self.parse_pattern()?;
                            self.expect(Token::RightBracket)?;
                            
                            // Convert to nested ListCons patterns
                            let mut result = tail_pattern;
                            for element in elements.into_iter().rev() {
                                result = Pattern::ListCons {
                                    head: Box::new(element),
                                    tail: Box::new(result),
                                };
                            }
                            return Ok(result);
                        }
                        
                        if matches!(self.current_token(), Token::Comma) {
                            self.advance();
                        }
                    }
                    
                    self.expect(Token::RightBracket)?;
                    Ok(Pattern::List(elements))
                }
            }
            Token::LeftParen => {
                self.advance();
                if matches!(self.current_token(), Token::RightParen) {
                    self.advance();
                    Ok(Pattern::Unit)
                } else {
                    // Parenthesized pattern
                    let pattern = self.parse_pattern()?;
                    self.expect(Token::RightParen)?;
                    Ok(pattern)
                }
            }
            _ => Err(ParseError::invalid_syntax("Expected pattern".to_string(), self.current_location()))
        }
    }
    
    /// Parse an expression
    fn parse_expr(&mut self) -> Result<Expr, ParseError> {
        self.parse_let_expr()
    }
    
    /// Parse let expression: let $x = value in body
    fn parse_let_expr(&mut self) -> Result<Expr, ParseError> {
        if matches!(self.current_token(), Token::Let) {
            self.advance();
            let var = self.parse_variable()?;
            self.expect(Token::Equal)?;
            let value = self.parse_expr()?;
            self.expect(Token::In)?;
            let body = self.parse_expr()?;
            
            Ok(Expr::Let {
                var,
                value: Box::new(value),
                body: Box::new(body),
            })
        } else {
            self.parse_binary_expr()
        }
    }
    
    /// Parse binary expressions (simplified - just handle basic operators)
    fn parse_binary_expr(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_call_expr()?;
        
        while let Some(op) = self.current_binary_op() {
            self.advance();
            let right = self.parse_call_expr()?;
            left = Expr::BinOp {
                op,
                left: Box::new(left),
                right: Box::new(right),
            };
        }
        
        Ok(left)
    }
    
    /// Parse function call
    fn parse_call_expr(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.parse_primary_expr()?;
        
        while matches!(self.current_token(), Token::LeftParen | Token::Dot) {
            match self.current_token() {
                Token::LeftParen => {
                    self.advance();
                    let mut args = Vec::new();
                    
                    while !matches!(self.current_token(), Token::RightParen) {
                        args.push(self.parse_expr()?);
                        if matches!(self.current_token(), Token::Comma) {
                            self.advance();
                        }
                    }
                    
                    self.expect(Token::RightParen)?;
                    expr = Expr::Call {
                        func: Box::new(expr),
                        args,
                    };
                }
                Token::Dot => {
                    self.advance();
                    if let Token::Identifier(field) = self.current_token() {
                        let field = field.clone();
                        self.advance();
                        expr = Expr::FieldAccess {
                            expr: Box::new(expr),
                            field,
                        };
                    } else {
                        return Err(ParseError::invalid_syntax("Expected field name after '.'".to_string(), self.current_location()));
                    }
                }
                _ => break,
            }
        }
        
        Ok(expr)
    }
    
    /// Parse primary expressions
    fn parse_primary_expr(&mut self) -> Result<Expr, ParseError> {
        match self.current_token() {
            Token::Tilde => {
                self.advance(); // consume '~'
                match self.current_token() {
                    Token::LogicVar(name) => {
                        let name = name.clone();
                        let location = self.current_location();
                        self.advance();
                        Ok(Expr::NonConsumptiveVar(Variable::new_logic(name).with_location(location)))
                    }
                    _ => {
                        let location = self.current_location();
                        Err(ParseError::invalid_syntax("Expected logic variable after '~'".to_string(), location))
                    }
                }
            }
            Token::LogicVar(name) => {
                let name = name.clone();
                let location = self.current_location();
                self.advance();
                Ok(Expr::Var(Variable::new_logic(name).with_location(location)))
            }
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();
                
                // Check for C function call
                if matches!(self.current_token(), Token::Dot) {
                    // This might be C.Module.function
                    if name == "C" {
                        self.advance(); // consume '.'
                        if let Token::Identifier(module) = self.current_token() {
                            let module = module.clone();
                            self.advance();
                            self.expect(Token::Dot)?;
                            if let Token::Identifier(function) = self.current_token() {
                                let function = function.clone();
                                self.advance();
                                self.expect(Token::LeftParen)?;
                                
                                let mut args = Vec::new();
                                while !matches!(self.current_token(), Token::RightParen) {
                                    args.push(self.parse_expr()?);
                                    if matches!(self.current_token(), Token::Comma) {
                                        self.advance();
                                    }
                                }
                                self.expect(Token::RightParen)?;
                                
                                return Ok(Expr::CCall { module, function, args });
                            }
                        }
                    }
                }
                
                Ok(Expr::Var(Variable::new(name).with_location(self.current_location())))
            }
            Token::Integer(n) => {
                let n = *n;
                self.advance();
                Ok(Expr::Int(n))
            }
            Token::String(s) => {
                let s = s.clone();
                self.advance();
                Ok(Expr::Str(s))
            }
            Token::True => {
                self.advance();
                Ok(Expr::Bool(true))
            }
            Token::False => {
                self.advance();
                Ok(Expr::Bool(false))
            }
            Token::LeftBracket => {
                self.advance();
                
                if matches!(self.current_token(), Token::RightBracket) {
                    self.advance();
                    Ok(Expr::List(Vec::new()))
                } else {
                    let mut elements = Vec::new();
                    
                    while !matches!(self.current_token(), Token::RightBracket) {
                        let element = self.parse_expr()?;
                        elements.push(element);
                        
                        if matches!(self.current_token(), Token::Pipe) {
                            self.advance();
                            let tail = self.parse_expr()?;
                            self.expect(Token::RightBracket)?;
                            
                            // Convert to nested ListCons
                            let mut result = tail;
                            for element in elements.into_iter().rev() {
                                result = Expr::ListCons {
                                    head: Box::new(element),
                                    tail: Box::new(result),
                                };
                            }
                            return Ok(result);
                        }
                        
                        if matches!(self.current_token(), Token::Comma) {
                            self.advance();
                        }
                    }
                    
                    self.expect(Token::RightBracket)?;
                    Ok(Expr::List(elements))
                }
            }
            Token::LeftParen => {
                self.advance();
                if matches!(self.current_token(), Token::RightParen) {
                    self.advance();
                    Ok(Expr::Unit)
                } else {
                    let expr = self.parse_expr()?;
                    self.expect(Token::RightParen)?;
                    Ok(expr)
                }
            }
            Token::LeftBrace => {
                // Parse block expression: { statements; result_expr }
                self.parse_block()
            }
            Token::CModule(module) => {
                let module = module.clone();
                self.advance();
                self.expect(Token::Dot)?;
                
                if let Token::Identifier(function) = self.current_token() {
                    let function = function.clone();
                    self.advance();
                    self.expect(Token::LeftParen)?;
                    
                    let mut args = Vec::new();
                    while !matches!(self.current_token(), Token::RightParen) {
                        args.push(self.parse_expr()?);
                        if matches!(self.current_token(), Token::Comma) {
                            self.advance();
                        }
                    }
                    self.expect(Token::RightParen)?;
                    
                    Ok(Expr::CCall { module, function, args })
                } else {
                    let location = self.current_location();
                    Err(ParseError::invalid_syntax("Expected function name after C module".to_string(), location))
                }
            }
            _ => Err(ParseError::invalid_syntax("Expected expression".to_string(), self.current_location()))
        }
    }
    
    /// Parse a type expression
    fn parse_type(&mut self) -> Result<FlintType, ParseError> {
        let mut base_type = self.parse_base_type()?;
        
        // Handle function types with arrows
        while matches!(self.current_token(), Token::Arrow) {
            self.advance();
            let result_type = self.parse_type()?;
            
            // Convert to function type
            let params = if let FlintType::Function { params, .. } = base_type {
                params
            } else {
                vec![base_type]
            };
            
            // Parse effects if present
            let effects = if matches!(self.current_token(), Token::Using) {
                self.advance();
                self.parse_effect_list()?
            } else {
                Vec::new()
            };
            
            base_type = FlintType::Function {
                params,
                result: Box::new(result_type),
                effects,
            };
        }
        
        Ok(base_type)
    }
    
    /// Parse base types
    fn parse_base_type(&mut self) -> Result<FlintType, ParseError> {
        match self.current_token() {
            Token::I32 => {
                self.advance();
                Ok(FlintType::Int32)
            }
            Token::Str => {
                self.advance();
                Ok(FlintType::String)
            }
            Token::Bool => {
                self.advance();
                Ok(FlintType::Bool)
            }
            Token::Unit => {
                self.advance();
                Ok(FlintType::Unit)
            }
            Token::List => {
                self.advance();
                self.expect(Token::Less)?;
                let element_type = self.parse_type()?;
                self.expect(Token::Greater)?;
                Ok(FlintType::List(Box::new(element_type)))
            }
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();
                Ok(FlintType::Named(name))
            }
            Token::LeftParen => {
                self.advance();
                if matches!(self.current_token(), Token::RightParen) {
                    self.advance();
                    Ok(FlintType::Unit)
                } else {
                    let ty = self.parse_type()?;
                    self.expect(Token::RightParen)?;
                    Ok(ty)
                }
            }
            _ => Err(ParseError::invalid_syntax("Expected type".to_string(), self.current_location()))
        }
    }
    
    /// Parse effect list: Effect1, Effect2, Effect3
    fn parse_effect_list(&mut self) -> Result<Vec<String>, ParseError> {
        let mut effects = Vec::new();
        
        if let Token::Identifier(name) = self.current_token() {
            effects.push(name.clone());
            self.advance();
            
            while matches!(self.current_token(), Token::Comma) {
                self.advance();
                if let Token::Identifier(name) = self.current_token() {
                    effects.push(name.clone());
                    self.advance();
                } else {
                    return Err(ParseError::invalid_syntax("Expected effect name after ','".to_string(), self.current_location()));
                }
            }
        }
        
        Ok(effects)
    }
    
    /// Parse a variable (logic or regular)
    fn parse_variable(&mut self) -> Result<Variable, ParseError> {
        match self.current_token() {
            Token::LogicVar(name) => {
                let name = name.clone();
                let location = self.current_location();
                self.advance();
                Ok(Variable::new_logic(name).with_location(location))
            }
            Token::Identifier(name) => {
                let name = name.clone();
                let location = self.current_location();
                self.advance();
                Ok(Variable::new(name).with_location(location))
            }
            _ => Err(ParseError::invalid_syntax("Expected variable".to_string(), self.current_location()))
        }
    }
    
    /// Check if current position starts a type expression (not a function definition pattern)
    fn is_type_expression(&self) -> bool {
        match self.current_token() {
            Token::I32 | Token::Str | Token::Bool | Token::Unit | Token::List | Token::Identifier(_) => true,
            Token::LeftParen => {
                // Look ahead to distinguish between type `()` and function pattern `()`
                // If we see `() ->`, it's a type. If we see `() =>`, it's a function pattern.
                self.is_type_not_function_pattern()
            }
            _ => false
        }
    }
    
    /// Check if a parenthesized expression is a type (not a function pattern)
    fn is_type_not_function_pattern(&self) -> bool {
        let mut lookahead = self.current;
        
        // Skip past the opening parenthesis
        if lookahead < self.tokens.len() && matches!(self.tokens[lookahead].token, Token::LeftParen) {
            lookahead += 1;
            
            // Skip to the matching closing parenthesis
            let mut paren_depth = 1;
            while lookahead < self.tokens.len() && paren_depth > 0 {
                match &self.tokens[lookahead].token {
                    Token::LeftParen => paren_depth += 1,
                    Token::RightParen => paren_depth -= 1,
                    _ => {}
                }
                lookahead += 1;
            }
            
            // Now check what comes after the closing parenthesis
            if lookahead < self.tokens.len() {
                match &self.tokens[lookahead].token {
                    Token::Arrow => true,    // `() ->` indicates a type
                    Token::FatArrow => false, // `() =>` indicates a function pattern
                    _ => true  // Default to type if unclear
                }
            } else {
                true // Default to type if we hit end of input
            }
        } else {
            true // Default to type if not starting with parenthesis
        }
    }
    
    /// Get binary operator if current token is one
    fn current_binary_op(&self) -> Option<BinOp> {
        match self.current_token() {
            Token::Plus => Some(BinOp::Add),
            Token::Minus => Some(BinOp::Sub),
            Token::Multiply => Some(BinOp::Mul),
            Token::Divide => Some(BinOp::Div),
            Token::Modulo => Some(BinOp::Mod),
            Token::EqualEqual => Some(BinOp::Eq),
            Token::NotEqual => Some(BinOp::Ne),
            Token::Less => Some(BinOp::Lt),
            Token::LessEqual => Some(BinOp::Le),
            Token::Greater => Some(BinOp::Gt),
            Token::GreaterEqual => Some(BinOp::Ge),
            Token::And => Some(BinOp::And),
            Token::Or => Some(BinOp::Or),
            Token::PipeGt => Some(BinOp::Append),
            _ => None,
        }
    }
    
    /// Parse block expression: { statements; optional_expr }
    fn parse_block(&mut self) -> Result<Expr, ParseError> {
        self.advance(); // consume '{'
        
        let mut statements = Vec::new();
        let mut result = None;
        
        while !matches!(self.current_token(), Token::RightBrace) {
            // Save current position for backtracking
            let checkpoint = self.current;
            
            // Try to parse a statement first
            match self.try_parse_statement() {
                Ok(stmt) => {
                    statements.push(stmt);
                    
                    // Consume optional semicolon
                    if matches!(self.current_token(), Token::Semicolon) {
                        self.advance();
                    }
                }
                Err(_) => {
                    // Restore position and try to parse as final expression
                    self.current = checkpoint;
                    let expr = self.parse_block_expr()?;
                    result = Some(Box::new(expr));
                    break;
                }
            }
        }
        
        self.expect(Token::RightBrace)?;
        
        Ok(Expr::Block { statements, result })
    }
    
    /// Try to parse a statement (returns error if it's not a statement)
    fn try_parse_statement(&mut self) -> Result<Statement, ParseError> {
        match self.current_token() {
            Token::Let => {
                self.advance(); // consume 'let'
                
                // Parse variable - must be a logic variable with $
                let var = self.parse_variable()?;
                
                // Check if this is a typed let statement: let $var: Type = value
                if matches!(self.current_token(), Token::Colon) {
                    self.advance(); // consume ':'
                    let var_type = self.parse_type()?;
                    
                    // Expect assignment: = value
                    self.expect(Token::Equal)?;
                    let value = self.parse_expr()?;
                    
                    Ok(Statement::LetTyped { var, var_type, value })
                } else if matches!(self.current_token(), Token::Equal) {
                    // This is an untyped let statement: let $var = value
                    self.advance(); // consume '='
                    let value = self.parse_expr()?;
                    
                    Ok(Statement::Let { var, value })
                } else {
                    // Invalid let statement syntax
                    let location = self.current_location();
                    Err(ParseError::invalid_syntax("Expected ':' or '=' after variable in let statement".to_string(), location))
                }
            }
            _ => {
                // This is not a statement we recognize
                let location = self.current_location();
                Err(ParseError::invalid_syntax("Expected statement".to_string(), location))
            }
        }
    }
    
    /// Parse a block expression (used when statement parsing fails)
    fn parse_block_expr(&mut self) -> Result<Expr, ParseError> {
        // This parses the remaining expression in a block
        // It should not try to parse statements, just expressions
        self.parse_binary_expr()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::flint_lexer::tokenize;
    
    #[test]
    fn test_parse_simple_function() {
        let input = "reverse :: List<T> -> List<T>";
        let lex_result = tokenize(input).unwrap();
        let mut parser = FlintParser::new(lex_result.tokens);
        
        let program = parser.parse_program().unwrap();
        assert_eq!(program.declarations.len(), 1);
        
        if let Declaration::FunctionSig { name, .. } = &program.declarations[0] {
            assert_eq!(name, "reverse");
        } else {
            panic!("Expected function signature");
        }
    }
    
    #[test]
    fn test_parse_function_definition() {
        let input = "reverse :: ([]) => []";
        let lex_result = tokenize(input).unwrap();
        let mut parser = FlintParser::new(lex_result.tokens);
        
        let program = parser.parse_program().unwrap();
        assert_eq!(program.declarations.len(), 1);
        
        if let Declaration::FunctionDef(func) = &program.declarations[0] {
            assert_eq!(func.name, "reverse");
            assert_eq!(func.clauses.len(), 1);
        } else {
            panic!("Expected function definition");
        }
    }
    
    #[test]
    fn test_parse_c_import() {
        let input = "import C \"stdio.h\" as IO";
        let lex_result = tokenize(input).unwrap();
        let mut parser = FlintParser::new(lex_result.tokens);
        
        let program = parser.parse_program().unwrap();
        assert_eq!(program.declarations.len(), 1);
        
        if let Declaration::CImport(import) = &program.declarations[0] {
            assert_eq!(import.header_file, "stdio.h");
            assert_eq!(import.alias, "IO");
        } else {
            panic!("Expected C import");
        }
    }
}
