use crate::ast::*;
use std::collections::HashSet;

/// Error types for linear resource management
#[derive(Debug, Clone, PartialEq)]
pub enum LinearError {
    /// Attempting to use a resource that has already been consumed
    ResourceAlreadyConsumed {
        resource_id: usize,
        predicate: String,
    },
    
    /// A variable in a predicate was not consumed by the rule body
    UnconsumedVariable {
        variable: String,
        predicate: String,
    },
    
    /// A rule attempts to use more instances of a resource than available
    InsufficientResources {
        predicate: String,
        requested: usize,
        available: usize,
    },
    
    /// No unification possible
    UnificationFailed {
        goal: Term,
        available_facts: Vec<String>,
    },
}

/// Tracks the consumption state of variables in a rule
#[derive(Debug, Clone)]
pub struct VariableTracker {
    /// Variables that appear in the head of a rule
    head_variables: HashSet<String>,
    /// Variables that have been consumed in the body
    consumed_variables: HashSet<String>,
}

impl VariableTracker {
    pub fn new() -> Self {
        Self {
            head_variables: HashSet::new(),
            consumed_variables: HashSet::new(),
        }
    }
    
    /// Extract variables from a term
    pub fn extract_variables(&mut self, term: &Term) {
        match term {
            Term::Var { name, .. } => {
                self.head_variables.insert(name.clone());
            }
            Term::Compound { args, .. } => {
                for arg in args {
                    self.extract_variables(arg);
                }
            }
            Term::Clone(inner) => {
                // Cloned terms don't require consumption tracking
                // The inner term's variables are available for multiple uses
                self.extract_variables_without_tracking(inner);
            }
            _ => {}
        }
    }
    
    /// Extract variables without adding them to consumption tracking
    fn extract_variables_without_tracking(&self, term: &Term) {
        match term {
            Term::Var { .. } => {
                // Cloned variables don't need consumption tracking
            }
            Term::Compound { args, .. } => {
                for arg in args {
                    self.extract_variables_without_tracking(arg);
                }
            }
            Term::Clone(inner) => {
                self.extract_variables_without_tracking(inner);
            }
            _ => {}
        }
    }
    
    /// Mark a variable as consumed
    pub fn consume_variable(&mut self, var_name: &str) {
        self.consumed_variables.insert(var_name.to_string());
    }
    
    /// Check if all head variables have been consumed
    pub fn check_all_consumed(&self) -> Result<(), LinearError> {
        for var in &self.head_variables {
            if !self.consumed_variables.contains(var) {
                return Err(LinearError::UnconsumedVariable {
                    variable: var.clone(),
                    predicate: "unknown".to_string(), // TODO: Pass predicate name
                });
            }
        }
        Ok(())
    }
}

/// Linear resource manager for proper linear logic semantics
#[derive(Debug)]
pub struct LinearResourceManager {
    /// All available resources with unique IDs
    resources: Vec<LinearResource>,
    /// Next available resource ID
    next_id: usize,
    /// Rules (these don't get consumed, but create consumption constraints)
    rules: Vec<Clause>,
}

impl LinearResourceManager {
    pub fn new() -> Self {
        Self {
            resources: Vec::new(),
            next_id: 0,
            rules: Vec::new(),
        }
    }
    
    /// Add a linear fact (becomes a consumable resource)
    pub fn add_fact(&mut self, predicate: String, args: Vec<Term>) {
        let clause = Clause::Fact { predicate, args, persistent: false };
        let resource = LinearResource {
            id: self.next_id,
            clause,
            state: ResourceState::Available,
        };
        self.resources.push(resource);
        self.next_id += 1;
    }
    
    /// Add a rule (doesn't get consumed, but constrains resource usage)
    pub fn add_rule(&mut self, head: Term, body: Vec<Term>) {
        let clause = Clause::Rule { head, body, produces: None };
        self.rules.push(clause);
    }
    
    /// Find available resources matching a goal
    pub fn find_matching_resources(&self, goal: &Term) -> Vec<&LinearResource> {
        self.resources
            .iter()
            .filter(|resource| {
                resource.state == ResourceState::Available && self.can_unify(goal, &resource.clause)
            })
            .collect()
    }
    
    /// Consume a resource (mark it as used)
    pub fn consume_resource(&mut self, resource_id: usize) -> Result<(), LinearError> {
        // First check if resource exists and get predicate name
        let predicate_name = self.resources
            .iter()
            .find(|r| r.id == resource_id)
            .map(|r| self.extract_predicate_name(&r.clause))
            .unwrap_or_else(|| "unknown".to_string());
        
        if let Some(resource) = self.resources.iter_mut().find(|r| r.id == resource_id) {
            if resource.state == ResourceState::Consumed {
                return Err(LinearError::ResourceAlreadyConsumed {
                    resource_id,
                    predicate: predicate_name,
                });
            }
            resource.state = ResourceState::Consumed;
            Ok(())
        } else {
            Err(LinearError::ResourceAlreadyConsumed {
                resource_id,
                predicate: predicate_name,
            })
        }
    }
    
    /// Try to consume a fact that matches the given goal
    pub fn consume_fact(&mut self, goal: &Term) -> Result<Option<Term>, LinearError> {
        // Find matching resources and get the first one's ID and clause
        let resource_info = self.resources
            .iter()
            .find(|resource| {
                resource.state == ResourceState::Available && self.can_unify(goal, &resource.clause)
            })
            .map(|resource| (resource.id, resource.clause.clone()));
        
        if let Some((resource_id, clause)) = resource_info {
            self.consume_resource(resource_id)?;
            
            // Return the consumed fact
            match clause {
                Clause::Fact { predicate, args, .. } => {
                    Ok(Some(Term::Compound {
                        functor: predicate,
                        args,
                    }))
                }
                _ => Ok(None),
            }
        } else {
            Ok(None)
        }
    }
    
    /// Check if a fact is available (not consumed) that matches the goal
    pub fn has_available_fact(&self, goal: &Term) -> bool {
        !self.find_matching_resources(goal).is_empty()
    }
    
    /// Get available facts as terms
    pub fn get_available_facts(&self) -> Vec<Term> {
        self.get_available_resources()
            .iter()
            .map(|resource| match &resource.clause {
                Clause::Fact { predicate, args, .. } => {
                    Term::Compound {
                        functor: predicate.clone(),
                        args: args.clone(),
                    }
                }
                Clause::Rule { head, .. } => head.clone(),
            })
            .collect()
    }
    
    /// Get consumed facts as terms
    pub fn get_consumed_facts(&self) -> Vec<Term> {
        self.get_consumed_resources()
            .iter()
            .map(|resource| match &resource.clause {
                Clause::Fact { predicate, args, .. } => {
                    Term::Compound {
                        functor: predicate.clone(),
                        args: args.clone(),
                    }
                }
                Clause::Rule { head, .. } => head.clone(),
            })
            .collect()
    }
    
    /// Check rule consumption with proper error handling
    pub fn check_rule_consumption(&self, rule: &Clause) -> Result<(), LinearError> {
        match rule {
            Clause::Rule { head, body, .. } => {
                self.validate_rule_consumption(head, body)
            }
            _ => Ok(()), // Facts don't need validation
        }
    }

    /// Get available and consumed resource counts
    pub fn get_resource_counts(&self) -> (usize, usize) {
        let available = self.resources.iter().filter(|r| r.state == ResourceState::Available).count();
        let consumed = self.resources.iter().filter(|r| r.state == ResourceState::Consumed).count();
        (available, consumed)
    }
    
    /// Validate that a rule properly consumes linear resources
    pub fn validate_rule_consumption(&self, head: &Term, body: &[Term]) -> Result<(), LinearError> {
        let mut tracker = VariableTracker::new();
        
        // Extract variables from head
        tracker.extract_variables(head);
        
        // Check that all head variables are consumed in body
        for body_term in body {
            self.mark_consumed_variables_in_term(body_term, &mut tracker);
        }
        
        // Verify all head variables were consumed
        tracker.check_all_consumed()
    }
    
    /// Helper to mark variables as consumed in a term
    fn mark_consumed_variables_in_term(&self, term: &Term, tracker: &mut VariableTracker) {
        match term {
            Term::Var { name, .. } => {
                tracker.consume_variable(name);
            }
            Term::Compound { args, .. } => {
                for arg in args {
                    self.mark_consumed_variables_in_term(arg, tracker);
                }
            }
            Term::Clone(inner) => {
                // Cloned terms don't consume variables, just reference them
                // So we don't mark them as consumed
                self.mark_cloned_variables_in_term(inner, tracker);
            }
            _ => {}
        }
    }
    
    /// Helper to handle cloned variables (they satisfy the consumption requirement)
    fn mark_cloned_variables_in_term(&self, term: &Term, tracker: &mut VariableTracker) {
        match term {
            Term::Var { name, .. } => {
                // Cloned variables satisfy the consumption requirement even though they're persistent
                tracker.consume_variable(name);
            }
            Term::Compound { args, .. } => {
                for arg in args {
                    self.mark_cloned_variables_in_term(arg, tracker);
                }
            }
            Term::Clone(inner) => {
                self.mark_cloned_variables_in_term(inner, tracker);
            }
            _ => {}
        }
    }

    /// Extract predicate name from a clause for error messages
    fn extract_predicate_name(&self, clause: &Clause) -> String {
        match clause {
            Clause::Fact { predicate, .. } => predicate.clone(),
            Clause::Rule { head, .. } => {
                match head {
                    Term::Atom { name, .. } => name.clone(),
                    Term::Compound { functor, .. } => functor.clone(),
                    _ => "unknown".to_string(),
                }
            }
        }
    }
    
    /// Mark variables as consumed when they appear in body terms
    fn mark_consumed_variables(&self, tracker: &mut VariableTracker, term: &Term) {
        match term {
            Term::Var { name, .. } => {
                tracker.consume_variable(name);
            }
            Term::Compound { args, .. } => {
                for arg in args {
                    self.mark_consumed_variables(tracker, arg);
                }
            }
            Term::Clone(_inner) => {
                // Cloned terms don't consume their variables
                // The original variables remain available
            }
            _ => {}
        }
    }
    
    /// Get all available resources
    pub fn get_available_resources(&self) -> Vec<&LinearResource> {
        self.resources
            .iter()
            .filter(|r| r.state == ResourceState::Available)
            .collect()
    }
    
    /// Get all consumed resources
    pub fn get_consumed_resources(&self) -> Vec<&LinearResource> {
        self.resources
            .iter()
            .filter(|r| r.state == ResourceState::Consumed)
            .collect()
    }
    
    /// Get all rules
    pub fn get_rules(&self) -> &[Clause] {
        &self.rules
    }
    
    /// Simple unification check (simplified for now)
    fn can_unify(&self, goal: &Term, clause: &Clause) -> bool {
        match clause {
            Clause::Fact { predicate, args, .. } => {
                // Check if goal matches the fact
                match goal {
                    Term::Atom { name, .. } => args.is_empty() && name == predicate,
                    Term::Compound { functor, args: goal_args } => {
                        functor == predicate && goal_args.len() == args.len()
                        // TODO: Implement proper unification algorithm
                    }
                    _ => false,
                }
            }
            Clause::Rule { head, .. } => {
                // Check if goal can unify with rule head
                self.terms_can_unify(goal, head)
            }
        }
    }
    
    /// Check if two terms can potentially unify (simplified)
    fn terms_can_unify(&self, t1: &Term, t2: &Term) -> bool {
        match (t1, t2) {
            (Term::Atom { name: n1, .. }, Term::Atom { name: n2, .. }) => n1 == n2,
            (Term::Var { .. }, _) | (_, Term::Var { .. }) => true, // Variables can unify with anything
            (Term::Integer(i1), Term::Integer(i2)) => i1 == i2,
            (Term::Compound { functor: f1, args: a1 }, Term::Compound { functor: f2, args: a2 }) => {
                f1 == f2 && a1.len() == a2.len() && 
                a1.iter().zip(a2.iter()).all(|(arg1, arg2)| self.terms_can_unify(arg1, arg2))
            }
            (Term::Clone(inner1), Term::Clone(inner2)) => self.terms_can_unify(inner1, inner2),
            (Term::Clone(inner), term) | (term, Term::Clone(inner)) => self.terms_can_unify(inner, term),
            _ => false,
        }
    }
}
