mod ast;
mod lexer;
mod parser;
mod codegen;
mod diagnostic;
mod typechecking;
mod ir;
mod package;

use clap::{Parser, Subcommand};
use std::fs;
use std::path::PathBuf;
use std::process::Command;
use std::env;
use crate::ast::*;

#[derive(Parser)]
#[command(name = "flint")]
#[command(about = "A compiler for Flint functional logic programming language")]
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
        /// Dump IR to file
        #[arg(long)]
        ir: bool,
    },
    /// Check a program
    Check {
        /// Input source file
        input: PathBuf,
        /// Generate detailed analysis report
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
    ir_program: ir::IRProgram,
    source: String,
    input_path: PathBuf,
}

fn main() {
    let cli = Cli::parse();
    
    let result = match cli.command {
        Commands::Run { input, debug } => {
            run_command(input, debug)
        }
        Commands::Compile { input, output, executable, debug, ir } => {
            compile_command(input, output, executable, debug, ir)
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

/// Full pipeline: lexing, parsing, and code generation
fn parse_program(input: PathBuf, debug: bool) -> Result<ParsedProgram, Box<dyn std::error::Error>> {
    // Read source file
    let source = fs::read_to_string(&input)?;
    
    if debug {
        println!("=== PARSING {} ===", input.display());
    }
    
    // Tokenize
    let lex_result = match lexer::tokenize_with_filename(&source, input.to_string_lossy().to_string()) {
        Ok(lex_result) => {
            // Report any lexer errors first
            for error in &lex_result.errors {
                error.diagnostic.emit();
            }
            
            // Exit if there were lexer errors
            if lex_result.has_errors() {
                std::process::exit(1);
            }
            
            if debug {
                eprintln!("DEBUG: Tokens:");
                for (i, token_span) in lex_result.tokens.iter().enumerate() {
                    eprintln!("  {}: {:?}", i, token_span);
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
    let mut parser = parser::FlintParser::new_with_filename(lex_result.tokens, input.to_string_lossy().to_string());
    let program = match parser.parse_program() {
        Ok(program) => {
            if debug {
                eprintln!("DEBUG: Parsed program:");
                eprintln!("  Type definitions: {:?}", program.types());
                eprintln!("  Function signatures: {:?}", program.function_signatures());
                eprintln!("  Function definitions: {:?}", program.functions());
                eprintln!("  Effect declarations: {:?}", program.effects());
                eprintln!("  Effect handlers: {:?}", program.handlers());
                eprintln!("  C imports: {:?}", program.c_imports());
                eprintln!("  Main function: {:?}", program.main_function());
            }
            program
        },
        Err(e) => {
            match e {
                parser::ParseError::Diagnostic(diagnostic) => {
                    let diagnostic_with_source = diagnostic.with_source_text(source.clone());
                    diagnostic_with_source.emit();
                }
            }
            std::process::exit(1);
        }
    };
    
    // Type checking for the functional logic language
    if debug {
        eprintln!("DEBUG: Starting type checking...");
    }
    
    let mut type_checker = typechecking::TypeChecker::new()
        .with_source(input.to_string_lossy().to_string(), source.clone());
    match type_checker.check_program(&program) {
        Ok(type_env) => {
            if debug {
                eprintln!("DEBUG: Type checking completed successfully");
                eprintln!("DEBUG: Type environment has {} variables and {} functions", 
                         type_env.variables.len(), type_env.functions.len());
            }
        },
        Err(e) => {
            match e {
                typechecking::TypeCheckError::Diagnostic(diagnostic) => {
                    let diagnostic_with_source = diagnostic.with_source_text(source.clone());
                    diagnostic_with_source.emit();
                }
            }
            std::process::exit(1);
        }
    };

    // IR Analysis - build intermediate representation
    if debug {
        eprintln!("DEBUG: Starting IR analysis...");
    }
    
    let mut ir_builder = ir::IRBuilder::new(&program);
    let ir_program = ir_builder.build();
    
    if debug {
        eprintln!("DEBUG: IR analysis completed successfully");
        eprintln!("DEBUG: Found {} functions in IR", ir_program.functions.len());
        eprintln!("DEBUG: Global constraints: {}", ir_program.global_constraints.len());
        eprintln!("DEBUG: Symbol table size: {}", ir_program.symbol_table.len());
        
        // Print detailed IR for each function
        for func in &ir_program.functions {
            eprintln!("=== IR for function '{}' ===", func.name);
            eprintln!("  Parameters: {:?}", func.parameters);
            eprintln!("  Determinism: {:?}", func.determinism);
            eprintln!("  Body: {}", func.body);
            eprintln!("  Constraints:");
            for constraint in &func.constraints {
                eprintln!("    {}", constraint);
            }
            eprintln!("  Binding analysis:");
            for (symbol, status) in &func.binding_analysis {
                eprintln!("    {} -> {:?}", symbol.name, status);
            }
        }
    }
    
    if debug {
        eprintln!("DEBUG: Parsing completed successfully");
    }
    
    // TODO: Add compile-time analysis for the new functional logic language
    // COMPILE-TIME LINEAR RESOURCE ANALYSIS
    // if debug {
    //     eprintln!("DEBUG: Performing linear resource analysis...");
    // }
    
    // // Run comprehensive linearity analysis with actual file name
    // let filename = input.to_string_lossy().to_string();
    // if let Err(linearity_errors) = resource::analyze_program_linearity_with_file(&program, &filename) {
    //     eprintln!("Linear resource errors detected:");
    //     for error in &linearity_errors {
    //         match error {
    //             resource::LinearError::UnconsumedResource { resource_name, location } => {
    //                 let diagnostic = diagnostic::Diagnostic::error(
    //                     format!("Unconsumed linear resource: '{}'", resource_name)
    //                 )
    //                 .with_location(diagnostic::SourceLocation::new(
    //                     location.file.clone(),
    //                     location.line,
    //                     location.column,
    //                     location.length,
    //                 ))
    //                 .with_help(format!("Linear resource '{}' must be consumed exactly once. Either add a rule that uses this resource, or mark the fact as optional with '?' prefix.", resource_name))
    //                 .with_source_text(source.clone());
    //                 diagnostic.emit();
    //             }
    //             resource::LinearError::MultipleUseWithoutClone { variable, first_use, second_use } => {
    //                 let diagnostic = diagnostic::Diagnostic::error(
    //                     format!("Linear variable '{}' used multiple times", variable)
    //                 )
    //                 .with_location(diagnostic::SourceLocation::new(
    //                     second_use.file.clone(),
    //                     second_use.line,
    //                     second_use.column,
    //                     second_use.length,
    //                 ))
    //                 .with_help(format!("Linear variable '{}' was first used at '{}' and then again at '{}'. Use the exponential prefix !{} if multiple uses are intended.", variable, first_use, second_use, variable))
    //                 .with_source_text(source.clone());
    //                 diagnostic.emit();
    //             }
    //             resource::LinearError::UseAfterFree { resource_name, deallocation_site, use_site } => {
    //                 let diagnostic = diagnostic::Diagnostic::error(
    //                     format!("Use after free: '{}'", resource_name)
    //                 )
    //                 .with_location(diagnostic::SourceLocation::new(
    //                     use_site.file.clone(),
    //                     use_site.line,
    //                     use_site.column,
    //                     use_site.length,
    //                 ))
    //                 .with_help(format!("Resource '{}' was deallocated at '{}' but used again at '{}'", resource_name, deallocation_site, use_site))
    //                 .with_source_text(source.clone());
    //                 diagnostic.emit();
    //             }
    //             _ => {
    //                 eprintln!("  - {:?}", error);
    //             }
    //         }
    //     }
    //     std::process::exit(1);
    // }
    
    // if debug {
    //     eprintln!("DEBUG: Linear resource analysis passed - all resources properly consumed");
    //     // Generate and print linearity report
    //     let report = resource::generate_linearity_report(&program);
    //     eprintln!("{}", report);
    // }
    
    // // Perform linear resource analysis
    // let mut resource_manager = resource::LinearResourceManager::new();
    // for clause in &program.clauses {
    //     match clause {
    //         Clause::Fact { predicate, args, persistent: _, .. } => {
    //             resource_manager.add_fact(predicate.clone(), args.clone());
    //         }
    //         Clause::Rule { head, body, produces, .. } => {
    //             resource_manager.add_rule(head.clone(), body.clone());
    //             if debug && produces.is_some() {
    //                 eprintln!("DEBUG: Rule produces: {:?}", produces);
    //             }
    //         }
    //     }
    // }
    
    // if debug {
    //     let (available, _consumed) = resource_manager.get_resource_counts();
    //     eprintln!("DEBUG: Linear resource analysis complete");
    //     eprintln!("  Available resources: {}", available);
    //     eprintln!("  Resource manager initialized");
    // }
    
    Ok(ParsedProgram {
        program,
        ir_program,
        source,
        input_path: input,
    })
}

/// Helper function for cross-platform C compilation with dead code elimination
fn setup_c_compiler(debug: bool) -> Command {
    // Cross-platform compiler detection and configuration
    let (compiler, dead_code_flags) = if cfg!(target_os = "macos") {
        ("clang", vec!["-Wl,-dead_strip"])
    } else if cfg!(target_os = "windows") {
        ("gcc", vec!["-Wl,--gc-sections"])
    } else {
        // Linux and others
        ("gcc", vec!["-Wl,--gc-sections"])
    };
    
    let mut command = Command::new(compiler);
    command
        .arg("-std=c99")
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-O2")
        .arg("-march=native")
        .arg("-ffunction-sections")
        .arg("-fdata-sections");
    
    // Add dead code elimination flags
    for flag in dead_code_flags {
        command.arg(flag);
    }
    
    if debug {
        command.arg("-DDEBUG");
    }
    
    command
}

/// Setup C compiler with Python package linking
fn setup_c_compiler_with_packages(debug: bool, package_manager: &package::PackageManager) -> Command {
    let mut command = setup_c_compiler(debug);
    
    // Add Python package include directories
    for include_dir in package_manager.get_include_dirs() {
        command.arg("-I").arg(&include_dir);
    }
    
    // Add Python package linking flags
    for flag in package_manager.get_linking_flags() {
        command.arg(flag);
    }
    
    command
}

/// Run command: compile and execute functional logic programs
fn run_command(input: PathBuf, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    let parsed = parse_program(input, debug)?;
    
    // Handle package management (Python imports, etc.)
    let project_dir = parsed.input_path.parent()
        .unwrap_or(std::path::Path::new("."))
        .to_path_buf();
    let mut package_manager = package::PackageManager::new(project_dir);
    
    if let Err(diagnostic) = package_manager.process_packages(&parsed.program) {
        diagnostic.emit();
        std::process::exit(1);
    }
    
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
    let mut codegen = codegen::CodeGenerator::new()
        .with_debug(debug)
        .with_package_manager(&package_manager);
    let c_code = match codegen.generate_with_context(&parsed.program, &parsed.input_path.to_string_lossy(), &parsed.source) {
        Ok(code) => code,
        Err(codegen::CodegenError::Diagnostic(diagnostic)) => {
            diagnostic.emit();
            std::process::exit(1);
        }
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
    
    // Get the path to the Flint project root (where runtime/ directory is located)
    // Try to find the project root by looking for runtime/ directory
    let current_dir = std::env::current_dir().unwrap_or_else(|_| std::path::PathBuf::from("."));
    let flint_root = if current_dir.join("runtime").exists() {
        // We're in the project root
        current_dir
    } else if current_dir.parent().map(|p| p.join("runtime").exists()).unwrap_or(false) {
        // We're in a subdirectory (like examples), go up one level
        current_dir.parent().unwrap().to_path_buf()
    } else {
        // Fallback: assume we're relative to the executable
        if let Ok(exe_path) = std::env::current_exe() {
            exe_path.parent()
                .and_then(|p| p.parent())  // Go up from target/debug
                .and_then(|p| p.parent())  // Go up from target to project root
                .unwrap_or(&current_dir)
                .to_path_buf()
        } else {
            current_dir
        }
    };
    
    let runtime_lib = flint_root.join("runtime/out/libflint_runtime.a");
    let runtime_include = flint_root.join("runtime");
    let libdill_include = flint_root.join("runtime/lib/libdill/libdill-install/include");
    
    let status = setup_c_compiler_with_packages(debug, &package_manager)
        .arg(&c_file)
        .arg(&runtime_lib)
        .arg("-I")
        .arg(&runtime_include)
        .arg("-I")
        .arg(&libdill_include)
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
fn compile_command(input: PathBuf, output: Option<PathBuf>, executable: bool, debug: bool, dump_ir: bool) -> Result<(), Box<dyn std::error::Error>> {
    // Check if input is a C file (Flint-generated)
    if input.extension().and_then(|s| s.to_str()) == Some("c") {
        return compile_c_file(input, output, debug);
    }

    let parsed = parse_program(input, debug)?;
    
    // Handle package management (Python imports, etc.)
    let project_dir = parsed.input_path.parent()
        .unwrap_or(std::path::Path::new("."))
        .to_path_buf();
    let mut package_manager = package::PackageManager::new(project_dir);
    
    if let Err(diagnostic) = package_manager.process_packages(&parsed.program) {
        diagnostic.emit();
        std::process::exit(1);
    }
    
    // Dump IR if requested
    if dump_ir {
        let ir_path = {
            let mut path = parsed.input_path.clone();
            let filename = path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("output");
            path.set_file_name(format!("{}.ir", filename));
            path
        };
        
        let ir_content = format!("{}", parsed.ir_program);
        fs::write(&ir_path, ir_content)?;
        println!("IR dumped to: {}", ir_path.display());
    }
    
    // Generate C code
    let mut codegen = codegen::CodeGenerator::new()
        .with_debug(debug)
        .with_package_manager(&package_manager);
    let c_code = match codegen.generate_with_context(&parsed.program, &parsed.input_path.to_string_lossy(), &parsed.source) {
        Ok(code) => code,
        Err(codegen::CodegenError::Diagnostic(diagnostic)) => {
            diagnostic.emit();
            std::process::exit(1);
        }
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
        
        // Get the path to the Flint project root (where runtime/ directory is located)
        // Try to find the project root by looking for runtime/ directory
        let current_dir = std::env::current_dir().unwrap_or_else(|_| std::path::PathBuf::from("."));
        let flint_root = if current_dir.join("runtime").exists() {
            // We're in the project root
            current_dir
        } else if current_dir.parent().map(|p| p.join("runtime").exists()).unwrap_or(false) {
            // We're in a subdirectory (like examples), go up one level
            current_dir.parent().unwrap().to_path_buf()
        } else {
            // Fallback: assume we're relative to the executable
            if let Ok(exe_path) = std::env::current_exe() {
                exe_path.parent()
                    .and_then(|p| p.parent())  // Go up from target/debug
                    .and_then(|p| p.parent())  // Go up from target to project root
                    .unwrap_or(&current_dir)
                    .to_path_buf()
            } else {
                current_dir
            }
        };
        
        let runtime_lib = flint_root.join("runtime/out/libflint_runtime.a");
        let runtime_include = flint_root.join("runtime");
        let libdill_include = flint_root.join("runtime/lib/libdill/libdill-install/include");
        
        let status = setup_c_compiler_with_packages(debug, &package_manager)
            .arg(&output_path)
            .arg(&runtime_lib)
            .arg("-I")
            .arg(&runtime_include)
            .arg("-I")
            .arg(&libdill_include)
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

/// Compile a Flint-generated C file with automatic Python linking detection
fn compile_c_file(input: PathBuf, output: Option<PathBuf>, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    // Read the C file to detect Python imports
    let c_content = fs::read_to_string(&input)?;
    
    // Detect if this C file uses Python imports by looking for Python headers
    let has_python_imports = c_content.contains("#include \".flint/lib/python/") || 
                             c_content.contains("_wrapper.h") ||
                             c_content.contains("#include <Python.h>");
    
    if debug {
        println!("=== COMPILING C FILE {} ===", input.display());
        if has_python_imports {
            println!("Detected Python imports - will add Python linking flags");
        }
    }
    
    // Determine output path
    let output_path = output.unwrap_or_else(|| {
        let mut path = input.clone();
        path.set_extension("");
        path
    });
    
    // Get the path to the Flint project root (where runtime/ directory is located)
    let current_dir = std::env::current_dir().unwrap_or_else(|_| std::path::PathBuf::from("."));
    let flint_root = if current_dir.join("runtime").exists() {
        // We're in the project root
        current_dir
    } else if current_dir.parent().map(|p| p.join("runtime").exists()).unwrap_or(false) {
        // We're in a subdirectory (like examples), go up one level
        current_dir.parent().unwrap().to_path_buf()
    } else {
        // Fallback: assume we're relative to the executable
        if let Ok(exe_path) = std::env::current_exe() {
            exe_path.parent()
                .and_then(|p| p.parent())  // Go up from target/debug
                .and_then(|p| p.parent())  // Go up from target to project root
                .unwrap_or(&current_dir)
                .to_path_buf()
        } else {
            current_dir
        }
    };

    let runtime_lib = flint_root.join("runtime/out/libflint_runtime.a");
    let runtime_include = flint_root.join("runtime");
    let libdill_include = flint_root.join("runtime/lib/libdill/libdill-install/include");
    
    // Start building the compilation command
    let mut cmd = if debug {
        let mut c = Command::new("gcc");
        c.arg("-g").arg("-O0");
        c
    } else {
        let mut c = Command::new("gcc");
        c.arg("-O2");
        c
    };
    
    // Add basic flags
    cmd.arg("-std=c99")
       .arg("-Wall")
       .arg("-Wextra")
       .arg(&input)
       .arg(&runtime_lib)
       .arg("-I")
       .arg(&runtime_include)
       .arg("-I")
       .arg(&libdill_include);
    
    // If Python imports detected, add Python linking flags
    if has_python_imports {
        // Get Python include path using sysconfig
        let python_include = Command::new("python3")
            .arg("-c")
            .arg("import sysconfig; print(sysconfig.get_paths()['include'])")
            .output();
            
        match python_include {
            Ok(output) if output.status.success() => {
                let include_path_string = String::from_utf8_lossy(&output.stdout).to_string();
                let include_path = include_path_string.trim();
                cmd.arg("-I").arg(include_path);
                
                if debug {
                    println!("Added Python include path: {}", include_path);
                }
                
                // Get Python library linking flags
                let python_ldflags = Command::new("python3")
                    .arg("-c")
                    .arg("import sysconfig; print(sysconfig.get_config_var('LDFLAGS') or '')")
                    .output();
                    
                if let Ok(ldflags_output) = python_ldflags {
                    if ldflags_output.status.success() {
                        let ldflags_string = String::from_utf8_lossy(&ldflags_output.stdout).to_string();
                        let ldflags = ldflags_string.trim();
                        if !ldflags.is_empty() {
                            for flag in ldflags.split_whitespace() {
                                cmd.arg(flag);
                            }
                            if debug {
                                println!("Added Python LDFLAGS: {}", ldflags);
                            }
                        }
                    }
                }
                
                // Get Python library path and name
                // First try to determine if we should use -framework Python or direct linking
                let python_libs = Command::new("python3")
                    .arg("-c")
                    .arg("import sysconfig; libdir = sysconfig.get_config_var('LIBDIR'); ldlibrary = sysconfig.get_config_var('LDLIBRARY'); version = sysconfig.get_config_var('VERSION'); print(f'-L{libdir} -lpython{version}')")
                    .output();
                    
                if let Ok(libs_output) = python_libs {
                    if libs_output.status.success() {
                        let libs_string = String::from_utf8_lossy(&libs_output.stdout).to_string();
                        let libs = libs_string.trim();
                        
                        // Test if the library flags work by attempting a simple compile test
                        let test_compile = Command::new("clang")
                            .args(&["-x", "c", "-", "-o", "/dev/null"])
                            .args(libs.split_whitespace())
                            .stdin(std::process::Stdio::piped())
                            .stdout(std::process::Stdio::null())
                            .stderr(std::process::Stdio::null())
                            .spawn();
                            
                        if let Ok(mut child) = test_compile {
                            if let Some(stdin) = child.stdin.as_mut() {
                                use std::io::Write;
                                let _ = stdin.write_all(b"int main() { return 0; }");
                            }
                            if child.wait().map(|s| s.success()).unwrap_or(false) {
                                // Direct linking works
                                for flag in libs.split_whitespace() {
                                    cmd.arg(flag);
                                }
                                if debug {
                                    println!("Added Python library flags: {}", libs);
                                }
                            } else {
                                // Try framework linking as fallback
                                let framework_test = Command::new("clang")
                                    .args(&["-framework", "Python", "-x", "c", "-", "-o", "/dev/null"])
                                    .stdin(std::process::Stdio::piped())
                                    .stdout(std::process::Stdio::null())
                                    .stderr(std::process::Stdio::null())
                                    .spawn();
                                    
                                if let Ok(mut child) = framework_test {
                                    if let Some(stdin) = child.stdin.as_mut() {
                                        use std::io::Write;
                                        let _ = stdin.write_all(b"int main() { return 0; }");
                                    }
                                    if child.wait().map(|s| s.success()).unwrap_or(false) {
                                        cmd.arg("-framework").arg("Python");
                                        if debug {
                                            println!("Added Python library flags: -framework Python");
                                        }
                                    } else if debug {
                                        println!("Warning: Could not determine working Python linking flags");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            _ => {
                eprintln!("Warning: Could not get Python include path");
                eprintln!("Make sure python3 is installed and available in PATH");
            }
        }
        
        // Note: We no longer link against external libraries since we're using Python C API directly
    }
    
    // Set output
    cmd.arg("-o").arg(&output_path);
    
    if debug {
        println!("Compilation command: {:?}", cmd);
    }
    
    // Execute compilation
    let status = cmd.status()?;
    
    if !status.success() {
        return Err("C compilation failed".into());
    }
    
    println!("Successfully compiled to: {}", output_path.display());
    Ok(())
}

/// Check command: validate syntax and semantics
fn check_command(input: PathBuf, report: bool, debug: bool) -> Result<(), Box<dyn std::error::Error>> {
    let _parsed = parse_program(input, debug)?;
    
    if report {
        // TODO: Implement analysis reports for the functional logic language
        println!("Analysis reports not yet implemented for functional logic language");
    }
    
    println!("✓ All checks passed!");
    println!("✓ Syntax and parsing successful!");
    println!("Program structure is valid.");
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
        fs::write(&input_path, "main = 42").unwrap();
        
        let result = parse_program(input_path, false);
        assert!(result.is_ok());
    }

    #[test]
    fn test_run_simple_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "main = 42").unwrap();
        
        let result = run_command(input_path, false);
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_compile_simple_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "main = 42").unwrap();
        
        let result = compile_command(input_path, None, false, false);
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_check_valid_program() {
        let dir = tempdir().unwrap();
        let input_path = dir.path().join("test.fl");
        fs::write(&input_path, "main = 42").unwrap();
        
        let result = check_command(input_path, false);
        assert!(result.is_ok());
    }
}
