mod ast;
mod lexer;
mod parser;
mod typechecker;
mod codegen;
mod diagnostic;
mod resource;

use clap::{Parser, Subcommand};
use std::fs;
use std::path::PathBuf;
use std::process::Command;
use std::env;
use crate::ast::*;

#[derive(Parser)]
#[command(name = "flint")]
#[command(about = "A compiler for linear logic programming language")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Run a program
    Run {
        /// Input source file
        input: PathBuf,
        /// Enable debug output
        #[arg(long)]
        debug: bool,
    },
    /// Compile a program
    Compile {
        /// Input source file
        input: PathBuf,
        /// Output C file
        #[arg(short, long)]
        output: Option<PathBuf>,
        /// Also compile to executable
        #[arg(short, long)]
        executable: bool,
        /// Enable debug output
        #[arg(long)]
        debug: bool,
    },
    /// Check a program
    Check {
        /// Input source file
        input: PathBuf,
        /// Generate detailed linearity report
        #[arg(long)]
        report: bool,
        /// Enable debug output
        #[arg(long)]
        debug: bool,
    },

}

/// Result of the parsing pipeline
struct ParsedProgram {
    program: Program,
    source: String,
    input_path: PathBuf,
}

fn main() {
    let cli = Cli::parse();
    
    let result = match cli.command {
        Commands::Run { input, debug } => {
            run_command(input, debug)
        }
        Commands::Compile { input, output, executable, debug } => {
            compile_command(input, output, executable, debug)
        }
        Commands::Check { input, report, debug } => {
            check_command(input, report, debug)
        }
    };
    
    if let Err(e) = result {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}

/// Full pipeline: lexing, parsing, typechecking, and linear resource analysis
fn parse_program(input: PathBuf, debug: bool) -> Result<ParsedProgram, Box<dyn std::error::Error>> {
    // Read source file
    let source = fs::read_to_string(&input)?;
    
    if debug {
        println!("=== PARSING {} ===", input.display());
    }
    
    // Tokenize
    let lex_result = match lexer::tokenize(&source) {
        Ok(lex_result) => {
            // Report any lexer errors first
            for error in &lex_result.errors {
                let diagnostic = diagnostic::Diagnostic::error(format!("Lexer error: {}", error.message))
                    .with_location(diagnostic::SourceLocation::new(
                        input.to_string_lossy().to_string(),
                        error.line,
                        error.column,
                        error.length,
                    ))
                    .with_source_text(source.clone());
                diagnostic.emit();
            }
            
            // Exit if there were lexer errors
            if lex_result.has_errors() {
                std::process::exit(1);
            }
            
            if debug {
                eprintln!("DEBUG: Tokens:");
                for (i, token) in lex_result.tokens.iter().enumerate() {
                    eprintln!("  {}: {:?}", i, token);
                }
            }
            lex_result
        },
        Err(e) => {
            let diagnostic = diagnostic::Diagnostic::error(format!("Lexer error: {}", e))
                .with_location(diagnostic::SourceLocation::new(
                    input.to_string_lossy().to_string(),
                    1, 1, 1
                ))
                .with_source_text(source.clone())
                .with_help("Check for invalid characters or syntax errors".to_string());
            diagnostic.emit();
            std::process::exit(1);
        }
    };
    
    // Parse
    let mut parser = parser::Parser::new(lex_result.tokens, input.to_string_lossy().to_string(), source.clone()).with_debug(debug);
    let program = match parser.parse_program() {
        Ok(program) => {
            if debug {
                eprintln!("DEBUG: Parsed program:");
                eprintln!("  Type definitions: {:?}", program.type_definitions);
                eprintln!("  Clauses: {:?}", program.clauses);
                eprintln!("  Queries: {:?}", program.queries);
            }
            program
        },
        Err(e) => {
            match e {
                parser::ParseError::Diagnostic(diagnostic) => {
                    diagnostic.emit();
                },
                _ => {
                    let diagnostic = diagnostic::Diagnostic::error(format!("Parse error: {}", e))
                        .with_location(diagnostic::SourceLocation::new(
                            input.to_string_lossy().to_string(),
                            1, 1, 1
                        ))
                        .with_source_text(source.clone())
                        .with_help("Check syntax and ensure all declarations are properly formatted".to_string());
                    diagnostic.emit();
                }
            }
            std::process::exit(1);
        }
    };
    
    // Type check
    let mut type_checker = typechecker::TypeChecker::new();
    if let Err(e) = type_checker.check_program(&program) {
        let diagnostic = diagnostic::Diagnostic::error(format!("Type error: {}", e))
            .with_location(diagnostic::SourceLocation::new(
                input.to_string_lossy().to_string(),
                1, 1, 1
            ))
            .with_help("Ensure all predicates and terms have proper type declarations".to_string());
        diagnostic.emit();
        std::process::exit(1);
    }
    
    if debug {
        eprintln!("DEBUG: Type checking passed");
    }
    
    // COMPILE-TIME LINEAR RESOURCE ANALYSIS
    if debug {
        eprintln!("DEBUG: Performing linear resource analysis...");
    }
    
    // Run comprehensive linearity analysis with actual file name
    let filename = input.to_string_lossy().to_string();
    if let Err(linearity_errors) = resource::analyze_program_linearity_with_file(&program, &filename) {
        eprintln!("Linear resource errors detected:");
        for error in &linearity_errors {
            match error {
                resource::LinearError::UnconsumedResource { resource_name, location } => {
                    let diagnostic = diagnostic::Diagnostic::error(
                        format!("Unconsumed linear resource: '{}'", resource_name)
                    )
                    .with_location(diagnostic::SourceLocation::new(
                        location.file.clone(),
                        location.line,
                        location.column,
                        location.length,
                    ))
                    .with_help(format!("Linear resource '{}' must be consumed exactly once. Either add a rule that uses this resource, or mark the fact as optional with '?' prefix.", resource_name))
                    .with_source_text(source.clone());
                    diagnostic.emit();
                }
                resource::LinearError::MultipleUseWithoutClone { variable, first_use, second_use } => {
                    let diagnostic = diagnostic::Diagnostic::error(
                        format!("Linear variable '{}' used multiple times", variable)
                    )
                    .with_location(diagnostic::SourceLocation::new(
                        second_use.file.clone(),
                        second_use.line,
                        second_use.column,
                        second_use.length,
                    ))
                    .with_help(format!("Linear variable '{}' was first used at '{}' and then again at '{}'. Use the exponential prefix !{} if multiple uses are intended.", variable, first_use, second_use, variable))
                    .with_source_text(source.clone());
                    diagnostic.emit();
                }
                resource::LinearError::UseAfterFree { resource_name, deallocation_site, use_site } => {
                    let diagnostic = diagnostic::Diagnostic::error(
                        format!("Use after free: '{}'", resource_name)
                    )
                    .with_location(diagnostic::SourceLocation::new(
                        use_site.file.clone(),
                        use_site.line,
                        use_site.column,
                        use_site.length,
                    ))
                    .with_help(format!("Resource '{}' was deallocated at '{}' but used again at '{}'", resource_name, deallocation_site, use_site))
                    .with_source_text(source.clone());
                    diagnostic.emit();
                }
                _ => {
                    eprintln!("  - {:?}", error);
                }
            }
        }
        std::process::exit(1);
    }
    
    if debug {
        eprintln!("DEBUG: Linear resource analysis passed - all resources properly consumed");
        // Generate and print linearity report
        let report = resource::generate_linearity_report(&program);
        eprintln!("{}", report);
    }
    
    // Perform linear resource analysis
    let mut resource_manager = resource::LinearResourceManager::new();
    for clause in &program.clauses {
        match clause {
            Clause::Fact { predicate, args, persistent: _, .. } => {
                resource_manager.add_fact(predicate.clone(), args.clone());
            }
            Clause::Rule { head, body, produces, .. } => {
                resource_manager.add_rule(head.clone(), body.clone());
                if debug && produces.is_some() {
                    eprintln!("DEBUG: Rule produces: {:?}", produces);
                }
            }
        }
    }
    
    if debug {
        let (available, _consumed) = resource_manager.get_resource_counts();
        eprintln!("DEBUG: Linear resource analysis complete");
        eprintln!("  Available resources: {}", available);
        eprintln!("  Resource manager initialized");
    }
    
    Ok(ParsedProgram {
        program,
        source,
        input_path: input,
    })
}

/// Run command: compile and execute with full linear logic resource tracking
fn run_command(input: PathBuf, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    let parsed = parse_program(input, debug)?;
    
    // Create temporary directory for compilation
    let temp_dir = env::temp_dir().join(format!("flint-{}", std::process::id()));
    fs::create_dir_all(&temp_dir)?;
    
    // Ensure cleanup happens even if we panic
    struct TempDirGuard(PathBuf);
    impl Drop for TempDirGuard {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }
    let _guard = TempDirGuard(temp_dir.clone());
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new().with_debug(debug);
    let c_code = match codegen.generate(&parsed.program) {
        Ok(code) => code,
        Err(e) => {
            let diagnostic = diagnostic::Diagnostic::error(format!("Code generation error: {}", e))
                .with_location(diagnostic::SourceLocation::new(
                    parsed.input_path.to_string_lossy().to_string(),
                    1, 1, 1
                ))
                .with_help("Internal compiler error during code generation".to_string());
            diagnostic.emit();
            std::process::exit(1);
        }
    };
    
    // Write C code to temp file
    let c_file = temp_dir.join("program.c");
    fs::write(&c_file, c_code)?;
    
    if debug {
        eprintln!("DEBUG: Generated C code written to: {}", c_file.display());
    }
    
    // Compile to executable
    let exe_file = temp_dir.join("program");
    let mut command = Command::new(if cfg!(target_os = "macos") { "clang" } else { "gcc" });
    command
        .arg("-std=c99")
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-O2");
    
    if debug {
        command.arg("-DDEBUG");
    }
    
    let status = command
        .arg(&c_file)
        .arg("runtime/runtime.c")
        .arg("-I")
        .arg("runtime")
        .arg("-o")
        .arg(&exe_file)
        .status()?;
    
    if !status.success() {
        return Err("C compilation failed".into());
    }
    
    if debug {
        eprintln!("DEBUG: Executable compiled to: {}", exe_file.display());
    }
    
    // Execute the program
    let output = Command::new(&exe_file).output()?;
    
    if debug {
        eprintln!("DEBUG: Program output:");
        eprintln!("stdout: {}", String::from_utf8_lossy(&output.stdout));
        eprintln!("stderr: {}", String::from_utf8_lossy(&output.stderr));
        eprintln!("exit code: {}", output.status.code().unwrap_or(-1));
    }
    
    // Print the program output
    print!("{}", String::from_utf8_lossy(&output.stdout));
    eprint!("{}", String::from_utf8_lossy(&output.stderr));
    
    if !output.status.success() {
        std::process::exit(output.status.code().unwrap_or(1));
    }
    
    Ok(())
}

/// Compile command: generate C code and optionally compile to executable
fn compile_command(input: PathBuf, output: Option<PathBuf>, executable: bool, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    let parsed = parse_program(input, debug)?;
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new().with_debug(debug);
    let c_code = match codegen.generate(&parsed.program) {
        Ok(code) => code,
        Err(e) => {
            let diagnostic = diagnostic::Diagnostic::error(format!("Code generation error: {}", e))
                .with_location(diagnostic::SourceLocation::new(
                    parsed.input_path.to_string_lossy().to_string(),
                    1, 1, 1
                ))
                .with_help("Internal compiler error during code generation".to_string());
            diagnostic.emit();
            std::process::exit(1);
        }
    };
    
    // Determine output path
    let output_path = output.unwrap_or_else(|| {
        let mut path = parsed.input_path.clone();
        path.set_extension("c");
        path
    });
    
    // Write C code
    fs::write(&output_path, c_code)?;
    println!("Successfully compiled to: {}", output_path.display());
    
    // Optionally compile to executable
    if executable {
        let exe_path = {
            let mut path = output_path.clone();
            path.set_extension("");
            path
        };
        
        let mut command = Command::new(if cfg!(target_os = "macos") { "clang" } else { "gcc" });
        command
            .arg("-std=c99")
            .arg("-Wall")
            .arg("-Wextra")
            .arg("-O2");
        
        if debug {
            command.arg("-DDEBUG");
        }
        
        let status = command
            .arg(&output_path)
            .arg("runtime/runtime.c")
            .arg("-I")
            .arg("runtime")
            .arg("-o")
            .arg(&exe_path)
            .status()?;
        
        if !status.success() {
            return Err("C compilation failed".into());
        }
        
        println!("Compiled executable: {}", exe_path.display());
    }
    
    Ok(())
}

/// Check command: validate syntax, types, and linear resource usage
fn check_command(input: PathBuf, report: bool, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    let parsed = parse_program(input, debug)?;
    
    if report {
        let report = resource::generate_linearity_report(&parsed.program);
        println!("{}", report);
    }
    
    println!("✓ All checks passed!");
    println!("✓ Type checking passed!");
    println!("✓ Linear resource analysis passed!");
    println!("Program is well-typed and linear resource usage is correct.");
    Ok(())
}



#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn test_parse_simple_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "type person.\njohn :: person.").unwrap();
        
        let result = parse_program(input_path, false);
        assert!(result.is_ok());
    }

    #[test]
    fn test_run_simple_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "type person.\njohn :: person.").unwrap();
        
        let result = run_command(input_path, false);
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_compile_simple_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "type person.\njohn :: person.").unwrap();
        
        let result = compile_command(input_path, None, false, false);
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_check_valid_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "type person.\njohn :: person.").unwrap();
        
        let result = check_command(input_path, false);
        assert!(result.is_ok());
    }
}
