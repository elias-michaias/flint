use crate::ast::*;
use std::collections::HashMap;

#[derive(Debug, thiserror::Error)]
pub enum TypeCheckError {
    #[error("Predicate '{predicate}' expects {expected} arguments, but got {actual}")]
    ArityMismatch {
        predicate: String,
        expected: usize,
        actual: usize,
    },
    
    #[error("Type mismatch: expected {expected:?}, but got {actual:?}")]
    TypeMismatch {
        expected: LogicType,
        actual: LogicType,
    },
    
    #[error("Unknown predicate: {predicate}")]
    UnknownPredicate { predicate: String },
    
    #[error("Unknown term: {term}")]
    UnknownTerm { term: String },
    
    #[error("Unification failed: cannot unify {term1:?} with {term2:?}")]
    UnificationFailed { term1: Term, term2: Term },
}

pub struct TypeChecker {
    predicate_types: HashMap<String, PredicateType>,
    term_types: HashMap<String, LogicType>,
}

impl TypeChecker {
    pub fn new() -> Self {
        Self {
            predicate_types: HashMap::new(),
            term_types: HashMap::new(),
        }
    }
    
    pub fn add_predicate_type(&mut self, pred_type: PredicateType) {
        self.predicate_types.insert(pred_type.name.clone(), pred_type);
    }
    
    pub fn add_term_type(&mut self, term_type: TermType) {
        self.term_types.insert(term_type.name.clone(), term_type.term_type);
    }
    
    pub fn check_program(&mut self, program: &Program) -> Result<(), TypeCheckError> {
        // Load type declarations
        for pred_type in &program.type_declarations {
            self.add_predicate_type(pred_type.clone());
        }
        
        for term_type in &program.term_types {
            self.add_term_type(term_type.clone());
        }
        
        // Validate that all user-defined types are properly declared
        self.validate_user_defined_types(program)?;
        
        // Check all clauses
        for clause in &program.clauses {
            self.check_clause(clause, program)?;
        }
        
        // Check queries if present
        for query in &program.queries {
            self.check_query(query)?;
        }
        
        Ok(())
    }
    
    fn check_clause(&self, clause: &Clause, program: &Program) -> Result<(), TypeCheckError> {
        match clause {
            Clause::Fact { predicate, args, .. } => {
                // Check if this is a nullary fact from a term type
                if args.is_empty() && self.term_types.contains_key(predicate) {
                    // This is a nullary fact like "c1" - it's valid if it's in term_types
                    return Ok(());
                } else {
                    // Regular predicate fact - check as usual
                    self.check_predicate_call(predicate, args)?;
                    return Ok(());
                }
            }
            Clause::Rule { head, body, produces } => {
                // Check head
                match head {
                    Term::Compound { functor, args } => {
                        self.check_predicate_call(functor, args)?;
                    }
                    _ => {
                        // Single atom fact - infer as 0-arity predicate
                    }
                }
                
                // Check body terms
                for term in body {
                    match term {
                        Term::Compound { functor, args } => {
                            self.check_predicate_call(functor, args)?;
                        }
                        _ => {
                            // Single atom - infer as 0-arity predicate
                        }
                    }
                }
                
                // Check production if present
                if let Some(production) = produces {
                    self.check_production(production, program)?;
                }
                
                // Check variable consistency between head and body
                self.check_variable_consistency(head, body)?;
            }
        }
        
        Ok(())
    }
    
    fn check_query(&self, query: &Query) -> Result<(), TypeCheckError> {
        for term in &query.goals {
            match term {
                Term::Compound { functor, args } => {
                    self.check_predicate_call(functor, args)?;
                }
                _ => {
                    // Single atom query
                }
            }
        }
        Ok(())
    }
    
    fn check_predicate_call(&self, predicate: &str, args: &[Term]) -> Result<(), TypeCheckError> {
        let pred_type = self.predicate_types.get(predicate)
            .ok_or_else(|| TypeCheckError::UnknownPredicate {
                predicate: predicate.to_string(),
            })?;
        
        // Extract argument types from the arrow signature
        let arg_types = self.extract_arg_types(&pred_type.signature);
        
        // Check arity
        if args.len() != arg_types.len() {
            return Err(TypeCheckError::ArityMismatch {
                predicate: predicate.to_string(),
                expected: arg_types.len(),
                actual: args.len(),
            });
        }
        
        // Check argument types
        for (_i, (arg, expected_type)) in args.iter().zip(&arg_types).enumerate() {
            let actual_type = self.infer_term_type(arg)?;
            if !self.types_compatible(&actual_type, expected_type) {
                return Err(TypeCheckError::TypeMismatch {
                    expected: expected_type.clone(),
                    actual: actual_type,
                });
            }
        }
        
        Ok(())
    }
    
    fn infer_term_type(&self, term: &Term) -> Result<LogicType, TypeCheckError> {
        match term {
            Term::Atom { name, type_name: _ } => {
                // Look up the term's declared type
                if let Some(term_type) = self.term_types.get(name) {
                    Ok(term_type.clone())
                } else {
                    // For now, if a term doesn't have an explicit type, we'll infer "any"
                    // In a stricter system, this would be an error
                    if name.chars().next().unwrap_or('a').is_uppercase() {
                        // Variables get special treatment - for now we'll say they're "any"
                        Ok(LogicType::Named("any".to_string()))
                    } else {
                        Err(TypeCheckError::UnknownTerm { term: name.clone() })
                    }
                }
            }
            Term::Var { name: _, type_name } => {
                // Variables should have their types inferred from context
                // For now, return a placeholder type
                if let Some(type_name) = type_name {
                    Ok(type_name.clone())
                } else {
                    Ok(LogicType::Named("any".to_string()))
                }
            }
            Term::Integer(_) => Ok(LogicType::Integer),
            Term::Compound { functor, args: _ } => {
                // For compound terms, we need to look up the predicate type
                // This is a simplified approach - in practice you'd want more sophisticated inference
                Err(TypeCheckError::UnknownTerm { term: functor.clone() })
            }
            Term::Clone(inner) => {
                // Cloned terms have the same type as their inner term
                self.infer_term_type(inner)
            }
        }
    }
    
    fn types_compatible(&self, actual: &LogicType, expected: &LogicType) -> bool {
        match (actual, expected) {
            (LogicType::Named(a), LogicType::Named(e)) => a == e || a == "any" || e == "any",
            (LogicType::Integer, LogicType::Integer) => true,
            (LogicType::String, LogicType::String) => true,
            (LogicType::Type, LogicType::Type) => true,
            _ => false,
        }
    }
    
    fn check_variable_consistency(&self, head: &Term, body: &[Term]) -> Result<(), TypeCheckError> {
        let mut var_types = HashMap::new();
        
        // Collect variable types from head
        self.collect_variable_types(head, &mut var_types)?;
        
        // Collect and check variable types from body
        for term in body {
            self.collect_variable_types(term, &mut var_types)?;
        }
        
        // Verify all variables are consistently typed
        self.check_term_variables(head, &var_types)?;
        for term in body {
            self.check_term_variables(term, &var_types)?;
        }
        
        Ok(())
    }
    
    fn collect_variable_types(&self, term: &Term, var_types: &mut HashMap<String, LogicType>) -> Result<(), TypeCheckError> {
        match term {
            Term::Var { name, type_name: _ } => {
                // For now, we'll assign a generic type to variables
                var_types.insert(name.clone(), LogicType::Named("any".to_string()));
            }
            Term::Compound { functor: _, args } => {
                for arg in args {
                    self.collect_variable_types(arg, var_types)?;
                }
            }
            _ => {}
        }
        Ok(())
    }
    
    fn check_term_variables(&self, term: &Term, var_types: &HashMap<String, LogicType>) -> Result<(), TypeCheckError> {
        match term {
            Term::Var { name, type_name: _ } => {
                if let Some(_expected_type) = var_types.get(name) {
                    // Variable is consistent
                } else {
                    return Err(TypeCheckError::TypeMismatch {
                        expected: LogicType::Named("any".to_string()),
                        actual: LogicType::Named("unknown".to_string()),
                    });
                }
            }
            Term::Compound { functor: _, args } => {
                for arg in args {
                    self.check_term_variables(arg, var_types)?;
                }
            }
            _ => {}
        }
        Ok(())
    }
    
    /// Extract argument types from an arrow type signature
    /// e.g., person -> person -> type becomes [person, person]
    fn extract_arg_types(&self, logic_type: &LogicType) -> Vec<LogicType> {
        match logic_type {
            LogicType::Arrow(types) => {
                // Return all but the last type (which should be 'type')
                if types.len() > 1 {
                    types[..types.len()-1].to_vec()
                } else {
                    Vec::new()
                }
            }
            _ => Vec::new(), // Not an arrow type, no arguments
        }
    }
    
    fn validate_user_defined_types(&self, program: &Program) -> Result<(), TypeCheckError> {
        // Get all types that are defined with ":: type."
        let mut defined_types = std::collections::HashSet::new();
        defined_types.insert("int".to_string());  // Built-in types
        defined_types.insert("string".to_string());
        defined_types.insert("type".to_string());
        
        // Find all user-defined types from term_types (old syntax: name :: type.)
        for term_type in &program.term_types {
            if let LogicType::Type = term_type.term_type {
                defined_types.insert(term_type.name.clone());
            }
        }
        
        // Find all user-defined types from type_definitions (new syntax: type name.)
        for type_def in &program.type_definitions {
            defined_types.insert(type_def.name.clone());
            
            // Also add union variants as types if they exist
            if let Some(ref variants) = type_def.union_variants {
                self.add_union_variants_to_set(&mut defined_types, variants);
            }
        }
        
        // Check that all referenced types are defined
        for term_type in &program.term_types {
            match &term_type.term_type {
                LogicType::Named(type_name) => {
                    if !defined_types.contains(type_name) {
                        return Err(TypeCheckError::UnknownTerm { 
                            term: format!("Type '{}' is not defined. Define it with '{} :: type.'", type_name, type_name)
                        });
                    }
                }
                LogicType::Arrow(types) => {
                    // Check all types in the arrow
                    for logic_type in types {
                        if let LogicType::Named(type_name) = logic_type {
                            if !defined_types.contains(type_name) {
                                return Err(TypeCheckError::UnknownTerm { 
                                    term: format!("Type '{}' is not defined. Define it with '{} :: type.'", type_name, type_name)
                                });
                            }
                        }
                    }
                }
                _ => {}
            }
        }
        
        // Also check predicate types
        for pred_type in &program.type_declarations {
            match &pred_type.signature {
                LogicType::Arrow(types) => {
                    for logic_type in types {
                        if let LogicType::Named(type_name) = logic_type {
                            if !defined_types.contains(type_name) {
                                return Err(TypeCheckError::UnknownTerm { 
                                    term: format!("Type '{}' is not defined. Define it with '{} :: type.'", type_name, type_name)
                                });
                            }
                        }
                    }
                }
                LogicType::Named(type_name) => {
                    if !defined_types.contains(type_name) {
                        return Err(TypeCheckError::UnknownTerm { 
                            term: format!("Type '{}' is not defined. Define it with '{} :: type.'", type_name, type_name)
                        });
                    }
                }
                _ => {}
            }
        }
        
        Ok(())
    }
    
    fn add_union_variants_to_set(&self, defined_types: &mut std::collections::HashSet<String>, variants: &UnionVariants) {
        match variants {
            UnionVariants::Simple(names) => {
                for name in names {
                    defined_types.insert(name.clone());
                }
            }
            UnionVariants::Nested(variant_list) => {
                for variant in variant_list {
                    defined_types.insert(variant.name.clone());
                    if let Some(ref sub_variants) = variant.sub_variants {
                        self.add_union_variants_to_set(defined_types, sub_variants);
                    }
                }
            }
        }
    }
    
    fn check_production(&self, production: &Term, program: &Program) -> Result<(), TypeCheckError> {
        match production {
            Term::Atom { name, .. } => {
                // Build the same set of defined types as in check_program
                let mut defined_types = std::collections::HashSet::new();
                
                // Add built-in types
                defined_types.insert("type".to_string());
                
                // Find all user-defined types from term_types (old syntax: name :: type.)
                for term_type in &program.term_types {
                    if let LogicType::Type = term_type.term_type {
                        defined_types.insert(term_type.name.clone());
                    }
                }
                
                // Find all user-defined types from type_definitions (new syntax: type name.)
                for type_def in &program.type_definitions {
                    defined_types.insert(type_def.name.clone());
                    
                    // Also add union variants as types if they exist
                    if let Some(ref variants) = type_def.union_variants {
                        self.add_union_variants_to_set(&mut defined_types, variants);
                    }
                }
                
                // Check if the production name is a defined type
                if !defined_types.contains(name) {
                    return Err(TypeCheckError::UnknownTerm { term: name.clone() });
                }
            }
            Term::Compound { functor, args } => {
                // For compound productions, check the functor as a predicate
                self.check_predicate_call(functor, args)?;
            }
            _ => {
                // Other term types (variables, integers) are not valid productions
                return Err(TypeCheckError::UnknownTerm { 
                    term: format!("{:?}", production) 
                });
            }
        }
        Ok(())
    }
}
