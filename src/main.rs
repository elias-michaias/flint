mod ast;
mod lexer;
mod parser;
mod typechecker;
mod codegen;
mod unification;

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
    },
    /// Run a linear logic program directly
    Run {
        /// Input source file
        input: PathBuf,
    },
    /// Check syntax and types without generating code
    Check {
        /// Input source file
        input: PathBuf,
    },
}

fn main() {
    let cli = Cli::parse();
    
    match cli.command {
        Commands::Compile { input, output, executable } => {
            match compile_file(input, output, executable) {
                Ok(output_path) => {
                    println!("Successfully compiled to: {}", output_path.display());
                }
                Err(e) => {
                    eprintln!("Compilation failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
        Commands::Run { input } => {
            match run_file(input) {
                Ok(()) => {}
                Err(e) => {
                    eprintln!("Execution failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
        Commands::Check { input } => {
            match check_file(input) {
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

fn compile_file(input: PathBuf, output: Option<PathBuf>, executable: bool) -> Result<PathBuf, Box<dyn std::error::Error>> {
    // Read source file
    let source = fs::read_to_string(&input)?;
    
    // Tokenize
    let tokens = lexer::tokenize(&source)?;
    
    // Parse
    let mut parser = parser::Parser::new(tokens);
    let program = parser.parse_program()?;
    
    // Type check
    let mut type_checker = typechecker::TypeChecker::new();
    type_checker.check_program(&program)?;
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new();
    let c_code = codegen.generate(&program)?;
    
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

fn run_file(input: PathBuf) -> Result<(), Box<dyn std::error::Error>> {
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
    let tokens = lexer::tokenize(&source)?;
    
    // Parse
    let mut parser = parser::Parser::new(tokens);
    let program = parser.parse_program()?;
    
    // Type check
    let mut type_checker = typechecker::TypeChecker::new();
    type_checker.check_program(&program)?;
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new();
    let c_code = codegen.generate(&program)?;
    
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

fn check_file(input: PathBuf) -> Result<(), Box<dyn std::error::Error>> {
    // Read source file
    let source = fs::read_to_string(&input)?;
    
    // Tokenize
    let tokens = lexer::tokenize(&source)?;
    
    // Parse
    let mut parser = parser::Parser::new(tokens);
    let program = parser.parse_program()?;
    
    // Type check
    let mut type_checker = typechecker::TypeChecker::new();
    type_checker.check_program(&program)?;
    
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