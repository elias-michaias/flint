use crate::ast::*;
use crate::lexer::{Token, TokenSpan};
use crate::diagnostic::{Diagnostic, DiagnosticBuilder, SourceLocation};

#[derive(Debug)]
pub struct Parser {
    tokens: Vec<TokenSpan>,
    current: usize,
    file_path: String,
    source_text: String,
}

#[derive(Debug, thiserror::Error)]
pub enum ParseError {
    #[error("Unexpected token: expected {expected}, found {found:?}")]
    UnexpectedToken { expected: String, found: Token },
    
    #[error("Unexpected end of input")]
    UnexpectedEof,
    
    #[error("Invalid expression")]
    InvalidExpression,
    
    #[error("Diagnostic error")]
    Diagnostic(Diagnostic),
}

enum TypeDeclaration {
    Predicate(PredicateType),
    Term(TermType),
    BatchTerms(Vec<String>, LogicType),
}

impl Parser {
    pub fn new(tokens: Vec<TokenSpan>, file_path: String, source_text: String) -> Self {
        Self { tokens, current: 0, file_path, source_text }
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
        let mut type_declarations = Vec::new();
        let mut term_types = Vec::new();
        let mut functions = Vec::new();
        let mut clauses = Vec::new();
        let mut query = None;
        let mut main = None;
        
        // Parse type declarations, clauses, functions, and queries
        while !matches!(self.current_token(), Token::Eof) {
            match self.current_token() {
                Token::Query => {
                    // Parse query: ?- goal1, goal2.
                    query = Some(self.parse_query()?);
                }
                Token::Identifier(_) => {
                    // Check if this is a type declaration (name :: type_signature.)
                    if self.peek_type_declaration() {
                        // Parse the type declaration and decide if it's a predicate or term
                        let type_decl = self.parse_type_declaration()?;
                        match type_decl {
                            TypeDeclaration::Predicate(pred_type) => {
                                type_declarations.push(pred_type);
                            }
                            TypeDeclaration::Term(term_type) => {
                                term_types.push(term_type);
                            }
                            TypeDeclaration::BatchTerms(names, logic_type) => {
                                // Create individual TermType entries for each name
                                for name in names {
                                    term_types.push(TermType { name, term_type: logic_type.clone() });
                                }
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
            type_declarations, 
            term_types, 
            functions, 
            clauses, 
            query, 
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
        // Look for patterns like: pred(args) :- body. or pred(args).
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
        // Look for pattern: identifier [, identifier]* :: ...
        if !matches!(self.current_token(), Token::Identifier(_)) {
            return false;
        }
        
        // Check if we can find :: after potential comma-separated identifiers
        let mut i = 1;
        loop {
            match self.peek_token_at(i) {
                Some(Token::ColonColon) => return true,
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
        goals.push(self.parse_term()?);
        
        while matches!(self.current_token(), Token::Comma) {
            self.advance();
            goals.push(self.parse_term()?);
        }
        
        self.expect(Token::Dot)?;
        Ok(Query { goals })
    }
    
    fn parse_clause(&mut self) -> Result<Clause, ParseError> {
        let head_term = self.parse_term()?;
        
        match self.current_token() {
            Token::Dot => {
                // Fact
                self.advance();
                match head_term {
                    Term::Compound { functor, args } => {
                        Ok(Clause::Fact { predicate: functor, args })
                    }
                    _ => Err(ParseError::InvalidExpression),
                }
            }
            Token::Rule => {
                // Rule
                self.advance();
                let mut body = Vec::new();
                body.push(self.parse_term()?);
                
                while matches!(self.current_token(), Token::Comma) {
                    self.advance();
                    body.push(self.parse_term()?);
                }
                
                self.expect(Token::Dot)?;
                Ok(Clause::Rule { head: head_term, body })
            }
            _ => Err(ParseError::InvalidExpression),
        }
    }
    
    fn parse_term(&mut self) -> Result<Term, ParseError> {
        match self.current_token() {
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
                    // Check if it's a variable (starts with uppercase) or atom
                    if name.chars().next().unwrap().is_uppercase() {
                        Ok(Term::Var { 
                            name, 
                            type_name: None 
                        })
                    } else {
                        Ok(Term::Atom { 
                            name, 
                            type_name: None 
                        })
                    }
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
                    eprintln!("DEBUG: Creating BatchTerms for {} names", names.len());
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
