mod ast;
mod lexer;
mod parser;
mod typechecker;
mod codegen;
mod unification;
mod diagnostic;

use clap::{Parser, Subcommand};
use std::fs;
use std::path::PathBuf;
use std::process::Command;

#[derive(Parser)]
#[command(name = "linear-logic")]
#[command(about = "A compiler for linear logic programming language")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Compile a linear logic source file to C
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
    /// Run a linear logic program directly
    Run {
        /// Input source file
        input: PathBuf,
        /// Enable debug output
        #[arg(long)]
        debug: bool,
    },
    /// Check syntax and types without generating code
    Check {
        /// Input source file
        input: PathBuf,
        /// Enable debug output
        #[arg(long)]
        debug: bool,
    },
}

fn main() {
    let cli = Cli::parse();
    
    match cli.command {
        Commands::Compile { input, output, executable, debug } => {
            match compile_file(input, output, executable, debug) {
                Ok(output_path) => {
                    println!("Successfully compiled to: {}", output_path.display());
                }
                Err(e) => {
                    eprintln!("Compilation failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
        Commands::Run { input, debug } => {
            match run_file(input, debug) {
                Ok(()) => {}
                Err(e) => {
                    eprintln!("Execution failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
        Commands::Check { input, debug } => {
            match check_file(input, debug) {
                Ok(()) => {
                    println!("Program is well-typed");
                }
                Err(e) => {
                    eprintln!("Type check failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
    }
}

fn compile_file(input: PathBuf, output: Option<PathBuf>, executable: bool, debug: bool) -> Result<PathBuf, Box<dyn std::error::Error>> {
    // Read source file
    let source = fs::read_to_string(&input)?;
    
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
    let mut parser = parser::Parser::new(lex_result.tokens, input.to_string_lossy().to_string(), source.clone());
    let program = match parser.parse_program() {
        Ok(program) => program,
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
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new();
    let c_code = match codegen.generate(&program) {
        Ok(code) => code,
        Err(e) => {
            let diagnostic = diagnostic::Diagnostic::error(format!("Code generation error: {}", e))
                .with_location(diagnostic::SourceLocation::new(
                    input.to_string_lossy().to_string(),
                    1, 1, 1
                ))
                .with_help("Internal compiler error during code generation".to_string());
            diagnostic.emit();
            std::process::exit(1);
        }
    };
    
    // Determine output path
    let output_path = output.unwrap_or_else(|| {
        let mut path = input.clone();
        path.set_extension("c");
        path
    });
    
    // Write C code
    fs::write(&output_path, c_code)?;
    
    // Optionally compile to executable
    if executable {
        let exe_path = {
            let mut path = output_path.clone();
            path.set_extension("exe");
            path
        };
        
        let status = Command::new(if cfg!(target_os = "macos") { "clang" } else { "gcc" })
            .arg("-std=c99")
            .arg("-Wall")
            .arg("-Wextra")
            .arg("-O2")
            .arg(&output_path)
            .arg("c/linear_runtime.c")
            .arg("-I")
            .arg(".")
            .arg("-o")
            .arg(&exe_path)
            .status()?;
        
        if !status.success() {
            return Err("C compilation failed".into());
        }
        
        return Ok(exe_path);
    }
    
    Ok(output_path)
}

fn run_file(input: PathBuf, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    use std::env;
    
    // Create temporary directory
    let temp_dir = env::temp_dir().join(format!("linear-logic-{}", std::process::id()));
    fs::create_dir_all(&temp_dir)?;
    
    // Ensure cleanup happens even if we panic
    struct TempDirGuard(PathBuf);
    impl Drop for TempDirGuard {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }
    let _guard = TempDirGuard(temp_dir.clone());
    
    // Read and compile the linear logic source
    let source = fs::read_to_string(&input)?;
    
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
    let mut parser = parser::Parser::new(lex_result.tokens, input.to_string_lossy().to_string(), source.clone());
    let program = match parser.parse_program() {
        Ok(program) => program,
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
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new();
    let c_code = match codegen.generate(&program) {
        Ok(code) => code,
        Err(e) => {
            let diagnostic = diagnostic::Diagnostic::error(format!("Code generation error: {}", e))
                .with_location(diagnostic::SourceLocation::new(
                    input.to_string_lossy().to_string(),
                    1, 1, 1
                ))
                .with_help("Internal compiler error during code generation".to_string());
            diagnostic.emit();
            std::process::exit(1);
        }
    };
    
    // Write C code to temp directory
    let c_file = temp_dir.join("program.c");
    fs::write(&c_file, c_code)?;
    
    // Copy runtime files to temp directory
    let runtime_c = temp_dir.join("linear_runtime.c");
    let runtime_h = temp_dir.join("linear_runtime.h");
    fs::copy("c/linear_runtime.c", &runtime_c)?;
    fs::copy("c/linear_runtime.h", &runtime_h)?;
    
    // Compile to executable
    let exe_file = temp_dir.join("program");
    let compiler = if cfg!(target_os = "macos") { "clang" } else { "gcc" };
    
    let status = Command::new(compiler)
        .arg("-std=c99")
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-O2")
        .arg(&c_file)
        .arg(&runtime_c)
        .arg("-I")
        .arg(&temp_dir)
        .arg("-o")
        .arg(&exe_file)
        .status()?;
    
    if !status.success() {
        return Err("C compilation failed".into());
    }
    
    // Run the executable
    let status = Command::new(&exe_file).status()?;
    
    if !status.success() {
        return Err("Program execution failed".into());
    }
    
    // Cleanup happens automatically via the guard
    Ok(())
}

fn check_file(input: PathBuf, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    // Read source file
    let source = fs::read_to_string(&input)?;
    
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
    let mut parser = parser::Parser::new(lex_result.tokens, input.to_string_lossy().to_string(), source.clone());
    let program = match parser.parse_program() {
        Ok(program) => program,
        Err(e) => {
            match e {
                parser::ParseError::Diagnostic(diagnostic) => {
                    diagnostic.emit();
                },
                _ => {
                    let diagnostic = diagnostic::Diagnostic::error(format!("{}", e))
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
    
    println!("{}✓{} Type checking passed!", diagnostic::Colors::GREEN, diagnostic::Colors::RESET);
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;
    
    #[test]
    fn test_compile_simple_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.ll");
        fs::write(&input_path, "let x = 42 in x + 1").unwrap();
        
        let result = compile_file(input_path, None, false);
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_check_valid_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.ll");
        fs::write(&input_path, "let x = 42 in x + 1").unwrap();
        
        let result = check_file(input_path);
        assert!(result.is_ok());
    }
}