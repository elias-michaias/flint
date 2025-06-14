use crate::ast::{Term, Clause, Query, Substitution};
use std::collections::{HashMap, VecDeque};

/// Unification engine for logical programming
pub struct UnificationEngine {
    /// Database of facts and rules
    clauses: Vec<Clause>,
    /// Variable counter for generating unique variables
    var_counter: usize,
}

impl UnificationEngine {
    pub fn new() -> Self {
        Self {
            clauses: Vec::new(),
            var_counter: 0,
        }
    }
    
    pub fn add_clause(&mut self, clause: Clause) {
        self.clauses.push(clause);
    }
    
    /// Generate a fresh variable name
    fn fresh_var(&mut self) -> String {
        let var = format!("_G{}", self.var_counter);
        self.var_counter += 1;
        var
    }
    
    /// Rename variables in a term to avoid conflicts
    fn rename_variables(&mut self, term: &Term, mapping: &mut HashMap<String, String>) -> Term {
        match term {
            Term::Var { name, type_name } => {
                if let Some(new_name) = mapping.get(name) {
                    Term::Var { name: new_name.clone(), type_name: type_name.clone() }
                } else {
                    let new_name = self.fresh_var();
                    mapping.insert(name.clone(), new_name.clone());
                    Term::Var { name: new_name, type_name: type_name.clone() }
                }
            }
            Term::Compound { functor, args } => {
                Term::Compound {
                    functor: functor.clone(),
                    args: args.iter().map(|arg| self.rename_variables(arg, mapping)).collect(),
                }
            }
            _ => term.clone(),
        }
    }
    
    /// Unify two terms, returning a substitution if successful
    pub fn unify(&self, term1: &Term, term2: &Term) -> Option<Substitution> {
        let mut subst = Substitution::new();
        if self.unify_terms(term1, term2, &mut subst) {
            Some(subst)
        } else {
            None
        }
    }
    
    /// Internal unification algorithm
    fn unify_terms(&self, term1: &Term, term2: &Term, subst: &mut Substitution) -> bool {
        let t1 = subst.apply(term1);
        let t2 = subst.apply(term2);
        
        match (&t1, &t2) {
            // Variable unification
            (Term::Var { name: n1, .. }, Term::Var { name: n2, .. }) if n1 == n2 => true,
            (Term::Var { name, .. }, term) | (term, Term::Var { name, .. }) => {
                if self.occurs_check(name, term) {
                    false // Occurs check failure
                } else {
                    subst.bind(name.clone(), term.clone());
                    true
                }
            }
            
            // Atom unification
            (Term::Atom { name: n1, .. }, Term::Atom { name: n2, .. }) => n1 == n2,
            
            // Integer unification
            (Term::Integer(i1), Term::Integer(i2)) => i1 == i2,
            
            // Compound term unification
            (Term::Compound { functor: f1, args: args1 }, 
             Term::Compound { functor: f2, args: args2 }) => {
                if f1 == f2 && args1.len() == args2.len() {
                    args1.iter().zip(args2.iter()).all(|(a1, a2)| {
                        self.unify_terms(a1, a2, subst)
                    })
                } else {
                    false
                }
            }
            
            _ => false,
        }
    }
    
    /// Occurs check to prevent infinite structures
    fn occurs_check(&self, var: &str, term: &Term) -> bool {
        match term {
            Term::Var { name, .. } => var == name,
            Term::Compound { args, .. } => {
                args.iter().any(|arg| self.occurs_check(var, arg))
            }
            _ => false,
        }
    }
    
    /// Resolve a query using SLD resolution
    pub fn resolve(&mut self, query: &Query) -> Vec<Substitution> {
        let mut solutions = Vec::new();
        let mut stack = VecDeque::new();
        
        // Initialize with the query goals
        stack.push_back((query.goals.clone(), Substitution::new()));
        
        while let Some((goals, mut subst)) = stack.pop_back() {
            if goals.is_empty() {
                // Success - all goals resolved
                solutions.push(subst);
                continue;
            }
            
            let goal = &goals[0];
            let remaining_goals = goals[1..].to_vec();
            
            // Try to match against each clause
            for clause in &self.clauses.clone() {
                let mut var_mapping = HashMap::new();
                let renamed_clause = self.rename_clause(clause, &mut var_mapping);
                
                match &renamed_clause {
                    Clause::Fact { predicate, args, .. } => {
                        let fact_term = Term::Compound {
                            functor: predicate.clone(),
                            args: args.clone(),
                        };
                        
                        if let Some(new_subst) = self.unify(goal, &fact_term) {
                            let mut combined_subst = subst.clone();
                            self.compose_substitutions(&mut combined_subst, &new_subst);
                            
                            let new_goals: Vec<Term> = remaining_goals.iter()
                                .map(|g| combined_subst.apply(g))
                                .collect();
                            
                            stack.push_back((new_goals, combined_subst));
                        }
                    }
                    
                    Clause::Rule { head, body, .. } => {
                        if let Some(new_subst) = self.unify(goal, head) {
                            let mut combined_subst = subst.clone();
                            self.compose_substitutions(&mut combined_subst, &new_subst);
                            
                            let mut new_goals = body.iter()
                                .map(|g| combined_subst.apply(g))
                                .collect::<Vec<_>>();
                            
                            new_goals.extend(remaining_goals.iter()
                                .map(|g| combined_subst.apply(g)));
                            
                            stack.push_back((new_goals, combined_subst));
                        }
                    }
                }
            }
        }
        
        solutions
    }
    
    /// Rename variables in a clause
    fn rename_clause(&mut self, clause: &Clause, mapping: &mut HashMap<String, String>) -> Clause {
        match clause {
            Clause::Fact { predicate, args, .. } => {
                Clause::Fact {
                    predicate: predicate.clone(),
                    args: args.iter().map(|arg| self.rename_variables(arg, mapping)).collect(),
                    persistent: false, // Default for renamed clauses
                }
            }
            Clause::Rule { head, body, .. } => {
                Clause::Rule {
                    head: self.rename_variables(head, mapping),
                    body: body.iter().map(|term| self.rename_variables(term, mapping)).collect(),
                    produces: None, // TODO: Handle produces in renaming
                }
            }
        }
    }
    
    /// Compose two substitutions
    fn compose_substitutions(&self, subst1: &mut Substitution, subst2: &Substitution) {
        // Apply subst2 to all bindings in subst1
        for (var, term) in &mut subst1.bindings {
            *term = subst2.apply(term);
        }
        
        // Add bindings from subst2 that aren't in subst1
        for (var, term) in &subst2.bindings {
            if !subst1.bindings.contains_key(var) {
                subst1.bindings.insert(var.clone(), term.clone());
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_simple_unification() {
        let engine = UnificationEngine::new();
        
        let term1 = Term::Var { name: "X".to_string(), type_name: None };
        let term2 = Term::Atom { name: "john".to_string(), type_name: None };
        
        let result = engine.unify(&term1, &term2);
        assert!(result.is_some());
        
        let subst = result.unwrap();
        assert_eq!(subst.lookup("X"), Some(&Term::Atom { name: "john".to_string(), type_name: None }));
    }
    
    #[test]
    fn test_compound_unification() {
        let engine = UnificationEngine::new();
        
        let term1 = Term::Compound {
            functor: "parent".to_string(),
            args: vec![
                Term::Var { name: "X".to_string(), type_name: None }, 
                Term::Atom { name: "mary".to_string(), type_name: None }
            ],
        };
        
        let term2 = Term::Compound {
            functor: "parent".to_string(),
            args: vec![
                Term::Atom { name: "john".to_string(), type_name: None }, 
                Term::Var { name: "Y".to_string(), type_name: None }
            ],
        };
        
        let result = engine.unify(&term1, &term2);
        assert!(result.is_some());
        
        let subst = result.unwrap();
        assert_eq!(subst.lookup("X"), Some(&Term::Atom { name: "john".to_string(), type_name: None }));
        assert_eq!(subst.lookup("Y"), Some(&Term::Atom { name: "mary".to_string(), type_name: None }));
    }
}
