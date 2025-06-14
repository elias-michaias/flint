use crate::ast::*;
use crate::lexer::Token;

#[derive(Debug)]
pub struct Parser {
    tokens: Vec<Token>,
    current: usize,
}

#[derive(Debug, thiserror::Error)]
pub enum ParseError {
    #[error("Unexpected token: expected {expected}, found {found:?}")]
    UnexpectedToken { expected: String, found: Token },
    
    #[error("Unexpected end of input")]
    UnexpectedEof,
    
    #[error("Invalid expression")]
    InvalidExpression,
}

impl Parser {
    pub fn new(tokens: Vec<Token>) -> Self {
        Self { tokens, current: 0 }
    }
    
    fn current_token(&self) -> &Token {
        self.tokens.get(self.current).unwrap_or(&Token::Eof)
    }
    
    fn advance(&mut self) -> &Token {
        if self.current < self.tokens.len() {
            self.current += 1;
        }
        self.current_token()
    }
    
    fn expect(&mut self, expected: Token) -> Result<(), ParseError> {
        let current = self.current_token().clone();
        if std::mem::discriminant(&current) == std::mem::discriminant(&expected) {
            self.advance();
            Ok(())
        } else {
            Err(ParseError::UnexpectedToken {
                expected: format!("{:?}", expected),
                found: current,
            })
        }
    }
    
    pub fn parse_program(&mut self) -> Result<Program, ParseError> {
        let mut functions = Vec::new();
        let mut clauses = Vec::new();
        let mut query = None;
        let mut main = None;
        
        // Parse clauses, functions, and queries
        while !matches!(self.current_token(), Token::Eof) {
            match self.current_token() {
                Token::Question => {
                    // Parse query: ?- goal1, goal2.
                    query = Some(self.parse_query()?);
                }
                Token::Identifier(_) => {
                    if self.peek_function_def() {
                        functions.push(self.parse_function()?);
                    } else if self.peek_clause() {
                        clauses.push(self.parse_clause()?);
                    } else {
                        // Main expression
                        main = Some(self.parse_expression()?);
                        break;
                    }
                }
                _ => {
                    // Main expression
                    main = Some(self.parse_expression()?);
                    break;
                }
            }
        }
        
        self.expect(Token::Eof)?;
        
        Ok(Program { functions, clauses, query, main })
    }
    
    fn peek_function_def(&self) -> bool {
        // Look ahead to see if this is a function definition
        // Pattern: identifier ( identifier : type ) : type = expr
        if self.current + 4 < self.tokens.len() {
            matches!(
                (&self.tokens[self.current + 1], &self.tokens[self.current + 3]),
                (Token::LeftParen, Token::Identifier(_))
            )
        } else {
            false
        }
    }
    
    fn peek_clause(&self) -> bool {
        // Look for patterns like: pred(args) :- body. or pred(args).
        for i in self.current..self.tokens.len() {
            match &self.tokens[i] {
                Token::Period => return true,
                Token::ColonDash => return true,
                Token::Equal => return false, // Function definition
                _ => continue,
            }
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
        self.expect(Token::Question)?;
        self.expect(Token::Minus)?; // For ?-
        
        let mut goals = Vec::new();
        goals.push(self.parse_term()?);
        
        while matches!(self.current_token(), Token::Comma) {
            self.advance();
            goals.push(self.parse_term()?);
        }
        
        self.expect(Token::Period)?;
        Ok(Query { goals })
    }
    
    fn parse_clause(&mut self) -> Result<Clause, ParseError> {
        let head_term = self.parse_term()?;
        
        match self.current_token() {
            Token::Period => {
                // Fact
                self.advance();
                match head_term {
                    Term::Compound { functor, args } => {
                        Ok(Clause::Fact { predicate: functor, args })
                    }
                    _ => Err(ParseError::InvalidExpression),
                }
            }
            Token::ColonDash => {
                // Rule
                self.advance();
                let mut body = Vec::new();
                body.push(self.parse_term()?);
                
                while matches!(self.current_token(), Token::Comma) {
                    self.advance();
                    body.push(self.parse_term()?);
                }
                
                self.expect(Token::Period)?;
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
                        Ok(Term::Var(name))
                    } else {
                        Ok(Term::Atom(name))
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
