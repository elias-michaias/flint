use crate::ast::*;
use crate::lexer::{Token, TokenSpan};
use crate::diagnostic::{Diagnostic, DiagnosticBuilder, SourceLocation};

#[derive(Debug)]
pub struct Parser {
    tokens: Vec<TokenSpan>,
    current: usize,
    file_path: String,
    source_text: String,
    debug: bool,
}

#[derive(Debug, thiserror::Error)]
pub enum ParseError {
    #[error("Unexpected token: expected {expected}, found {found:?}")]
    UnexpectedToken { expected: String, found: Token },
    
    #[error("Unexpected end of input")]
    UnexpectedEof,
    
    #[error("Invalid expression")]
    InvalidExpression,
    
    #[error("Invalid syntax: {0}")]
    InvalidSyntax(String),
    
    #[error("Diagnostic error")]
    Diagnostic(Diagnostic),
}

enum TypeDeclaration {
    Predicate(PredicateType),
    Term(TermType),
    BatchTerms(Vec<String>, LogicType),
    TypeDefinition(TypeDefinition),  // New type definition
}

impl Parser {
    pub fn new(tokens: Vec<TokenSpan>, file_path: String, source_text: String) -> Self {
        Self { tokens, current: 0, file_path, source_text, debug: false }
    }
    
    pub fn with_debug(mut self, debug: bool) -> Self {
        self.debug = debug;
        self
    }
    
    fn current_token(&self) -> &Token {
        self.tokens.get(self.current).map(|ts| &ts.token).unwrap_or(&Token::Eof)
    }
    
    fn current_token_span(&self) -> Option<&TokenSpan> {
        self.tokens.get(self.current)
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

    fn peek_token_at(&self, offset: usize) -> Option<&Token> {
        self.tokens.get(self.current + offset).map(|ts| &ts.token)
    }
    
    fn expect(&mut self, expected: Token) -> Result<(), ParseError> {
        let current = self.current_token().clone();
        if std::mem::discriminant(&current) == std::mem::discriminant(&expected) {
            self.advance();
            Ok(())
        } else {
            let current_span = self.current_token_span();
            if let Some(span) = current_span {
                let diagnostic = Diagnostic::error(format!("Expected {:?}, found {:?}", expected, span.token))
                    .with_location(SourceLocation::new(
                        self.file_path.clone(),
                        span.line,
                        span.column,
                        span.length,
                    ))
                    .with_source_text(self.source_text.clone());
                Err(ParseError::Diagnostic(diagnostic))
            } else {
                Err(ParseError::UnexpectedToken {
                    expected: format!("{:?}", expected),
                    found: current,
                })
            }
        }
    }
    
    fn create_error_diagnostic(&self, message: String) -> ParseError {
        let current_span = self.current_token_span();
        if let Some(span) = current_span {
            let diagnostic = Diagnostic::error(message)
                .with_location(SourceLocation::new(
                    self.file_path.clone(),
                    span.line,
                    span.column,
                    span.length,
                ))
                .with_source_text(self.source_text.clone());
            ParseError::Diagnostic(diagnostic)
        } else {
            ParseError::InvalidExpression
        }
    }
    
    pub fn parse_program(&mut self) -> Result<Program, ParseError> {
        let mut type_definitions = Vec::new();
        let mut type_declarations = Vec::new();
        let mut term_types = Vec::new();
        let mut functions = Vec::new();
        let mut clauses = Vec::new();
        let mut queries = Vec::new();
        let mut main = None;
        
        // Parse type declarations, clauses, functions, and queries
        while !matches!(self.current_token(), Token::Eof) {
            match self.current_token() {
                Token::Query => {
                    // Parse query: ?- goal1, goal2.
                    queries.push(self.parse_query()?);
                }
                Token::Bang => {
                    // Persistent fact: !fact(args). or !name :: predicate(args).
                    clauses.push(self.parse_persistent_clause()?);
                }
                Token::Persistent => {
                    // New persistent syntax: persistent name :: fact
                    clauses.push(self.parse_persistent_clause()?);
                }
                Token::Type => {
                    // New type definition: type name [(variants)] [is supertype].
                    let type_decl = self.parse_type_declaration()?;
                    self.expect(Token::Dot)?; // Expect period after type definition
                    if let TypeDeclaration::TypeDefinition(type_def) = type_decl {
                        type_definitions.push(type_def);
                    }
                }
                Token::Identifier(_) => {
                    // Check if this is a new-style type definition (name :: [distinct] type [of supertype].)
                    if self.peek_new_type_definition() {
                        let type_def = self.parse_new_type_definition()?;
                        if let TypeDeclaration::TypeDefinition(td) = type_def {
                            type_definitions.push(td);
                        }
                        self.expect(Token::Dot)?;
                    }
                    // Check if this is an old-style type declaration (name :: type_signature.)
                    else if self.peek_type_declaration() {
                        // Parse the type declaration and decide if it's a predicate or term
                        let type_decl = self.parse_type_declaration()?;
                        match type_decl {
                            TypeDeclaration::Predicate(pred_type) => {
                                type_declarations.push(pred_type);
                            }
                            TypeDeclaration::Term(term_type) => {
                                term_types.push(term_type.clone());
                                
                                // REMOVED: No longer create nullary facts for singleton resources
                                // The new syntax requires all resources to be relational predicates
                            }
                            TypeDeclaration::BatchTerms(names, logic_type) => {
                                // Create individual TermType entries for each name
                                for name in &names {
                                    term_types.push(TermType { name: name.clone(), term_type: logic_type.clone() });
                                }
                                
                                // REMOVED: No longer create nullary facts for singleton resources
                            }
                            TypeDeclaration::TypeDefinition(_) => {
                                // This shouldn't happen since we handle Token::Type separately now
                                unreachable!("TypeDefinition should be handled by Token::Type case")
                            }
                        }
                    } else if self.peek_function_def() {
                        functions.push(self.parse_function()?);
                    } else if self.peek_clause() {
                        clauses.push(self.parse_clause()?);
                    } else {
                        // Main expression
                        main = Some(self.parse_expression()?);
                        break;
                    }
                }
                Token::Eof => {
                    // End of file reached, exit the loop
                    break;
                }
                _ => {
                    // Main expression or unexpected token
                    main = Some(self.parse_expression()?);
                    break;
                }
            }
        }
        
        self.expect(Token::Eof)?;
        
        Ok(Program { 
            type_definitions,
            type_declarations, 
            term_types, 
            functions, 
            clauses, 
            queries, 
            main 
        })
    }
    
    fn peek_function_def(&self) -> bool {
        // Look ahead to see if this is a function definition
        // Pattern: identifier ( identifier : type ) : type = expr
        if self.current + 4 < self.tokens.len() {
            matches!(
                (&self.tokens[self.current + 1].token, &self.tokens[self.current + 3].token),
                (Token::LeftParen, Token::Identifier(_))
            )
        } else {
            false
        }
    }
    
    fn peek_clause(&self) -> bool {
        // Look for patterns like: 
        // - pred(args) :- body.
        // - pred(args).
        // - !pred(args).
        // - name :: pred(args).  (named facts)
        
        // Check for named fact pattern: identifier :: identifier(args).
        if matches!(self.current_token(), Token::Identifier(_)) {
            if let Some(Token::ColonColon) = self.peek_token_at(1) {
                // Look ahead to see if this is a named fact
                let mut i = 2;
                while let Some(token) = self.peek_token_at(i) {
                    match token {
                        Token::Identifier(_) => {
                            // Check if followed by left paren (predicate call)
                            if let Some(Token::LeftParen) = self.peek_token_at(i + 1) {
                                return true; // This is a named fact: name :: pred(args)
                            }
                            i += 1;
                        }
                        Token::Dot => return false, // name :: type.
                        Token::Arrow => return false, // name :: type -> type.
                        Token::Type => return false, // name :: type
                        _ => i += 1,
                    }
                }
            }
        }
        
        // Check for other clause patterns
        for i in self.current..self.tokens.len() {
            match &self.tokens[i].token {
                Token::Dot => return true,
                Token::Rule => return true,
                Token::Equal => return false, // Function definition
                _ => continue,
            }
        }
        false
    }
    
    fn peek_type_declaration(&self) -> bool {
        // Look for pattern: identifier [, identifier]* :: type_signature.
        // But exclude fact declarations which have: identifier :: predicate_call.
        // Type signatures typically contain 'type', '->', or basic type keywords
        // Predicate calls contain parentheses and concrete arguments
        if !matches!(self.current_token(), Token::Identifier(_)) {
            return false;
        }
        
        // Check if we can find :: after potential comma-separated identifiers
        let mut i = 1;
        loop {
            match self.peek_token_at(i) {
                Some(Token::ColonColon) => {
                    // Found ::, now check what comes after to distinguish type decl from fact decl
                    let mut j = i + 1;
                    while j < 20 { // reasonable lookahead limit
                        match self.peek_token_at(j) {
                            Some(Token::FatArrow) => {
                                // Found =>, this is a Celf-style rule, not a type declaration
                                return false;
                            }
                            Some(Token::Type) | Some(Token::Arrow) => {
                                // Found type keyword or arrow, this is a type declaration
                                return true;
                            }
                            Some(Token::LeftParen) => {
                                // Found left paren, this is likely a predicate call (fact), not type decl
                                return false;
                            }
                            Some(Token::Identifier(name)) => {
                                // Check if this identifier is followed by parentheses (predicate call)
                                // or if it's a type name
                                if let Some(Token::LeftParen) = self.peek_token_at(j + 1) {
                                    // predicate_name(args) - this is a fact declaration
                                    return false;
                                }
                                // Could be a type name, continue looking
                                j += 1;
                            }
                            Some(Token::Dot) => {
                                // Found ., assume this is a type declaration if we haven't found
                                // clear evidence of a predicate call
                                return true;
                            }
                            Some(Token::Eof) => {
                                return false;
                            }
                            _ => {
                                j += 1;
                            }
                        }
                    }
                    // If we didn't find clear evidence either way within reasonable distance,
                    // be conservative and assume it's not a type declaration
                    return false;
                }
                Some(Token::Comma) => {
                    // Check if next token after comma is identifier
                    if let Some(Token::Identifier(_)) = self.peek_token_at(i + 1) {
                        i += 2; // Skip comma and identifier
                        continue;
                    } else {
                        return false;
                    }
                }
                _ => return false,
            }
        }
    }

    fn peek_predicate_type(&self) -> bool {
        // Look ahead to see if this is a predicate type (contains arrow)
        // We need to look past the identifier :: part
        let mut i = 2; // Skip identifier and ::
        while let Some(token) = self.peek_token_at(i) {
            match token {
                Token::Arrow => return true,
                Token::Dot => return false,
                _ => i += 1,
            }
        }
        false
    }

    fn peek_term_type(&self) -> bool {
        // Look for pattern: name : type.
        // Must be: Identifier followed by Colon, then some type, then Dot
        if self.current + 3 < self.tokens.len() {
            matches!((&self.tokens[self.current].token, &self.tokens[self.current + 1].token), 
                     (Token::Identifier(_), Token::Colon)) &&
            // Look ahead to make sure it ends with dot and not other clause patterns
            self.tokens[self.current + 2..].iter().take_while(|t| !matches!(t.token, Token::Dot | Token::Eof)).any(|_| true) &&
            self.tokens[self.current + 2..].iter().find(|t| matches!(t.token, Token::Dot | Token::Rule | Token::LeftParen)).map_or(false, |t| matches!(t.token, Token::Dot))
        } else {
            false
        }
    }
    
    fn peek_new_type_definition(&self) -> bool {
        // Look for pattern: identifier :: [distinct] type [of supertype] .
        if !matches!(self.current_token(), Token::Identifier(_)) {
            return false;
        }
        
        // Check for :: after identifier
        if !matches!(self.peek_token_at(1), Some(Token::ColonColon)) {
            return false;
        }
        
        // Look for type keyword (possibly after distinct)
        let mut i = 2; // Start after ::
        
        // Skip optional 'distinct' keyword
        if matches!(self.peek_token_at(i), Some(Token::Distinct)) {
            i += 1;
        }
        
        // Look for 'type' keyword
        if matches!(self.peek_token_at(i), Some(Token::Type)) {
            return true;
        }
        
        false
    }

    fn parse_function(&mut self) -> Result<Function, ParseError> {
        // Parse: name(param: param_type): return_type = body
        let name = match self.current_token() {
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();
                name
            }
            _ => return Err(ParseError::InvalidExpression),
        };
        
        self.expect(Token::LeftParen)?;
        
        let param = match self.current_token() {
            Token::Identifier(param) => {
                let param = param.clone();
                self.advance();
                param
            }
            _ => return Err(ParseError::InvalidExpression),
        };
        
        // Skip type annotations for now (simplified)
        self.expect(Token::RightParen)?;
        self.expect(Token::Equal)?;
        
        let body = self.parse_expression()?;
        
        Ok(Function {
            name,
            param,
            param_type: Type::Int, // Simplified
            return_type: Type::Int, // Simplified
            body,
        })
    }
    
    pub fn parse_expression(&mut self) -> Result<Expr, ParseError> {
        self.parse_let()
    }
    
    fn parse_let(&mut self) -> Result<Expr, ParseError> {
        if matches!(self.current_token(), Token::Let) {
            self.advance(); // consume 'let'
            
            let var = match self.current_token() {
                Token::Identifier(name) => {
                    let name = name.clone();
                    self.advance();
                    name
                }
                _ => return Err(ParseError::InvalidExpression),
            };
            
            self.expect(Token::Equal)?;
            let value = Box::new(self.parse_expression()?);
            self.expect(Token::In)?;
            let body = Box::new(self.parse_expression()?);
            
            Ok(Expr::Let { var, value, body })
        } else {
            self.parse_if()
        }
    }
    
    fn parse_if(&mut self) -> Result<Expr, ParseError> {
        if matches!(self.current_token(), Token::If) {
            self.advance(); // consume 'if'
            
            let cond = Box::new(self.parse_expression()?);
            self.expect(Token::Then)?;
            let then_branch = Box::new(self.parse_expression()?);
            self.expect(Token::Else)?;
            let else_branch = Box::new(self.parse_expression()?);
            
            Ok(Expr::If {
                cond,
                then_branch,
                else_branch,
            })
        } else {
            self.parse_lambda()
        }
    }
    
    fn parse_lambda(&mut self) -> Result<Expr, ParseError> {
        if matches!(self.current_token(), Token::Lam) {
            self.advance(); // consume 'lam'
            
            let param = match self.current_token() {
                Token::Identifier(name) => {
                    let name = name.clone();
                    self.advance();
                    name
                }
                _ => return Err(ParseError::InvalidExpression),
            };
            
            self.expect(Token::Dot)?;
            let body = Box::new(self.parse_expression()?);
            
            Ok(Expr::Lambda { param, body })
        } else {
            self.parse_binary()
        }
    }
    
    fn parse_binary(&mut self) -> Result<Expr, ParseError> {
        self.parse_comparison()
    }
    
    fn parse_comparison(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.parse_addition()?;
        
        while matches!(
            self.current_token(),
            Token::EqualEqual | Token::Less | Token::Greater
        ) {
            let op = match self.current_token() {
                Token::EqualEqual => BinOpKind::Eq,
                Token::Less => BinOpKind::Lt,
                Token::Greater => BinOpKind::Gt,
                _ => unreachable!(),
            };
            self.advance();
            let right = self.parse_addition()?;
            expr = Expr::BinOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
            };
        }
        
        Ok(expr)
    }
    
    fn parse_addition(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.parse_multiplication()?;
        
        while matches!(self.current_token(), Token::Plus | Token::Minus) {
            let op = match self.current_token() {
                Token::Plus => BinOpKind::Add,
                Token::Minus => BinOpKind::Sub,
                _ => unreachable!(),
            };
            self.advance();
            let right = self.parse_multiplication()?;
            expr = Expr::BinOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
            };
        }
        
        Ok(expr)
    }
    
    fn parse_multiplication(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.parse_primary()?;
        
        while matches!(self.current_token(), Token::Multiply | Token::Divide) {
            let op = match self.current_token() {
                Token::Multiply => BinOpKind::Mul,
                Token::Divide => BinOpKind::Div,
                _ => unreachable!(),
            };
            self.advance();
            let right = self.parse_primary()?;
            expr = Expr::BinOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
            };
        }
        
        Ok(expr)
    }
    
    fn parse_primary(&mut self) -> Result<Expr, ParseError> {
        match self.current_token() {
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
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();
                
                // Check for function application
                if matches!(self.current_token(), Token::LeftParen) {
                    self.advance(); // consume '('
                    let mut args = Vec::new();
                    
                    if !matches!(self.current_token(), Token::RightParen) {
                        args.push(self.parse_expression()?);
                        
                        while matches!(self.current_token(), Token::Comma) {
                            self.advance(); // consume ','
                            args.push(self.parse_expression()?);
                        }
                    }
                    
                    self.expect(Token::RightParen)?;
                    Ok(Expr::App { func: name, args })
                } else {
                    Ok(Expr::Var(name))
                }
            }
            Token::LeftParen => {
                self.advance(); // consume '('
                let expr = self.parse_expression()?;
                self.expect(Token::RightParen)?;
                Ok(expr)
            }
            Token::Alloc => {
                self.advance(); // consume 'alloc'
                self.expect(Token::LeftParen)?;
                let size = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                Ok(Expr::Alloc { size })
            }
            Token::Free => {
                self.advance(); // consume 'free'
                self.expect(Token::LeftParen)?;
                let ptr = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                Ok(Expr::Free { ptr })
            }
            Token::Load => {
                self.advance(); // consume 'load'
                self.expect(Token::LeftParen)?;
                let ptr = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                Ok(Expr::Load { ptr })
            }
            Token::Store => {
                self.advance(); // consume 'store'
                self.expect(Token::LeftParen)?;
                let ptr = Box::new(self.parse_expression()?);
                self.expect(Token::Comma)?;
                let value = Box::new(self.parse_expression()?);
                self.expect(Token::RightParen)?;
                Ok(Expr::Store { ptr, value })
            }
            _ => Err(ParseError::InvalidExpression),
        }
    }
    
    fn parse_query(&mut self) -> Result<Query, ParseError> {
        self.expect(Token::Query)?; // For ?-
        
        let mut goals = Vec::new();
        let mut is_disjunctive = None; // None means not yet determined
        goals.push(self.parse_term()?);
        
        // Check for connectives and ensure consistency
        while matches!(self.current_token(), Token::Comma | Token::Ampersand | Token::Pipe) {
            let new_connective = match self.current_token() {
                Token::Pipe => true,  // disjunctive
                Token::Ampersand | Token::Comma => false,  // conjunctive
                _ => unreachable!(),
            };
            
            // Set the query type on first connective, or check consistency
            match is_disjunctive {
                None => is_disjunctive = Some(new_connective),
                Some(current) => {
                    if current != new_connective {
                        return Err(ParseError::InvalidSyntax("Cannot mix conjunctive (&, ,) and disjunctive (|) operators in the same query".to_string()));
                    }
                }
            }
            
            self.advance();
            goals.push(self.parse_term()?);
        }
        
        self.expect(Token::Dot)?;
        Ok(Query { 
            goals, 
            is_disjunctive: is_disjunctive.unwrap_or(false) // Default to conjunctive for single goals
        })
    }
    
    fn parse_persistent_clause(&mut self) -> Result<Clause, ParseError> {
        // Handle both old and new persistent syntax
        let is_new_syntax = matches!(self.current_token(), Token::Persistent);
        
        if is_new_syntax {
            // New syntax: persistent name :: predicate(args).
            self.expect(Token::Persistent)?;
            
            // Parse the name
            let name = if let Token::Identifier(name) = self.current_token() {
                let name = name.clone();
                self.advance();
                Some(name)
            } else {
                return Err(self.create_error_diagnostic("Expected name after 'persistent'".to_string()));
            };
            
            self.expect(Token::ColonColon)?;
            
            // Parse the predicate term
            let predicate_term = self.parse_term()?;
            self.expect(Token::Dot)?;
            
            match predicate_term {
                Term::Compound { functor, args } => {
                    Ok(Clause::Fact { predicate: functor, args, persistent: true, name })
                }
                Term::Atom { name: pred_name, .. } => {
                    Ok(Clause::Fact { predicate: pred_name, args: vec![], persistent: true, name })
                }
                _ => Err(ParseError::InvalidExpression),
            }
        } else {
            // Old syntax: !fact or !name :: fact
            self.expect(Token::Bang)?;
            
            // Parse the identifier (either predicate name or fact name)
            let first_identifier = if let Token::Identifier(name) = self.current_token() {
                let name = name.clone();
                self.advance();
                name
            } else {
                return Err(self.create_error_diagnostic("Expected identifier after '!'".to_string()));
            };
            
            // Check if this is a typed persistent fact (name :: predicate(args)) or simple persistent fact
            if matches!(self.current_token(), Token::ColonColon) {
                // This is !name :: predicate(args).
                self.advance(); // consume ::
                
                // Parse the predicate term
                let predicate_term = self.parse_term()?;
                self.expect(Token::Dot)?;
                
                match predicate_term {
                    Term::Compound { functor, args } => {
                        Ok(Clause::Fact { predicate: functor, args, persistent: true, name: None })
                    }
                    _ => Err(ParseError::InvalidExpression),
                }
            } else if matches!(self.current_token(), Token::LeftParen) {
                // This is !predicate(args).
                self.advance(); // consume (
                let mut args = Vec::new();
                
                if !matches!(self.current_token(), Token::RightParen) {
                    args.push(self.parse_term()?);
                    
                    while matches!(self.current_token(), Token::Comma) {
                        self.advance();
                        args.push(self.parse_term()?);
                    }
                }
                
                self.expect(Token::RightParen)?;
                self.expect(Token::Dot)?;
                Ok(Clause::Fact { predicate: first_identifier, args, persistent: true, name: None })
            } else {
                // This is a simple !fact.
                self.expect(Token::Dot)?;
                Ok(Clause::Fact { predicate: first_identifier, args: vec![], persistent: true, name: None })
            }
        }
    }

    fn parse_clause(&mut self) -> Result<Clause, ParseError> {
        // Check for persistent fact prefix
        let persistent = if matches!(self.current_token(), Token::Bang) {
            self.advance();
            true
        } else {
            false
        };
        
        // Parse the first identifier/term
        let first_term = self.parse_term()?;
        
        match self.current_token() {
            Token::Dot => {
                // Simple fact: predicate(args).
                self.advance();
                match first_term {
                    Term::Compound { functor, args } => {
                        Ok(Clause::Fact { predicate: functor, args, persistent, name: None })
                    }
                    Term::Atom { name, .. } => {
                        Ok(Clause::Fact { predicate: name, args: vec![], persistent, name: None })
                    }
                    _ => Err(ParseError::InvalidExpression),
                }
            }
            Token::ColonColon => {
                // Could be either:
                // 1. Celf-style rule: goal_name :: condition => result
                // 2. Named fact: name :: predicate(args).
                
                if persistent {
                    return Err(ParseError::InvalidExpression); // Can't have !rule
                }
                
                // The first_term should be the name
                let name = match first_term {
                    Term::Atom { name, .. } => name,
                    _ => return Err(ParseError::InvalidExpression),
                };
                
                self.advance(); // consume ::
                
                // Parse what comes after ::
                let second_term = self.parse_term()?;
                
                // Check if this is followed by => (Celf-style rule) or . (named fact)
                match self.current_token() {
                    Token::FatArrow => {
                        // Celf-style rule: goal_name :: condition => result
                        let mut body = vec![second_term];
                        
                        // Handle conjunction with & (instead of comma)
                        while matches!(self.current_token(), Token::Ampersand) {
                            self.advance();
                            body.push(self.parse_term()?);
                        }
                        
                        // Expect => for the result
                        self.expect(Token::FatArrow)?;
                        
                        // Parse the result/head
                        let produces = Some(self.parse_term()?);
                        
                        self.expect(Token::Dot)?;
                        
                        // Create the goal name as the head term
                        let head = Term::Atom { name, type_name: None };
                        Ok(Clause::Rule { head, body, produces })
                    }
                    Token::Dot => {
                        // Named fact: name :: predicate(args).
                        self.advance(); // consume .
                        match second_term {
                            Term::Compound { functor, args } => {
                                Ok(Clause::Fact { predicate: functor, args, persistent: false, name: Some(name) })
                            }
                            Term::Atom { name: pred_name, .. } => {
                                Ok(Clause::Fact { predicate: pred_name, args: vec![], persistent: false, name: Some(name) })
                            }
                            _ => Err(ParseError::InvalidExpression),
                        }
                    }
                    Token::Ampersand => {
                        // Celf-style rule with conjunction: goal_name :: condition & condition => result
                        let mut body = vec![second_term];
                        
                        // Handle conjunction with &
                        while matches!(self.current_token(), Token::Ampersand) {
                            self.advance();
                            body.push(self.parse_term()?);
                        }
                        
                        // Expect => for the result
                        self.expect(Token::FatArrow)?;
                        
                        // Parse the result/head
                        let produces = Some(self.parse_term()?);
                        
                        self.expect(Token::Dot)?;
                        
                        // Create the goal name as the head term
                        let head = Term::Atom { name, type_name: None };
                        Ok(Clause::Rule { head, body, produces })
                    }
                    _ => Err(ParseError::InvalidExpression),
                }
            }
            Token::Rule => {
                // Old Prolog-style rule: head :- body (for backward compatibility)
                if persistent {
                    return Err(ParseError::InvalidExpression); // Can't have !rule
                }
                self.advance();
                let mut body = Vec::new();
                let produces = None;
                
                // Parse the body goals
                body.push(self.parse_term()?);
                
                while matches!(self.current_token(), Token::Comma) {
                    self.advance();
                    body.push(self.parse_term()?);
                }
                
                self.expect(Token::Dot)?;
                Ok(Clause::Rule { head: first_term, body, produces })
            }
            _ => Err(ParseError::InvalidExpression),
        }
    }
    
    fn parse_term(&mut self) -> Result<Term, ParseError> {
        match self.current_token() {
            Token::Bang => {
                // Clone operator: !term
                self.advance();
                let inner_term = self.parse_term()?;
                Ok(Term::Clone(Box::new(inner_term)))
            }
            Token::Variable(name) => {
                // Variable: $name
                let name = name.clone();
                self.advance();
                Ok(Term::Var { name, type_name: None })
            }
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();
                
                if matches!(self.current_token(), Token::LeftParen) {
                    // Compound term
                    self.advance();
                    let mut args = Vec::new();
                    
                    if !matches!(self.current_token(), Token::RightParen) {
                        args.push(self.parse_term()?);
                        
                        while matches!(self.current_token(), Token::Comma) {
                            self.advance();
                            args.push(self.parse_term()?);
                        }
                    }
                    
                    self.expect(Token::RightParen)?;
                    Ok(Term::Compound { functor: name, args })
                } else {
                    // Simple atom/identifier
                    Ok(Term::Atom { 
                        name, 
                        type_name: None 
                    })
                }
            }
            Token::Integer(n) => {
                let n = *n;
                self.advance();
                Ok(Term::Integer(n))
            }
            _ => Err(ParseError::InvalidExpression),
        }
    }
    
    fn parse_logic_type(&mut self) -> Result<LogicType, ParseError> {
        // Parse the first type
        let mut current_type = self.parse_base_type()?;
        
        // If we see arrows, parse as function type
        let mut types = vec![current_type];
        while matches!(self.current_token(), Token::Arrow) {
            self.advance(); // consume ->
            types.push(self.parse_base_type()?);
        }
        
        if types.len() == 1 {
            Ok(types.into_iter().next().unwrap())
        } else {
            Ok(LogicType::Arrow(types))
        }
    }
    
    fn parse_base_type(&mut self) -> Result<LogicType, ParseError> {
        match self.current_token() {
            Token::Identifier(name) => {
                let type_name = name.clone();
                self.advance();
                match type_name.as_str() {
                    "int" => Ok(LogicType::Integer),
                    "string" => Ok(LogicType::String),
                    "atom" => Ok(LogicType::Atom),
                    _ => Ok(LogicType::Named(type_name)),
                }
            }
            Token::Type => {
                self.advance();
                Ok(LogicType::Type)
            }
            _ => Err(ParseError::InvalidExpression),
        }
    }
    
    fn parse_predicate_type(&mut self) -> Result<PredicateType, ParseError> {
        // Parse predicate name
        let name = match self.current_token() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                name
            }
            _ => return Err(ParseError::InvalidExpression),
        };
        
        self.expect(Token::ColonColon)?;
        
        // Parse the signature (e.g., person -> person -> type)
        let signature = self.parse_logic_type()?;
        
        self.expect(Token::Dot)?;
        
        Ok(PredicateType { name, signature })
    }
    
    fn parse_term_type(&mut self) -> Result<TermType, ParseError> {
        let name = match self.current_token() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                name
            }
            _ => return Err(ParseError::InvalidExpression),
        };
        
        self.expect(Token::ColonColon)?; // Use ColonColon for consistency
        
        let term_type = self.parse_logic_type()?;
        
        self.expect(Token::Dot)?;
        
        Ok(TermType { name, term_type })
    }

    fn parse_type_declaration(&mut self) -> Result<TypeDeclaration, ParseError> {
        // Check if this is the new syntax: "type name ..." 
        if matches!(self.current_token(), Token::Type) {
            return self.parse_old_style_type_definition();
        }
        
        // Otherwise, parse the old syntax: "name(s) :: type"
        self.parse_old_type_declaration()
    }
    
    fn parse_old_style_type_definition(&mut self) -> Result<TypeDeclaration, ParseError> {
        // Parse: type name [(variant1 | variant2)]
        self.expect(Token::Type)?;
        
        let name = match self.current_token() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                name
            }
            _ => return Err(self.create_error_diagnostic("Expected type name after 'type'".to_string())),
        };
        
        // Check for union variants: (apple | orange) or complex nested unions
        let union_variants = if matches!(self.current_token(), Token::LeftParen) {
            Some(self.parse_union_variants()?)
        } else {
            None
        };
        
        Ok(TypeDeclaration::TypeDefinition(TypeDefinition {
            name,
            union_variants,
            supertype: None, // Old syntax doesn't support subtyping
            distinct: false, // Old syntax doesn't support distinct
        }))
    }
    
    fn parse_new_type_definition(&mut self) -> Result<TypeDeclaration, ParseError> {
        // Parse: name :: [distinct] type [of supertype]
        
        // Parse the type name
        let name = match self.current_token() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                name
            }
            _ => return Err(self.create_error_diagnostic("Expected type name".to_string())),
        };
        
        self.expect(Token::ColonColon)?;
        
        // Check for optional 'distinct' keyword
        let distinct = if matches!(self.current_token(), Token::Distinct) {
            self.advance(); // consume 'distinct'
            true
        } else {
            false
        };
        
        // Expect 'type' keyword
        self.expect(Token::Type)?;
        
        // Check for optional 'of supertype' clause
        let supertype = if matches!(self.current_token(), Token::Of) {
            self.advance(); // consume 'of'
            Some(self.parse_logic_type()?)
        } else {
            None
        };
        
        Ok(TypeDeclaration::TypeDefinition(TypeDefinition {
            name,
            union_variants: None, // New syntax doesn't support union variants directly
            supertype,
            distinct,
        }))
    }
    
    fn parse_union_variants(&mut self) -> Result<UnionVariants, ParseError> {
        self.expect(Token::LeftParen)?;
        
        let mut variants = Vec::new();
        
        // Parse first variant
        let first_variant = self.parse_union_variant()?;
        variants.push(first_variant);
        
        // Parse additional variants
        while matches!(self.current_token(), Token::Pipe) {
            self.advance(); // consume |
            let variant = self.parse_union_variant()?;
            variants.push(variant);
        }
        
        self.expect(Token::RightParen)?;
        
        // Check if this is a simple list or if any have sub-variants
        let has_nested = variants.iter().any(|v| v.sub_variants.is_some());
        
        if has_nested {
            Ok(UnionVariants::Nested(variants))
        } else {
            // Convert to simple list
            let simple_names = variants.into_iter().map(|v| v.name).collect();
            Ok(UnionVariants::Simple(simple_names))
        }
    }
    
    fn parse_union_variant(&mut self) -> Result<UnionVariant, ParseError> {
        let name = match self.current_token() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                name
            }
            _ => return Err(self.create_error_diagnostic("Expected variant name".to_string())),
        };
        
        // Check for nested union: fruit (apple | orange)
        let sub_variants = if matches!(self.current_token(), Token::LeftParen) {
            Some(self.parse_union_variants()?)
        } else {
            None
        };
        
        Ok(UnionVariant {
            name,
            sub_variants,
        })
    }
    
    fn parse_old_type_declaration(&mut self) -> Result<TypeDeclaration, ParseError> {
        // Parse name(s) :: type_signature.
        // This can be either "name :: type" or "name1, name2, name3 :: type"
        let mut names = Vec::new();
        
        // Parse the first name
        let first_name = match self.current_token() {
            Token::Identifier(n) => {
                let name = n.clone();
                self.advance();
                name
            }
            _ => return Err(self.create_error_diagnostic("Expected identifier for type declaration".to_string())),
        };
        names.push(first_name);
        
        // Parse additional names separated by commas
        while matches!(self.current_token(), Token::Comma) {
            self.advance(); // consume comma
            
            let name = match self.current_token() {
                Token::Identifier(n) => {
                    let name = n.clone();
                    self.advance();
                    name
                }
                _ => return Err(self.create_error_diagnostic("Expected identifier after comma in type declaration".to_string())),
            };
            names.push(name);
        }
        
        self.expect(Token::ColonColon)?;
        
        let signature = self.parse_logic_type()?;
        
        self.expect(Token::Dot)?;
        
        // Decide if this is a predicate or term type based on the signature
        match &signature {
            LogicType::Arrow(_) => {
                // Has arrows, so it's a predicate type - only single names allowed
                if names.len() > 1 {
                    return Err(self.create_error_diagnostic("Cannot batch assign predicate types - predicates must be declared individually".to_string()));
                }
                Ok(TypeDeclaration::Predicate(PredicateType { name: names[0].clone(), signature }))
            }
            _ => {
                // No arrows, so it's a term type - batch assignment allowed
                if names.len() == 1 {
                    Ok(TypeDeclaration::Term(TermType { name: names[0].clone(), term_type: signature }))
                } else {
                    if self.debug {
                        eprintln!("DEBUG: Creating BatchTerms for {} names", names.len());
                    }
                    Ok(TypeDeclaration::BatchTerms(names, signature))
                }
            }
        }
    }

    /// Create a diagnostic for the current parsing context
    fn create_diagnostic(&self, message: String, file_path: &str, source_text: &str) -> Diagnostic {
        let builder = DiagnosticBuilder::new(file_path.to_string(), source_text.to_string());
        
        // For now, we'll use a simple line/column estimate
        // In a real implementation, we'd track exact positions during tokenization
        let line = 1; // Placeholder
        let column = 1; // Placeholder
        let length = 1; // Placeholder
        
        builder.error_at(message, line, column, length)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::tokenize;
    
    #[test]
    fn test_parse_simple_let() {
        let tokens = tokenize("let x = 42 in x + 1").unwrap();
        let mut parser = Parser::new(tokens);
        let program = parser.parse_program().unwrap();
        
        match program.main {
            Expr::Let { var, value, body } => {
                assert_eq!(var, "x");
                assert_eq!(*value, Expr::Int(42));
                match *body {
                    Expr::BinOp { op, left, right } => {
                        assert_eq!(op, BinOpKind::Add);
                        assert_eq!(*left, Expr::Var("x".to_string()));
                        assert_eq!(*right, Expr::Int(1));
                    }
                    _ => panic!("Expected binary operation"),
                }
            }
            _ => panic!("Expected let expression"),
        }
    }
    
    #[test]
    fn test_parse_lambda() {
        let tokens = tokenize("lam x. x + 1").unwrap();
        let mut parser = Parser::new(tokens);
        let program = parser.parse_program().unwrap();
        
        match program.main {
            Expr::Lambda { param, body } => {
                assert_eq!(param, "x");
                match *body {
                    Expr::BinOp { op, left, right } => {
                        assert_eq!(op, BinOpKind::Add);
                        assert_eq!(*left, Expr::Var("x".to_string()));
                        assert_eq!(*right, Expr::Int(1));
                    }
                    _ => panic!("Expected binary operation"),
                }
            }
            _ => panic!("Expected lambda expression"),
        }
    }
}
