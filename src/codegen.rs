use crate::ast::*;
use std::collections::HashMap;
use std::fmt::Write;

pub struct CodeGenerator {
    /// Counter for generating unique variable names
    var_counter: usize,
    /// Counter for generating unique label names
    label_counter: usize,
    /// Function definitions
    functions: HashMap<String, Function>,
    /// Generated C code
    output: String,
    /// Enable debug output
    debug: bool,
}

impl CodeGenerator {
    pub fn new() -> Self {
        Self {
            var_counter: 0,
            label_counter: 0,
            functions: HashMap::new(),
            output: String::new(),
            debug: false,
        }
    }
    
    pub fn with_debug(mut self, debug: bool) -> Self {
        self.debug = debug;
        self
    }
    
    fn fresh_var(&mut self) -> String {
        let var = format!("v{}", self.var_counter);
        self.var_counter += 1;
        var
    }
    
    fn fresh_label(&mut self) -> String {
        let label = format!("L{}", self.label_counter);
        self.label_counter += 1;
        label
    }
    
    pub fn generate_unique_var(&mut self, prefix: &str) -> String {
        let var = format!("{}_{}", prefix, self.var_counter);
        self.var_counter += 1;
        var
    }
    
    pub fn generate(&mut self, program: &Program) -> Result<String, std::fmt::Error> {
        self.output.clear();
        
        // Generate C header
        self.generate_header()?;
        
        // Store function definitions
        for func in &program.functions {
            self.functions.insert(func.name.clone(), func.clone());
        }
        
        // Generate function declarations
        for func in &program.functions {
            self.generate_function_declaration(func)?;
        }
        
        // Always generate logical runtime for logic programs
        self.generate_logical_runtime()?;
        
        // Generate logical programming structures
        if !program.queries.is_empty() {
            // When we have queries, we'll generate clauses inline in the main function
            // so we just need the basic KB structure
            self.generate_empty_kb()?;
        } else if !program.clauses.is_empty() {
            self.generate_clauses(&program.clauses)?;
        } else {
            // Even with no clauses, we need the basic KB structure
            self.generate_empty_kb()?;
        }
        
        // Generate function definitions
        for func in &program.functions {
            self.generate_function_definition(func)?;
        }
        
        // Generate main function
        if let Some(ref main_expr) = program.main {
            self.generate_main(main_expr)?;
        } else if !program.queries.is_empty() {
            // Use the multiple queries version which we'll need to implement
            self.generate_multiple_queries_main(&program.clauses, &program.queries, &program.term_types, &program.type_definitions)?;
        } else {
            // Default main for logic programs
            self.generate_logic_main(&program.clauses)?;
        }
        
        Ok(self.output.clone())
    }
    
    fn generate_header(&mut self) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "#include <stdio.h>")?;
        writeln!(self.output, "#include <stdlib.h>")?;
        writeln!(self.output, "#include <stdint.h>")?;
        writeln!(self.output, "#include <string.h>")?;
        writeln!(self.output, "#include \"runtime.h\"")?;
        writeln!(self.output)?;
        Ok(())
    }
    
    fn generate_function_declaration(&mut self, func: &Function) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "int64_t {}(int64_t {});", func.name, func.param)?;
        Ok(())
    }
    
    fn generate_function_definition(&mut self, func: &Function) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "int64_t {}(int64_t {}) {{", func.name, func.param)?;
        
        // Generate function body
        let mut env = HashMap::new();
        env.insert(func.param.clone(), func.param.clone());
        
        let result_var = self.generate_expr(&func.body, &mut env)?;
        writeln!(self.output, "    return {};", result_var)?;
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }
    
    fn generate_main(&mut self, expr: &Expr) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "int main() {{")?;
        
        let mut env = HashMap::new();
        let result_var = self.generate_expr(expr, &mut env)?;
        
        writeln!(self.output, "    printf(\"%lld\\n\", {});", result_var)?;
        writeln!(self.output, "    return 0;")?;
        writeln!(self.output, "}}")?;
        
        Ok(())
    }
    
    fn generate_expr(&mut self, expr: &Expr, env: &mut HashMap<String, String>) -> Result<String, std::fmt::Error> {
        match expr {
            Expr::Var(name) => {
                Ok(env.get(name).cloned().unwrap_or_else(|| name.clone()))
            }
            
            Expr::Int(n) => Ok(n.to_string()),
            
            Expr::Str(s) => {
                // For simplicity, we'll just use the string as a comment
                // In a real implementation, you'd want proper string handling
                Ok(format!("/* {} */ 0", s))
            }
            
            Expr::App { func, args } => {
                if args.len() != 1 {
                    return Ok("0".to_string()); // Error case
                }
                
                let arg_var = self.generate_expr(&args[0], env)?;
                let result_var = self.fresh_var();
                
                writeln!(self.output, "    int64_t {} = {}({});", result_var, func, arg_var)?;
                Ok(result_var)
            }
            
            Expr::Lambda { param, body } => {
                // Lambda expressions are compiled as anonymous functions
                // For simplicity, we'll generate them inline
                let lambda_name = format!("lambda_{}", self.var_counter);
                self.var_counter += 1;
                
                // Generate lambda as a separate function
                writeln!(self.output, "int64_t {}(int64_t {}) {{", lambda_name, param)?;
                
                let mut lambda_env = env.clone();
                lambda_env.insert(param.clone(), param.clone());
                
                let result_var = self.generate_expr(body, &mut lambda_env)?;
                writeln!(self.output, "    return {};", result_var)?;
                writeln!(self.output, "}}")?;
                
                Ok(lambda_name)
            }
            
            Expr::Let { var, value, body } => {
                let value_var = self.generate_expr(value, env)?;
                let var_name = self.fresh_var();
                
                writeln!(self.output, "    int64_t {} = {};", var_name, value_var)?;
                
                env.insert(var.clone(), var_name);
                self.generate_expr(body, env)
            }
            
            Expr::If { cond, then_branch, else_branch } => {
                let cond_var = self.generate_expr(cond, env)?;
                let result_var = self.fresh_var();
                let then_label = self.fresh_label();
                let else_label = self.fresh_label();
                let end_label = self.fresh_label();
                
                writeln!(self.output, "    int64_t {};", result_var)?;
                writeln!(self.output, "    if ({}) goto {};", cond_var, then_label)?;
                writeln!(self.output, "    goto {};", else_label)?;
                
                writeln!(self.output, "{}:", then_label)?;
                let then_var = self.generate_expr(then_branch, env)?;
                writeln!(self.output, "    {} = {};", result_var, then_var)?;
                writeln!(self.output, "    goto {};", end_label)?;
                
                writeln!(self.output, "{}:", else_label)?;
                let else_var = self.generate_expr(else_branch, env)?;
                writeln!(self.output, "    {} = {};", result_var, else_var)?;
                
                writeln!(self.output, "{}:", end_label)?;
                Ok(result_var)
            }
            
            Expr::Pair(left, right) => {
                let left_var = self.generate_expr(left, env)?;
                let right_var = self.generate_expr(right, env)?;
                let result_var = self.fresh_var();
                
                writeln!(self.output, "    pair_t {} = {{ {}, {} }};", result_var, left_var, right_var)?;
                Ok(result_var)
            }
            
            Expr::MatchPair { expr, left_var, right_var, body } => {
                let expr_var = self.generate_expr(expr, env)?;
                let left_c_var = self.fresh_var();
                let right_c_var = self.fresh_var();
                
                writeln!(self.output, "    int64_t {} = {}.first;", left_c_var, expr_var)?;
                writeln!(self.output, "    int64_t {} = {}.second;", right_c_var, expr_var)?;
                
                env.insert(left_var.clone(), left_c_var);
                env.insert(right_var.clone(), right_c_var);
                
                self.generate_expr(body, env)
            }
            
            Expr::Alloc { size } => {
                let size_var = self.generate_expr(size, env)?;
                let result_var = self.fresh_var();
                
                writeln!(self.output, "    linear_ptr_t {} = linear_alloc({});", result_var, size_var)?;
                Ok(result_var)
            }
            
            Expr::Free { ptr } => {
                let ptr_var = self.generate_expr(ptr, env)?;
                writeln!(self.output, "    linear_free({});", ptr_var)?;
                Ok("0".to_string()) // Unit type represented as 0
            }
            
            Expr::Load { ptr } => {
                let ptr_var = self.generate_expr(ptr, env)?;
                let result_var = self.fresh_var();
                
                writeln!(self.output, "    int64_t {} = linear_load({});", result_var, ptr_var)?;
                Ok(result_var)
            }
            
            Expr::Store { ptr, value } => {
                let ptr_var = self.generate_expr(ptr, env)?;
                let value_var = self.generate_expr(value, env)?;
                
                writeln!(self.output, "    linear_store({}, {});", ptr_var, value_var)?;
                Ok("0".to_string()) // Unit type represented as 0
            }
            
            Expr::BinOp { op, left, right } => {
                let left_var = self.generate_expr(left, env)?;
                let right_var = self.generate_expr(right, env)?;
                let result_var = self.fresh_var();
                
                let op_str = match op {
                    BinOpKind::Add => "+",
                    BinOpKind::Sub => "-",
                    BinOpKind::Mul => "*",
                    BinOpKind::Div => "/",
                    BinOpKind::Eq => "==",
                    BinOpKind::Lt => "<",
                    BinOpKind::Gt => ">",
                    BinOpKind::And => "&&",
                    BinOpKind::Or => "||",
                };
                
                writeln!(self.output, "    int64_t {} = {} {} {};", result_var, left_var, op_str, right_var)?;
                Ok(result_var)
            }
        }
    }
    
    fn generate_logical_runtime(&mut self) -> Result<(), std::fmt::Error> {
        // No need to redefine types - they're in the header
        writeln!(self.output, "// Logical Programming Runtime - types defined in header")?;
        writeln!(self.output)?;
        
        Ok(())
    }
    
    fn generate_empty_kb(&mut self) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "// Linear Knowledge Base (empty)")?;
        writeln!(self.output, "linear_kb_t* kb;")?;
        writeln!(self.output)?;
        
        writeln!(self.output, "void initialize_kb() {{")?;
        writeln!(self.output, "    kb = create_linear_kb();")?;
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }
    
    fn generate_clauses(&mut self, clauses: &[Clause]) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "// Linear Knowledge Base")?;
        writeln!(self.output, "linear_kb_t* kb;")?;
        writeln!(self.output)?;
        
        writeln!(self.output, "void initialize_kb() {{")?;
        writeln!(self.output, "    kb = create_linear_kb();")?;
        writeln!(self.output)?;
        
        for (i, clause) in clauses.iter().enumerate() {
            match clause {
                Clause::Fact { predicate, args, persistent, .. } => {
                    writeln!(self.output, "    // Linear Fact: {}({})", 
                        predicate, 
                        args.iter().map(|_| "_").collect::<Vec<_>>().join(", "))?;
                    
                    // Generate code to create the fact
                    let args_code = self.generate_term_array(args)?;
                    writeln!(self.output, "    term_t** args_{} = {}; ", i, args_code)?;
                    writeln!(self.output, "    term_t* fact_{} = create_compound(\"{}\", args_{}, {});", 
                        i, predicate, i, args.len())?;
                    writeln!(self.output, "    add_linear_fact(kb, fact_{});", i)?;
                    writeln!(self.output, "    printf(\"Added linear fact: \");");
                    writeln!(self.output, "    print_term(fact_{});", i)?;
                    writeln!(self.output, "    printf(\"\\n\");")?;
                    writeln!(self.output)?;
                }
                
                Clause::Rule { head, body, produces, .. } => {
                    writeln!(self.output, "    // Rule: {} :- {}", 
                        self.term_to_string(head), 
                        body.iter().map(|t| self.term_to_string(t)).collect::<Vec<_>>().join(", "))?;
                    
                    // Generate head
                    let head_code = self.generate_term_creation(head)?;
                    writeln!(self.output, "    term_t* rule_head_{} = {};", i, head_code)?;
                    
                    // Generate body
                    let body_code = self.generate_term_array(body)?;
                    writeln!(self.output, "    term_t** rule_body_{} = {};", i, body_code)?;
                    writeln!(self.output, "    add_rule(kb, rule_head_{}, rule_body_{}, {});", 
                        i, i, body.len())?;
                    writeln!(self.output, "    printf(\"Added rule: \");");
                    writeln!(self.output, "    print_term(rule_head_{});", i)?;
                    writeln!(self.output, "    printf(\" :- \");")?;
                    for j in 0..body.len() {
                        if j > 0 {
                            writeln!(self.output, "    printf(\", \");")?;
                        }
                        writeln!(self.output, "    print_term(rule_body_{}[{}]);", i, j)?;
                    }
                    writeln!(self.output, "    printf(\"\\n\");")?;
                    writeln!(self.output)?;
                }
            }
            writeln!(self.output)?;
        }
        
        writeln!(self.output, "}}")?;
        writeln!(self.output)?;
        
        Ok(())
    }

    fn generate_term_creation(&mut self, term: &Term) -> Result<String, std::fmt::Error> {
        match term {
            Term::Atom { name, .. } => Ok(format!("create_atom(\"{}\")", name)),
            Term::Var { name, .. } => Ok(format!("create_var(\"${}\")", name)),
            Term::Integer(value) => Ok(format!("create_integer({})", value)),
            Term::Compound { functor, args } => {
                if args.is_empty() {
                    Ok(format!("create_compound(\"{}\", NULL, 0)", functor))
                } else {
                    let args_var = format!("args_{}", self.var_counter);
                    self.var_counter += 1;
                    
                    writeln!(self.output, "    term_t* {}[{}];", args_var, args.len())?;
                    for (i, arg) in args.iter().enumerate() {
                        let arg_code = self.generate_term_creation(arg)?;
                        writeln!(self.output, "    {}[{}] = {};", args_var, i, arg_code)?;
                    }
                    
                    Ok(format!("create_compound(\"{}\", {}, {})", functor, args_var, args.len()))
                }
            }
            Term::Clone(inner) => {
                // For C generation, we'll create a cloned term using the runtime function
                let inner_code = self.generate_term_creation(inner)?;
                Ok(format!("create_clone({})", inner_code))
            }
        }
    }
    
    fn generate_term_array(&mut self, terms: &[Term]) -> Result<String, std::fmt::Error> {
        if terms.is_empty() {
            return Ok("NULL".to_string());
        }
        
        let array_var = format!("term_array_{}", self.var_counter);
        self.var_counter += 1;
        
        writeln!(self.output, "    term_t** {} = malloc(sizeof(term_t*) * {});", array_var, terms.len())?;
        for (i, term) in terms.iter().enumerate() {
            let term_code = self.generate_term_creation(term)?;
            writeln!(self.output, "    {}[{}] = {};", array_var, i, term_code)?;
        }
        
        Ok(array_var)
    }
    
    fn term_to_string(&self, term: &Term) -> String {
        match term {
            Term::Atom { name, .. } => name.clone(),
            Term::Var { name, .. } => name.clone(),
            Term::Integer(value) => value.to_string(),
            Term::Compound { functor, args } => {
                format!("{}({})", functor, 
                    args.iter().map(|t| self.term_to_string(t)).collect::<Vec<_>>().join(", "))
            }
            Term::Clone(inner) => {
                format!("!{}", self.term_to_string(inner))
            }
        }
    }
    
    fn generate_query_main(&mut self, clauses: &[Clause], query: &Query) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "int main() {{")?;
        writeln!(self.output, "    initialize_kb();")?;
        writeln!(self.output, "    printf(\"\\n=== LINEAR LOGIC QUERY RESOLUTION ===\\n\");")?;
        
        // Generate query resolution using linear logic
        writeln!(self.output, "    term_t* goals[{}];", query.goals.len())?;
        for (i, goal) in query.goals.iter().enumerate() {
            let goal_code = self.generate_term_creation(goal)?;
            writeln!(self.output, "    goals[{}] = {};", i, goal_code)?;
            writeln!(self.output, "    printf(\"Goal {}: \");", i)?;
            writeln!(self.output, "    print_term(goals[{}]);", i)?;
            writeln!(self.output, "    printf(\"\\n\");")?;
        }
        
        writeln!(self.output, "    printf(\"\\nStarting linear resolution...\\n\");")?;
        writeln!(self.output, "    int success = linear_resolve_query(kb, goals, {});", query.goals.len())?;
        writeln!(self.output)?;
        writeln!(self.output, "    if (success) {{")?;
        writeln!(self.output, "        printf(\"\\n=== QUERY SUCCEEDED ===\\n\");")?;
        writeln!(self.output, "    }} else {{")?;
        writeln!(self.output, "        printf(\"\\n=== QUERY FAILED ===\\n\");")?;
        writeln!(self.output, "    }}")?;
        writeln!(self.output)?;
        writeln!(self.output, "    // Clean up")?;
        writeln!(self.output, "    free_linear_kb(kb);")?;
        writeln!(self.output, "    return success ? 0 : 1;")?;
        writeln!(self.output, "}}")?;
        
        Ok(())
    }
    
    fn generate_logic_main(&mut self, clauses: &[Clause]) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "int main() {{")?;
        writeln!(self.output, "    initialize_kb();")?;
        writeln!(self.output, "    printf(\"\\n=== LINEAR LOGIC KNOWLEDGE BASE ===\\n\");")?;
        writeln!(self.output, "    printf(\"Linear knowledge base initialized with {} clauses\\n\");", clauses.len())?;
        writeln!(self.output, "    printf(\"Ready for linear logic queries.\\n\");")?;
        writeln!(self.output)?;
        writeln!(self.output, "    // Clean up")?;
        writeln!(self.output, "    free_linear_kb(kb);")?;
        writeln!(self.output, "    return 0;")?;
        writeln!(self.output, "}}")?;
        
        Ok(())
    }

    fn generate_multiple_queries_main(&mut self, clauses: &[Clause], queries: &[Query], term_types: &[TermType], type_definitions: &[TypeDefinition]) -> Result<(), std::fmt::Error> {
        writeln!(self.output, "int main() {{")?;
        writeln!(self.output, "    // Initialize linear knowledge base")?;
        writeln!(self.output, "    kb = create_linear_kb();")?;
        writeln!(self.output)?;
        
        // Add union type hierarchy mappings
        for type_def in type_definitions {
            self.generate_union_hierarchy_mappings(type_def)?;
        }
        
        // Add type mappings for all terms
        for term_type in term_types {
            if let LogicType::Named(type_name) = &term_type.term_type {
                writeln!(self.output, "    add_type_mapping(kb, \"{}\", \"{}\");", term_type.name, type_name)?;
            }
        }
        writeln!(self.output)?;

        // Add all clauses to the knowledge base
        for clause in clauses {
            match clause {
                Clause::Fact { predicate, args, persistent, .. } => {
                    // Create appropriate term for the fact
                    let mut fact_term = if args.is_empty() {
                        // Nullary fact: create an atom
                        Term::Atom { 
                            name: predicate.clone(),
                            type_name: None,
                        }
                    } else {
                        // Regular fact: create a compound term
                        Term::Compound { 
                            functor: predicate.clone(), 
                            args: args.clone() 
                        }
                    };
                    
                    // If persistent, wrap in clone
                    if *persistent {
                        fact_term = Term::Clone(Box::new(fact_term));
                    }
                    
                    let term_creation = self.generate_term_creation(&fact_term)?;
                    writeln!(self.output, "    add_linear_fact(kb, {});", term_creation)?;
                }
                Clause::Rule { head, body, produces } => {
                    // Generate code to add the rule to the knowledge base
                    let head_code = self.generate_term_creation(head)?;
                    if body.is_empty() {
                        // Rule with no body - treat as a fact
                        writeln!(self.output, "    add_linear_fact(kb, {});", head_code)?;
                    } else {
                        // Generate body array
                        let body_array_var = self.generate_unique_var("body_array");
                        writeln!(self.output, "    term_t** {} = malloc(sizeof(term_t*) * {});", body_array_var, body.len())?;
                        for (i, body_term) in body.iter().enumerate() {
                            let body_code = self.generate_term_creation(body_term)?;
                            writeln!(self.output, "    {}[{}] = {};", body_array_var, i, body_code)?;
                        }
                        
                        // Handle production (if any)
                        let production_code = if let Some(prod_term) = produces {
                            self.generate_term_creation(prod_term)?
                        } else {
                            "NULL".to_string()
                        };
                        
                        writeln!(self.output, "    add_rule(kb, {}, {}, {}, {});", head_code, body_array_var, body.len(), production_code)?;
                    }
                }
            }
        }
        
        writeln!(self.output)?;
        
        // Execute each query
        for (query_index, query) in queries.iter().enumerate() {
            if query_index > 0 {
                writeln!(self.output)?; // Add blank line between queries
            }
            
            // Generate query term and print it
            writeln!(self.output, "    // Query {}: ", query_index + 1)?;
            writeln!(self.output, "    printf(\"?- \");")?;
            for (i, goal) in query.goals.iter().enumerate() {
                if i > 0 {
                    if query.is_disjunctive {
                        writeln!(self.output, "    printf(\" | \");")?;
                    } else {
                        writeln!(self.output, "    printf(\" & \");")?;
                    }
                }
                let goal_code = self.generate_term_creation(goal)?;
                writeln!(self.output, "    print_term({});", goal_code)?;
            }
            writeln!(self.output, "    printf(\".\\n\");")?;
            
            // Create goals array
            let goals_var = format!("goals_{}", query_index);
            writeln!(self.output, "    term_t** {} = malloc({} * sizeof(term_t*));", goals_var, query.goals.len())?;
            for (i, goal) in query.goals.iter().enumerate() {
                let goal_code = self.generate_term_creation(goal)?;
                writeln!(self.output, "    {}[{}] = {};", goals_var, i, goal_code)?;
            }
            
            // Execute the query with backtracking to find all solutions
            writeln!(self.output, "    solution_list_t* solutions_{} = create_solution_list();", query_index)?;
            writeln!(self.output, "    int found_solutions_{} = linear_resolve_query_all_solutions(kb, {}, {}, solutions_{});", 
                     query_index, goals_var, query.goals.len(), query_index)?;
            writeln!(self.output, "    if (solutions_{}->count > 0) {{", query_index)?;
            writeln!(self.output, "        printf(\"true (%d solution%s found).\\n\", solutions_{}->count, solutions_{}->count == 1 ? \"\" : \"s\");", query_index, query_index)?;
            writeln!(self.output, "    }} else {{")?;
            writeln!(self.output, "        printf(\"false.\\n\");")?;
            writeln!(self.output, "    }}")?;
            writeln!(self.output, "    free_solution_list(solutions_{});", query_index)?;
            
            // Clean up
            writeln!(self.output, "    for (int i = 0; i < {}; i++) {{", query.goals.len())?;
            writeln!(self.output, "        free({}[i]);", goals_var)?;
            writeln!(self.output, "    }}")?;
            writeln!(self.output, "    free({});", goals_var)?;
        }
        
        writeln!(self.output)?;
        writeln!(self.output, "    // Clean up")?;
        writeln!(self.output, "    free_linear_kb(kb);")?;
        writeln!(self.output, "    return 0;  // Always return success - false is a valid result")?;
        writeln!(self.output, "}}")?;
        
        Ok(())
    }
    
    fn generate_union_hierarchy_mappings(&mut self, type_def: &TypeDefinition) -> Result<(), std::fmt::Error> {
        if let Some(ref union_variants) = type_def.union_variants {
            self.generate_variant_mappings(&type_def.name, union_variants)?;
        }
        Ok(())
    }
    
    fn generate_variant_mappings(&mut self, parent_type: &str, variants: &UnionVariants) -> Result<(), std::fmt::Error> {
        match variants {
            UnionVariants::Simple(names) => {
                for name in names {
                    writeln!(self.output, "    add_union_mapping(kb, \"{}\", \"{}\");", name, parent_type)?;
                }
            }
            UnionVariants::Nested(variant_list) => {
                for variant in variant_list {
                    // Map the variant to the parent type
                    writeln!(self.output, "    add_union_mapping(kb, \"{}\", \"{}\");", variant.name, parent_type)?;
                    
                    // If this variant has sub-variants, recursively map them
                    if let Some(ref sub_variants) = variant.sub_variants {
                        self.generate_variant_mappings(&variant.name, sub_variants)?;
                    }
                }
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::tokenize;
    use crate::parser::Parser;
    
    #[test]
    fn test_simple_codegen() {
        let tokens = tokenize("let x = 42 in x + 1").unwrap();
        let mut parser = Parser::new(tokens);
        let program = parser.parse_program().unwrap();
        
        let mut codegen = CodeGenerator::new();
        let result = codegen.generate(&program);
        assert!(result.is_ok());
        
        let code = result.unwrap();
        assert!(code.contains("int main()"));
        assert!(code.contains("printf"));
    }
}
