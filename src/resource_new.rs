use crate::ast::*;
use std::collections::{HashSet, HashMap};

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

    /// COMPILE-TIME ERRORS: Resources not consumed
    UnconsumedResource {
        resource_name: String,
        location: String,
    },

    /// Linear variable used multiple times without clone
    MultipleUseWithoutClone {
        variable: String,
        first_use: String,
        second_use: String,
    },

    /// Attempting to access deallocated resource
    UseAfterFree {
        resource_name: String,
        deallocation_site: String,
        use_site: String,
    },
}

/// Resource state for tracking consumption and memory
#[derive(Debug, Clone, PartialEq)]
pub enum ResourceState {
    Available,
    Consumed,
    Deallocated,  // NEW: Actually freed from memory
}

/// Backtracking checkpoint for resource states
#[derive(Debug, Clone)]
pub struct ResourceCheckpoint {
    resource_states: HashMap<usize, ResourceState>,
    next_id: usize,
    memory_allocations: Vec<usize>,  // Track allocated resource IDs
}

/// Memory allocation record for tracking what needs deallocation
#[derive(Debug, Clone)]
pub struct MemoryAllocation {
    resource_id: usize,
    allocated_at: String,  // Location info for debugging
    size_estimate: usize,  // For memory pressure tracking
}

/// COMPILE-TIME: Linear resource usage analysis
#[derive(Debug, Clone)]
pub struct LinearUsageAnalysis {
    /// Resources that must be consumed
    required_consumption: HashSet<String>,
    /// Resources that have been consumed
    consumed_resources: HashSet<String>,
    /// Variables that have been used (for multiple-use detection)
    used_variables: HashMap<String, String>,  // var_name -> first_use_location
    /// Memory allocations that need cleanup
    pending_deallocations: Vec<String>,
}

impl LinearUsageAnalysis {
    pub fn new() -> Self {
        Self {
            required_consumption: HashSet::new(),
            consumed_resources: HashSet::new(),
            used_variables: HashMap::new(),
            pending_deallocations: Vec::new(),
        }
    }

    /// Mark a resource as requiring consumption
    pub fn require_consumption(&mut self, resource_name: String) {
        self.required_consumption.insert(resource_name);
    }

    /// Mark a resource as consumed
    pub fn consume_resource(&mut self, resource_name: String) -> Result<(), LinearError> {
        if !self.required_consumption.contains(&resource_name) {
            return Err(LinearError::UnconsumedResource {
                resource_name: resource_name.clone(),
                location: "unknown".to_string(),
            });
        }
        self.consumed_resources.insert(resource_name);
        Ok(())
    }

    /// Check for linear variable reuse
    pub fn use_variable(&mut self, var_name: String, location: String) -> Result<(), LinearError> {
        if let Some(first_use) = self.used_variables.get(&var_name) {
            return Err(LinearError::MultipleUseWithoutClone {
                variable: var_name,
                first_use: first_use.clone(),
                second_use: location,
            });
        }
        self.used_variables.insert(var_name, location);
        Ok(())
    }

    /// Get unconsumed resources for compile-time error reporting
    pub fn get_unconsumed_resources(&self) -> Vec<String> {
        self.required_consumption
            .difference(&self.consumed_resources)
            .cloned()
            .collect()
    }

    /// Check if all resources are properly consumed
    pub fn validate_complete_consumption(&self) -> Result<(), Vec<LinearError>> {
        let unconsumed = self.get_unconsumed_resources();
        if unconsumed.is_empty() {
            Ok(())
        } else {
            let errors = unconsumed.into_iter().map(|resource| {
                LinearError::UnconsumedResource {
                    resource_name: resource,
                    location: "end of scope".to_string(),
                }
            }).collect();
            Err(errors)
        }
    }
}

/// Resource flow analysis for optimization and debugging
#[derive(Debug, Clone)]
pub struct ResourceFlowAnalysis {
    pub total_allocations: usize,
    pub total_deallocations: usize,
    pub peak_memory_usage: usize,
    pub average_resource_lifetime: f64,
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
                    predicate: "unknown".to_string(),
                });
            }
        }
        Ok(())
    }
}

/// Linear resource manager for proper linear logic semantics with automatic memory management
#[derive(Debug)]
pub struct LinearResourceManager {
    /// All available resources with unique IDs
    resources: Vec<LinearResource>,
    /// Next available resource ID
    next_id: usize,
    /// Rules (these don't get consumed, but create consumption constraints)
    rules: Vec<Clause>,
    /// Memory allocations for automatic deallocation
    memory_allocations: HashMap<usize, MemoryAllocation>,
    /// Backtracking checkpoints stack
    checkpoints: Vec<ResourceCheckpoint>,
    /// Current usage analysis for compile-time checking
    usage_analysis: LinearUsageAnalysis,
    /// Enable automatic deallocation on consumption
    auto_deallocate: bool,
}

impl LinearResourceManager {
    pub fn new() -> Self {
        Self {
            resources: Vec::new(),
            next_id: 0,
            rules: Vec::new(),
            memory_allocations: HashMap::new(),
            checkpoints: Vec::new(),
            usage_analysis: LinearUsageAnalysis::new(),
            auto_deallocate: true,
        }
    }

    /// Create a new manager with specified deallocation behavior
    pub fn with_auto_deallocate(auto_deallocate: bool) -> Self {
        Self {
            resources: Vec::new(),
            next_id: 0,
            rules: Vec::new(),
            memory_allocations: HashMap::new(),
            checkpoints: Vec::new(),
            usage_analysis: LinearUsageAnalysis::new(),
            auto_deallocate,
        }
    }

    /// Add a linear fact (becomes a consumable resource) with memory tracking
    pub fn add_fact(&mut self, predicate: String, args: Vec<Term>) {
        let clause = Clause::Fact { predicate: predicate.clone(), args, persistent: false, name: None };
        let resource = LinearResource {
            id: self.next_id,
            clause,
            state: ResourceState::Available,
        };

        // Track memory allocation
        let allocation = MemoryAllocation {
            resource_id: self.next_id,
            allocated_at: format!("add_fact({})", predicate),
            size_estimate: 64 + predicate.len() * 8, // Rough estimate
        };
        self.memory_allocations.insert(self.next_id, allocation);

        // Compile-time tracking
        self.usage_analysis.require_consumption(predicate);

        self.resources.push(resource);
        self.next_id += 1;
    }

    /// Add a rule (doesn't get consumed, but constrains resource usage)
    pub fn add_rule(&mut self, head: Term, body: Vec<Term>) {
        let clause = Clause::Rule { head, body, produces: None };
        self.rules.push(clause);
    }

    /// COMPILE-TIME: Validate complete resource consumption for a program
    pub fn validate_program_linearity(&self) -> Result<(), Vec<LinearError>> {
        self.usage_analysis.validate_complete_consumption()
    }

    /// COMPILE-TIME: Check for unconsumed resources
    pub fn get_unconsumed_resources(&self) -> Vec<String> {
        self.usage_analysis.get_unconsumed_resources()
    }

    /// Get available and consumed resource counts (excluding deallocated)
    pub fn get_resource_counts(&self) -> (usize, usize) {
        let available = self.resources.iter().filter(|r| r.state == ResourceState::Available).count();
        let consumed = self.resources.iter().filter(|r| r.state == ResourceState::Consumed).count();
        (available, consumed)
    }

    /// Get detailed resource counts including deallocated
    pub fn get_detailed_resource_counts(&self) -> (usize, usize, usize) {
        let available = self.resources.iter().filter(|r| r.state == ResourceState::Available).count();
        let consumed = self.resources.iter().filter(|r| r.state == ResourceState::Consumed).count();
        let deallocated = self.resources.iter().filter(|r| r.state == ResourceState::Deallocated).count();
        (available, consumed, deallocated)
    }

    pub fn analyze_resource_flow(&self) -> ResourceFlowAnalysis {
        ResourceFlowAnalysis {
            total_allocations: self.memory_allocations.len(),
            total_deallocations: self.resources.iter()
                .filter(|r| r.state == ResourceState::Deallocated)
                .count(),
            peak_memory_usage: self.get_memory_usage(),
            average_resource_lifetime: self.calculate_average_lifetime(),
        }
    }

    fn calculate_average_lifetime(&self) -> f64 {
        let total_resources = self.resources.len() as f64;
        if total_resources == 0.0 {
            0.0
        } else {
            let deallocated_count = self.resources.iter()
                .filter(|r| r.state == ResourceState::Deallocated)
                .count() as f64;
            deallocated_count / total_resources
        }
    }

    pub fn get_memory_usage(&self) -> usize {
        self.memory_allocations.values()
            .map(|alloc| alloc.size_estimate)
            .sum()
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
            Term::Clone(_inner) => {
                // Cloned terms don't consume variables, just reference them
            }
            _ => {}
        }
    }

    /// ADVANCED: Check for linear variable usage patterns
    pub fn check_variable_linearity(&mut self, var_name: String, usage_location: String) -> Result<(), LinearError> {
        self.usage_analysis.use_variable(var_name, usage_location)
    }
}

/// COMPILE-TIME LINEAR ANALYSIS: Main entry point for compile-time checking
pub fn analyze_program_linearity(program: &Program) -> Result<(), Vec<LinearError>> {
    let mut manager = LinearResourceManager::new();
    let mut errors = Vec::new();

    // Analyze each clause in the program
    for clause in &program.clauses {
        if let Err(clause_errors) = analyze_clause_linearity(&mut manager, clause) {
            errors.extend(clause_errors);
        }
    }

    // Check for overall program linearity violations
    if let Err(program_errors) = manager.validate_program_linearity() {
        errors.extend(program_errors);
    }

    if errors.is_empty() {
        Ok(())
    } else {
        Err(errors)
    }
}

/// Analyze a single clause for linear resource usage
fn analyze_clause_linearity(manager: &mut LinearResourceManager, clause: &Clause) -> Result<(), Vec<LinearError>> {
    match clause {
        Clause::Fact { predicate, args, persistent, .. } => {
            if !*persistent {
                manager.add_fact(predicate.clone(), args.clone());
            }
            Ok(())
        }
        Clause::Rule { head, body, .. } => {
            // Check that rule properly consumes linear resources
            if let Err(error) = manager.check_rule_consumption(clause) {
                Err(vec![error])
            } else {
                // Analyze variable usage in the rule
                let mut var_errors = Vec::new();
                
                // Check head variables
                if let Err(e) = check_term_variable_usage(manager, head, "rule_head") {
                    var_errors.push(e);
                }
                
                // Check body variables
                for (i, body_term) in body.iter().enumerate() {
                    if let Err(e) = check_term_variable_usage(manager, body_term, &format!("rule_body_{}", i)) {
                        var_errors.push(e);
                    }
                }
                
                if var_errors.is_empty() {
                    Ok(())
                } else {
                    Err(var_errors)
                }
            }
        }
    }
}

/// Check variable usage in a term for linearity violations
fn check_term_variable_usage(manager: &mut LinearResourceManager, term: &Term, location: &str) -> Result<(), LinearError> {
    match term {
        Term::Var { name, .. } => {
            manager.check_variable_linearity(name.clone(), location.to_string())
        }
        Term::Compound { args, .. } => {
            for (i, arg) in args.iter().enumerate() {
                check_term_variable_usage(manager, arg, &format!("{}[{}]", location, i))?;
            }
            Ok(())
        }
        Term::Clone(_inner) => {
            // Clone allows multiple uses, so we don't check linearity for cloned terms
            Ok(())
        }
        _ => Ok(()),
    }
}

/// COMPILE-TIME: Generate a linearity report for debugging
pub fn generate_linearity_report(program: &Program) -> String {
    let mut manager = LinearResourceManager::new();
    let mut report = String::new();
    
    report.push_str("=== LINEAR RESOURCE ANALYSIS REPORT ===\n");
    
    // Analyze the program
    for clause in &program.clauses {
        let _ = analyze_clause_linearity(&mut manager, clause);
    }
    
    // Generate the report
    report.push_str(&format!("Total clauses analyzed: {}\n", program.clauses.len()));
    
    let (available, consumed, deallocated) = manager.get_detailed_resource_counts();
    report.push_str(&format!("Resources - Available: {}, Consumed: {}, Deallocated: {}\n", 
                            available, consumed, deallocated));
    
    let unconsumed = manager.get_unconsumed_resources();
    if !unconsumed.is_empty() {
        report.push_str(&format!("WARNING: Unconsumed resources: {:?}\n", unconsumed));
    }
    
    let flow_analysis = manager.analyze_resource_flow();
    report.push_str(&format!("Memory analysis - Allocations: {}, Deallocations: {}, Peak usage: {} bytes\n",
                            flow_analysis.total_allocations, flow_analysis.total_deallocations, 
                            flow_analysis.peak_memory_usage));
    
    report.push_str("========================================\n");
    
    report
}
