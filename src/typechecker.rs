use crate::ast::*;
use std::collections::HashMap;

#[derive(Debug, thiserror::Error)]
pub enum TypeError {
    #[error("Variable '{0}' not found")]
    VariableNotFound(String),
    
    #[error("Variable '{0}' used more than once")]
    LinearityViolation(String),
    
    #[error("Variable '{0}' not used")]
    UnusedVariable(String),
    
    #[error("Type mismatch: expected {expected:?}, found {found:?}")]
    TypeMismatch { expected: Type, found: Type },
    
    #[error("Function '{0}' not found")]
    FunctionNotFound(String),
    
    #[error("Wrong number of arguments: expected {expected}, found {found}")]
    WrongArgumentCount { expected: usize, found: usize },
    
    #[error("Cannot apply non-function type: {0:?}")]
    NotAFunction(Type),
    
    #[error("Invalid binary operation: {op:?} on {left:?} and {right:?}")]
    InvalidBinaryOp { op: BinOpKind, left: Type, right: Type },
}

pub struct TypeChecker {
    functions: HashMap<String, (Type, Type)>, // (param_type, return_type)
}

impl TypeChecker {
    pub fn new() -> Self {
        Self {
            functions: HashMap::new(),
        }
    }
    
    pub fn check_program(&mut self, program: &Program) -> Result<Type, TypeError> {
        // First pass: collect function signatures
        for func in &program.functions {
            self.functions.insert(
                func.name.clone(),
                (func.param_type.clone(), func.return_type.clone()),
            );
        }
        
        // Second pass: check function bodies
        for func in &program.functions {
            let mut env = TypeEnv::new();
            env.bind(func.param.clone(), func.param_type.clone());
            
            let body_type = self.check_expr(&func.body, &mut env)?;
            if body_type != func.return_type {
                return Err(TypeError::TypeMismatch {
                    expected: func.return_type.clone(),
                    found: body_type,
                });
            }
            
            // Check that all linear variables are used
            env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
        }
        
        // Check main expression if it exists
        if let Some(ref main_expr) = program.main {
            let mut main_env = TypeEnv::new();
            let main_type = self.check_expr(main_expr, &mut main_env)?;
            main_env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
            Ok(main_type)
        } else {
            // If no main expression, return Unit type
            Ok(Type::Unit)
        }
    }
    
    fn check_expr(&self, expr: &Expr, env: &mut TypeEnv) -> Result<Type, TypeError> {
        match expr {
            Expr::Var(name) => {
                env.use_var(name).map_err(|_| TypeError::LinearityViolation(name.clone()))?;
                env.lookup(name)
                    .cloned()
                    .ok_or_else(|| TypeError::VariableNotFound(name.clone()))
            }
            
            Expr::Int(_) => Ok(Type::Int),
            
            Expr::Str(_) => Ok(Type::Str),
            
            Expr::App { func, args } => {
                let (param_type, return_type) = self.functions
                    .get(func)
                    .cloned()
                    .ok_or_else(|| TypeError::FunctionNotFound(func.clone()))?;
                
                if args.len() != 1 {
                    return Err(TypeError::WrongArgumentCount {
                        expected: 1,
                        found: args.len(),
                    });
                }
                
                let arg_type = self.check_expr(&args[0], env)?;
                if arg_type != param_type {
                    return Err(TypeError::TypeMismatch {
                        expected: param_type,
                        found: arg_type,
                    });
                }
                
                Ok(return_type)
            }
            
            Expr::Lambda { param, body } => {
                // For now, assume Int -> Int (simplified)
                let mut lambda_env = env.clone();
                lambda_env.bind(param.clone(), Type::Int);
                
                let body_type = self.check_expr(body, &mut lambda_env)?;
                lambda_env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
                
                Ok(Type::Linear(Box::new(Type::Int), Box::new(body_type)))
            }
            
            Expr::Let { var, value, body } => {
                let value_type = self.check_expr(value, env)?;
                
                let mut body_env = env.clone();
                body_env.bind(var.clone(), value_type);
                
                let body_type = self.check_expr(body, &mut body_env)?;
                body_env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
                
                Ok(body_type)
            }
            
            Expr::If { cond, then_branch, else_branch } => {
                let cond_type = self.check_expr(cond, env)?;
                if cond_type != Type::Int {
                    return Err(TypeError::TypeMismatch {
                        expected: Type::Int,
                        found: cond_type,
                    });
                }
                
                // For linear logic, we need to split the environment
                let (mut then_env, mut else_env) = env.clone().split();
                
                let then_type = self.check_expr(then_branch, &mut then_env)?;
                let else_type = self.check_expr(else_branch, &mut else_env)?;
                
                if then_type != else_type {
                    return Err(TypeError::TypeMismatch {
                        expected: then_type,
                        found: else_type,
                    });
                }
                
                // Both branches must use the same linear variables
                then_env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
                else_env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
                
                Ok(then_type)
            }
            
            Expr::Pair(left, right) => {
                let left_type = self.check_expr(left, env)?;
                let right_type = self.check_expr(right, env)?;
                Ok(Type::Tensor(Box::new(left_type), Box::new(right_type)))
            }
            
            Expr::MatchPair { expr, left_var, right_var, body } => {
                let expr_type = self.check_expr(expr, env)?;
                
                match expr_type {
                    Type::Tensor(left_type, right_type) => {
                        let mut body_env = env.clone();
                        body_env.bind(left_var.clone(), *left_type);
                        body_env.bind(right_var.clone(), *right_type);
                        
                        let body_type = self.check_expr(body, &mut body_env)?;
                        body_env.check_all_used().map_err(|msg| TypeError::UnusedVariable(msg))?;
                        
                        Ok(body_type)
                    }
                    _ => Err(TypeError::TypeMismatch {
                        expected: Type::Tensor(Box::new(Type::Unit), Box::new(Type::Unit)),
                        found: expr_type,
                    }),
                }
            }
            
            Expr::Alloc { size } => {
                let size_type = self.check_expr(size, env)?;
                if size_type != Type::Int {
                    return Err(TypeError::TypeMismatch {
                        expected: Type::Int,
                        found: size_type,
                    });
                }
                Ok(Type::LinearPtr(Box::new(Type::Int))) // Simplified
            }
            
            Expr::Free { ptr } => {
                let ptr_type = self.check_expr(ptr, env)?;
                match ptr_type {
                    Type::LinearPtr(_) => Ok(Type::Unit),
                    _ => Err(TypeError::TypeMismatch {
                        expected: Type::LinearPtr(Box::new(Type::Unit)),
                        found: ptr_type,
                    }),
                }
            }
            
            Expr::Load { ptr } => {
                let ptr_type = self.check_expr(ptr, env)?;
                match ptr_type {
                    Type::LinearPtr(inner_type) => Ok(*inner_type),
                    _ => Err(TypeError::TypeMismatch {
                        expected: Type::LinearPtr(Box::new(Type::Unit)),
                        found: ptr_type,
                    }),
                }
            }
            
            Expr::Store { ptr, value } => {
                let ptr_type = self.check_expr(ptr, env)?;
                let value_type = self.check_expr(value, env)?;
                
                match ptr_type {
                    Type::LinearPtr(inner_type) => {
                        if *inner_type != value_type {
                            return Err(TypeError::TypeMismatch {
                                expected: *inner_type,
                                found: value_type,
                            });
                        }
                        Ok(Type::Unit)
                    }
                    _ => Err(TypeError::TypeMismatch {
                        expected: Type::LinearPtr(Box::new(Type::Unit)),
                        found: ptr_type,
                    }),
                }
            }
            
            Expr::BinOp { op, left, right } => {
                let left_type = self.check_expr(left, env)?;
                let right_type = self.check_expr(right, env)?;
                
                match (op, &left_type, &right_type) {
                    (BinOpKind::Add | BinOpKind::Sub | BinOpKind::Mul | BinOpKind::Div, Type::Int, Type::Int) => {
                        Ok(Type::Int)
                    }
                    (BinOpKind::Eq | BinOpKind::Lt | BinOpKind::Gt, Type::Int, Type::Int) => {
                        Ok(Type::Int) // Using Int for boolean (simplified)
                    }
                    (BinOpKind::And | BinOpKind::Or, Type::Int, Type::Int) => {
                        Ok(Type::Int)
                    }
                    _ => Err(TypeError::InvalidBinaryOp {
                        op: op.clone(),
                        left: left_type,
                        right: right_type,
                    }),
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::tokenize;
    use crate::parser::Parser;
    
    #[test]
    fn test_simple_type_check() {
        let tokens = tokenize("let x = 42 in x + 1").unwrap();
        let mut parser = Parser::new(tokens);
        let program = parser.parse_program().unwrap();
        
        let mut type_checker = TypeChecker::new();
        let result = type_checker.check_program(&program);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), Type::Int);
    }
    
    #[test]
    fn test_linearity_violation() {
        let tokens = tokenize("let x = 42 in x + x").unwrap();
        let mut parser = Parser::new(tokens);
        let program = parser.parse_program().unwrap();
        
        let mut type_checker = TypeChecker::new();
        let result = type_checker.check_program(&program);
        assert!(result.is_err());
        // Should fail due to using x twice
    }
}
