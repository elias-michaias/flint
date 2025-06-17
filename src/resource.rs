use crate::ast::*;
use crate::diagnostic::SourceLocation;
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
        location: SourceLocation,
    },

    /// Linear variable used multiple times without clone
    MultipleUseWithoutClone {
        variable: String,
        first_use: SourceLocation,
        second_use: SourceLocation,
    },

    /// Attempting to access deallocated resource
    UseAfterFree {
        resource_name: String,
        deallocation_site: SourceLocation,
        use_site: SourceLocation,
    },
}

/// Source location information for accurate error reporting
/// Extended resource state for tracking consumption and memory
#[derive(Debug, Clone, PartialEq)]
pub enum ExtendedResourceState {
    Available,
    Consumed,
    Deallocated,  // NEW: Actually freed from memory
}

/// Backtracking checkpoint for resource states
#[derive(Debug, Clone)]
pub struct ResourceCheckpoint {
    resource_states: HashMap<usize, ExtendedResourceState>,
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

/// COMPILE-TIME: Linear resource usage analysis with source location tracking
#[derive(Debug, Clone)]
pub struct LinearUsageAnalysis {
    /// Resources that must be consumed with their declaration locations
    required_consumption: HashMap<String, SourceLocation>,
    /// Resources that have been consumed with their consumption locations
    consumed_resources: HashMap<String, SourceLocation>,
    /// Variables that have been used (for multiple-use detection)
    used_variables: HashMap<String, SourceLocation>,  // var_name -> first_use_location
    /// Memory allocations that need cleanup
    pending_deallocations: Vec<String>,
    /// Resources that are optional (don't need to be consumed)
    optional_resources: HashSet<String>,
}

impl LinearUsageAnalysis {
    pub fn new() -> Self {
        Self {
            required_consumption: HashMap::new(),
            consumed_resources: HashMap::new(),
            used_variables: HashMap::new(),
            pending_deallocations: Vec::new(),
            optional_resources: HashSet::new(),
        }
    }

    /// Mark a resource as requiring consumption with source location
    pub fn require_consumption(&mut self, resource_name: String, location: SourceLocation) {
        self.required_consumption.insert(resource_name, location);
    }

    /// Mark a resource as consumed with source location
    pub fn consume_resource(&mut self, resource_name: String, location: SourceLocation) -> Result<(), LinearError> {
        if !self.required_consumption.contains_key(&resource_name) {
            return Err(LinearError::UnconsumedResource {
                resource_name: resource_name.clone(),
                location,
            });
        }
        self.consumed_resources.insert(resource_name, location);
        Ok(())
    }

    /// Mark a resource as optional (doesn't need to be consumed)
    pub fn mark_resource_optional(&mut self, resource_name: String) {
        self.optional_resources.insert(resource_name);
    }

    /// Check for linear variable reuse with source locations
    pub fn use_variable(&mut self, var_name: String, location: SourceLocation) -> Result<(), LinearError> {
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

    /// Get unconsumed resources with their declaration locations
    pub fn get_unconsumed_resources_with_locations(&self) -> Vec<(String, SourceLocation)> {
        self.required_consumption
            .iter()
            .filter(|(name, _)| !self.consumed_resources.contains_key(*name) && !self.optional_resources.contains(*name))
            .map(|(name, loc)| (name.clone(), loc.clone()))
            .collect()
    }

    /// Get unconsumed resources for backwards compatibility
    pub fn get_unconsumed_resources(&self) -> Vec<String> {
        self.get_unconsumed_resources_with_locations()
            .into_iter()
            .map(|(name, _)| name)
            .collect()
    }

    /// Check if all resources are properly consumed with detailed location info
    pub fn validate_complete_consumption(&self) -> Result<(), Vec<LinearError>> {
        let unconsumed = self.get_unconsumed_resources_with_locations();
        if unconsumed.is_empty() {
            Ok(())
        } else {
            let errors = unconsumed.into_iter().map(|(resource, location)| {
                LinearError::UnconsumedResource {
                    resource_name: resource,
                    location,
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

/// Enhanced linear resource with extended state tracking
#[derive(Debug, Clone)]
pub struct EnhancedLinearResource {
    /// All available resources with unique IDs
    pub id: usize,
    pub clause: Clause,
    pub state: ExtendedResourceState,
    pub memory_allocation: Option<MemoryAllocation>,
}

/// Linear resource manager for proper linear logic semantics with automatic memory management
#[derive(Debug)]
pub struct LinearResourceManager {
    /// All available resources with unique IDs
    resources: Vec<EnhancedLinearResource>,
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
        let location = SourceLocation::new(
            "current_file.fl".to_string(), // Would be actual file in real implementation
            1, // Would be actual line number from parser
            1, // Would be actual column from parser  
            predicate.len()
        );
        
        let linear_info = LinearResourceInfo::new()
            .with_produces(vec![predicate.clone()]);
            
        let clause = Clause::Fact { 
            predicate: predicate.clone(), 
            args, 
            persistent: false, 
            optional: false,  // Default to required
            name: None,
            linear_info,
        };
        
        // Track memory allocation
        let allocation = MemoryAllocation {
            resource_id: self.next_id,
            allocated_at: format!("add_fact({}) at {}", predicate, location),
            size_estimate: 64 + predicate.len() * 8, // Rough estimate
        };

        let resource = EnhancedLinearResource {
            id: self.next_id,
            clause,
            state: ExtendedResourceState::Available,
            memory_allocation: Some(allocation.clone()),
        };

        self.memory_allocations.insert(self.next_id, allocation);

        // Compile-time tracking with source location
        self.usage_analysis.require_consumption(predicate, location);

        self.resources.push(resource);
        self.next_id += 1;
    }

    /// Add a linear fact with explicit source location
    pub fn add_fact_with_location(&mut self, predicate: String, args: Vec<Term>, location: SourceLocation) {
        let linear_info = LinearResourceInfo::new()
            .with_produces(vec![predicate.clone()]);
            
        let clause = Clause::Fact { 
            predicate: predicate.clone(), 
            args, 
            persistent: false, 
            optional: false,  // Default to required
            name: None,
            linear_info,
        };
        
        // Track memory allocation
        let allocation = MemoryAllocation {
            resource_id: self.next_id,
            allocated_at: format!("add_fact({}) at {}", predicate, location),
            size_estimate: 64 + predicate.len() * 8,
        };

        let resource = EnhancedLinearResource {
            id: self.next_id,
            clause,
            state: ExtendedResourceState::Available,
            memory_allocation: Some(allocation.clone()),
        };

        self.memory_allocations.insert(self.next_id, allocation);

        // Compile-time tracking with actual source location
        self.usage_analysis.require_consumption(predicate, location);

        self.resources.push(resource);
        self.next_id += 1;
    }

    /// Add a rule (doesn't get consumed, but constrains resource usage)
    pub fn add_rule(&mut self, head: Term, body: Vec<Term>) {
        let linear_info = LinearResourceInfo::new();
        let clause = Clause::Rule { head, body, produces: None, linear_info };
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
        let available = self.resources.iter().filter(|r| r.state == ExtendedResourceState::Available).count();
        let consumed = self.resources.iter().filter(|r| r.state == ExtendedResourceState::Consumed).count();
        (available, consumed)
    }

    /// Get detailed resource counts including deallocated
    pub fn get_detailed_resource_counts(&self) -> (usize, usize, usize) {
        let available = self.resources.iter().filter(|r| r.state == ExtendedResourceState::Available).count();
        let consumed = self.resources.iter().filter(|r| r.state == ExtendedResourceState::Consumed).count();
        let deallocated = self.resources.iter().filter(|r| r.state == ExtendedResourceState::Deallocated).count();
        (available, consumed, deallocated)
    }

    pub fn analyze_resource_flow(&self) -> ResourceFlowAnalysis {
        ResourceFlowAnalysis {
            total_allocations: self.memory_allocations.len(),
            total_deallocations: self.resources.iter()
                .filter(|r| r.state == ExtendedResourceState::Deallocated)
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
                .filter(|r| r.state == ExtendedResourceState::Deallocated)
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

    /// ADVANCED: Check for linear variable usage patterns with source location
    pub fn check_variable_linearity(&mut self, var_name: String, usage_context: &str) -> Result<(), LinearError> {
        let location = SourceLocation::new(
            "current_file.fl".to_string(),
            1, // Would come from actual parser location
            1,
            var_name.len()
        );
        self.usage_analysis.use_variable(var_name, location)
    }

    /// Extract source location from a term (simplified - in full implementation would track parsing locations)
    fn extract_source_location_from_term(&self, _term: &Term, context: &str) -> SourceLocation {
        // In a full implementation, Terms would carry source location information
        // For now, we'll create a location based on context
        SourceLocation::new(
            "current_file.fl".to_string(), // Would be actual file being parsed
            1, // Would be actual line number
            1, // Would be actual column
            context.len() // Rough estimate
        )
    }
    
    /// Extract source location from a clause
    fn extract_source_location_from_clause(&self, clause: &Clause) -> SourceLocation {
        match clause {
            Clause::Fact { predicate, .. } => {
                SourceLocation::new(
                    "current_file.fl".to_string(),
                    1, // In real implementation, this would come from parsing
                    1,
                    predicate.len()
                )
            }
            Clause::Rule { .. } => {
                SourceLocation::new(
                    "current_file.fl".to_string(),
                    1,
                    1,
                    10 // Rough estimate for rule length
                )
            }
        }
    }
}

/// COMPILE-TIME LINEAR ANALYSIS: Main entry point for compile-time checking
pub fn analyze_program_linearity(program: &Program) -> Result<(), Vec<LinearError>> {
    analyze_program_linearity_with_file(program, "current_file.fl")
}

/// COMPILE-TIME LINEAR ANALYSIS: With file name for better error reporting
pub fn analyze_program_linearity_with_file(program: &Program, filename: &str) -> Result<(), Vec<LinearError>> {
    let mut manager = LinearResourceManager::new();
    let mut errors = Vec::new();

    // Analyze each clause in the program
    for (clause_index, clause) in program.clauses.iter().enumerate() {
        if let Err(clause_errors) = analyze_clause_linearity_with_file(&mut manager, clause, filename, clause_index) {
            errors.extend(clause_errors);
        }
    }

    // Analyze queries (they should NOT consume resources)
    for (query_index, query) in program.queries.iter().enumerate() {
        if let Err(query_errors) = analyze_query_linearity_with_file(&mut manager, query, filename, query_index) {
            errors.extend(query_errors);
        }
    }

    // Analyze main goal if it exists (should NOT consume resources)
    if let Some(main_goal) = &program.main_goal {
        if let Err(main_errors) = analyze_query_linearity_with_file(&mut manager, main_goal, filename, 0) {
            errors.extend(main_errors);
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

/// Analyze a single clause for linear resource usage with file and line info
fn analyze_clause_linearity_with_file(
    manager: &mut LinearResourceManager, 
    clause: &Clause, 
    filename: &str, 
    clause_index: usize
) -> Result<(), Vec<LinearError>> {
    match clause {
        Clause::Fact { predicate, args, persistent, optional, linear_info, .. } => {
            if !*persistent {
                // Use actual location from AST if available
                let location = if let Some(ast_loc) = &linear_info.location {
                    ast_loc.clone()
                } else {
                    // Fallback to estimated location - this should be the line of the fact declaration
                    SourceLocation::new(filename.to_string(), clause_index + 1, 1, predicate.len())
                };
                
                // Add the fact as a resource, marking it as optional if needed
                manager.add_fact_with_location(predicate.clone(), args.clone(), location);
                
                // If it's optional, we don't require it to be consumed
                if *optional {
                    // Mark this resource as optional in the usage analysis
                    manager.usage_analysis.mark_resource_optional(predicate.clone());
                }
            }
            Ok(())
        }
        Clause::Rule { head, body, linear_info, .. } => {
            // Check that rule properly consumes linear resources
            if let Err(error) = manager.check_rule_consumption(clause) {
                Err(vec![error])
            } else {
                // For rules, variables can be reused within the same rule context
                // This is normal unification behavior in logic programming
                // We don't need to check for variable reuse within a single rule
                Ok(())
            }
        }
    }
}

/// Analyze a query for linear resource usage (queries should NOT consume resources)
fn analyze_query_linearity_with_file(
    manager: &mut LinearResourceManager,
    query: &Query,
    filename: &str,
    query_index: usize
) -> Result<(), Vec<LinearError>> {
    // Queries are read-only operations and should not consume linear resources
    // We only check for variable usage patterns but don't mark resources as consumed
    let mut var_errors = Vec::new();
    
    for (goal_index, goal) in query.goals.iter().enumerate() {
        if let Err(e) = check_term_variable_usage_for_query_with_location(
            manager, goal, filename, query_index + 1, &format!("query_goal_{}", goal_index)
        ) {
            var_errors.push(e);
        }
    }
    
    if var_errors.is_empty() {
        Ok(())
    } else {
        Err(var_errors)
    }
}

/// Check variable usage in a query term (different from rule terms - no consumption)
fn check_term_variable_usage_for_query_with_location(
    manager: &mut LinearResourceManager, 
    term: &Term, 
    filename: &str,
    line: usize,
    context: &str
) -> Result<(), LinearError> {
    match term {
        Term::Var { name, .. } => {
            // For queries, we don't consume variables, but we still track usage for consistency
            let location = SourceLocation::new(filename.to_string(), line, 1, name.len());
            // Note: We don't call use_variable here since queries don't consume
            Ok(())
        }
        Term::Compound { args, persistent_use, .. } => {
            // For queries, all usage is effectively persistent (non-consuming)
            // But we still need to check argument variables
            for (i, arg) in args.iter().enumerate() {
                check_term_variable_usage_for_query_with_location(
                    manager, arg, filename, line, &format!("{}[{}]", context, i)
                )?;
            }
            Ok(())
        }
        Term::Atom { .. } => {
            // Atoms in queries don't consume resources
            Ok(())
        }
        Term::Clone(_inner) => {
            // Clone in queries is always allowed
            Ok(())
        }
        _ => Ok(()),
    }
}

/// Check variable usage in a term for linearity violations with enhanced location tracking
fn check_term_variable_usage_with_location(
    manager: &mut LinearResourceManager, 
    term: &Term, 
    filename: &str,
    line: usize,
    context: &str
) -> Result<(), LinearError> {
    match term {
        Term::Var { name, .. } => {
            let location = SourceLocation::new(filename.to_string(), line, 1, name.len());
            manager.usage_analysis.use_variable(name.clone(), location)
        }
        Term::Compound { args, persistent_use, .. } => {
            // If this is persistent use (!predicate), don't consume the resource
            if !persistent_use {
                for (i, arg) in args.iter().enumerate() {
                    check_term_variable_usage_with_location(
                        manager, arg, filename, line, &format!("{}[{}]", context, i)
                    )?;
                }
            } else {
                // For persistent use, we still need to check argument variables
                // but we don't consume the resource itself
                for (i, arg) in args.iter().enumerate() {
                    if let Term::Var { .. } = arg {
                        check_term_variable_usage_with_location(
                            manager, arg, filename, line, &format!("{}[{}]", context, i)
                        )?;
                    }
                }
            }
            Ok(())
        }
        Term::Atom { persistent_use, .. } => {
            // Persistent atoms (!atom) don't consume resources
            if !persistent_use {
                // For regular atoms, no variable usage to check
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
    generate_linearity_report_with_file(program, "current_file.fl")
}

/// COMPILE-TIME: Generate a linearity report with file name
pub fn generate_linearity_report_with_file(program: &Program, filename: &str) -> String {
    let mut manager = LinearResourceManager::new();
    let mut report = String::new();
    
    report.push_str("=== LINEAR RESOURCE ANALYSIS REPORT ===\n");
    
    // Analyze the program
    for (clause_index, clause) in program.clauses.iter().enumerate() {
        let _ = analyze_clause_linearity_with_file(&mut manager, clause, filename, clause_index);
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
