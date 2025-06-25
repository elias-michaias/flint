use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::io::Write;

use crate::ast::{Program, Statement, Declaration};
use crate::diagnostic::{Diagnostic, SourceLocation};

// =============================================================================
// PACKAGE MANAGEMENT FOR FLINT
// =============================================================================
//
// This module handles package management for Flint, supporting:
// - Python packages via PyPI (direct Python C API interop)
// - Future: Native Flint packages
// - Future: C library packages
//
// Python Import Syntax:
//   import Python "pypi::numpy::2.3.1" as numpy
//   import Python "pypi::scipy::1.11.0" as scipy
//
// Process:
// 1. Parse import statements from Flint AST
// 2. Create .flint/lib/python/ directory structure
// 3. Fetch packages from PyPI using pip
// 4. Generate C headers for Python C API interop
// 5. Update build system to include Python linking flags

#[derive(Debug, Clone, PartialEq)]
pub enum PackageType {
    Python { pypi_name: String, version: String },
    Flint { name: String, version: String },
    C { name: String, version: Option<String> },
}

#[derive(Debug, Clone)]
pub struct PackageImport {
    pub package_type: PackageType,
    pub import_name: String,  // The name used in Flint code (e.g., "numpy")
    pub location: SourceLocation,
}

#[derive(Debug, Clone)]
pub struct PythonPackage {
    pub pypi_name: String,     // e.g., "numpy"
    pub version: String,       // e.g., "2.3.1"
    pub import_name: String,   // e.g., "numpy" (alias in Flint)
    pub install_path: PathBuf, // .flint/lib/python/numpy/
    pub is_installed: bool,
    pub is_compiled: bool,
    pub exported_functions: Vec<PythonFunction>,
}

#[derive(Debug, Clone)]
pub struct PythonFunction {
    pub name: String,          // e.g., "array"
    pub c_name: String,        // e.g., "numpy_array"
    pub package: String,       // e.g., "numpy"
    pub parameters: Vec<PythonParam>,
    pub return_type: PythonType,
}

#[derive(Debug, Clone)]
pub struct PythonParam {
    pub name: String,
    pub param_type: PythonType,
    pub optional: bool,
}

#[derive(Debug, Clone)]
pub enum PythonType {
    Integer,
    Float,
    String,
    Array { element_type: Box<PythonType> },
    Object { class_name: String },
    Any,
}

#[derive(Debug)]
pub struct PackageManager {
    pub project_dir: PathBuf,
    pub flint_dir: PathBuf,     // .flint/
    pub lib_dir: PathBuf,       // .flint/lib/
    pub python_dir: PathBuf,    // .flint/lib/python/
    pub packages: HashMap<String, PythonPackage>,
    pub imports: Vec<PackageImport>,
}

impl PackageManager {
    pub fn new(project_dir: PathBuf) -> Self {
        let flint_dir = project_dir.join(".flint");
        let lib_dir = flint_dir.join("lib");
        let python_dir = lib_dir.join("python");
        
        Self {
            project_dir,
            flint_dir,
            lib_dir,
            python_dir,
            packages: HashMap::new(),
            imports: Vec::new(),
        }
    }

    // =============================================================================
    // IMPORT PARSING
    // =============================================================================

    /// Parse import statements from a Flint program
    pub fn parse_imports(&mut self, program: &Program) -> Result<(), Diagnostic> {
        self.imports.clear();
        
        for declaration in &program.declarations {
            if let Declaration::PythonImport(python_import) = declaration {
                let import = self.parse_python_import_spec(&python_import.package_spec, &python_import.alias)?;
                self.imports.push(import);
                println!("[PACKAGE] Found Python import: {} as {}", python_import.package_spec, python_import.alias);
            }
        }
        
        println!("[PACKAGE] Found {} import statements", self.imports.len());
        Ok(())
    }

    fn parse_import_statement(&self, _statement: &Statement) -> Result<Option<PackageImport>, Diagnostic> {
        // Look for import expressions in statements
        // For now, we'll focus on Python imports which are already parsed in parse_imports()
        Ok(None)
    }

    /// Parse a Python import specification like "pypi::numpy::2.3.1"
    fn parse_python_import_spec(&self, spec: &str, alias: &str) -> Result<PackageImport, Diagnostic> {
        let parts: Vec<&str> = spec.split("::").collect();
        
        if parts.len() != 3 || parts[0] != "pypi" {
            return Err(Diagnostic::error(
                format!("Invalid Python import specification: '{}'. Expected format: 'pypi::package::version'", spec)
            ));
        }
        
        let pypi_name = parts[1].to_string();
        let version = parts[2].to_string();
        
        Ok(PackageImport {
            package_type: PackageType::Python { pypi_name, version },
            import_name: alias.to_string(),
            location: SourceLocation::new("".to_string(), 0, 0, 0),
        })
    }

    /// Manually add a Python import (for testing/development)
    pub fn add_python_import(&mut self, pypi_name: &str, version: &str, import_name: &str, location: SourceLocation) {
        let import = PackageImport {
            package_type: PackageType::Python {
                pypi_name: pypi_name.to_string(),
                version: version.to_string(),
            },
            import_name: import_name.to_string(),
            location,
        };
        self.imports.push(import);
    }

    // =============================================================================
    // DIRECTORY MANAGEMENT
    // =============================================================================

    /// Create the .flint directory structure and Python virtual environment
    pub fn setup_directories(&self) -> Result<(), Diagnostic> {
        self.create_dir_if_not_exists(&self.flint_dir)?;
        self.create_dir_if_not_exists(&self.lib_dir)?;
        self.create_dir_if_not_exists(&self.python_dir)?;
        
        // Create Python virtual environment if it doesn't exist
        let venv_path = self.python_dir.join("venv");
        if !venv_path.exists() {
            println!("[PACKAGE] Creating Python virtual environment...");
            let output = Command::new("python3")
                .args(["-m", "venv", "venv"])
                .current_dir(&self.python_dir)
                .output()
                .map_err(|e| {
                    Diagnostic::error(format!("Failed to create virtual environment: {}", e))
                })?;
                
            if !output.status.success() {
                let stderr = String::from_utf8_lossy(&output.stderr);
                return Err(Diagnostic::error(format!(
                    "Failed to create virtual environment: {}", stderr
                )));
            }
            
            println!("[PACKAGE] Virtual environment created successfully");
        }
        
        println!("[PACKAGE] Created .flint directory structure");
        Ok(())
    }

    fn create_dir_if_not_exists(&self, path: &Path) -> Result<(), Diagnostic> {
        if !path.exists() {
            fs::create_dir_all(path).map_err(|e| {
                Diagnostic::error(format!("Failed to create directory {}: {}", path.display(), e))
            })?;
        }
        Ok(())
    }

    /// Get the path to the virtual environment's Python executable
    fn get_venv_python(&self) -> PathBuf {
        let venv_path = self.python_dir.join("venv");
        let python_executable = if cfg!(target_os = "windows") {
            venv_path.join("Scripts").join("python.exe")
        } else {
            venv_path.join("bin").join("python")
        };
        
        // Convert to absolute path to avoid issues with current directory
        if python_executable.is_absolute() {
            python_executable
        } else {
            std::env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .join(python_executable)
        }
    }

    /// Get the path to the virtual environment's pip executable
    fn get_venv_pip(&self) -> PathBuf {
        let venv_path = self.python_dir.join("venv");
        if cfg!(target_os = "windows") {
            venv_path.join("Scripts").join("pip.exe")
        } else {
            venv_path.join("bin").join("pip")
        }
    }

    // =============================================================================
    // PYTHON PACKAGE INSTALLATION
    // =============================================================================

    /// Install all Python packages from imports
    pub fn install_packages(&mut self) -> Result<(), Diagnostic> {
        let imports = self.imports.clone(); // Clone to avoid borrowing issues
        for import in &imports {
            if let PackageType::Python { pypi_name, version } = &import.package_type {
                self.install_python_package(pypi_name, version, &import.import_name)?;
            }
        }
        Ok(())
    }

    fn install_python_package(&mut self, pypi_name: &str, version: &str, import_name: &str) -> Result<(), Diagnostic> {
        let package_dir = self.python_dir.join(import_name);
        
        // Create package directory
        self.create_dir_if_not_exists(&package_dir)?;
        
        // Check if already installed
        let installed_marker = package_dir.join(".installed");
        if installed_marker.exists() {
            println!("[PACKAGE] Python package {} already installed", import_name);
            
            let package = PythonPackage {
                pypi_name: pypi_name.to_string(),
                version: version.to_string(),
                import_name: import_name.to_string(),
                install_path: package_dir,
                is_installed: true,
                is_compiled: false,
                exported_functions: Vec::new(),
            };
            self.packages.insert(import_name.to_string(), package);
            return Ok(());
        }
        
        // Create requirements.txt
        let requirements_file = package_dir.join("requirements.txt");
        let mut req_file = fs::File::create(&requirements_file).map_err(|e| {
            Diagnostic::error(format!("Failed to create requirements.txt: {}", e))
        })?;
        writeln!(req_file, "{}=={}", pypi_name, version).map_err(|e| {
            Diagnostic::error(format!("Failed to write requirements.txt: {}", e))
        })?;
        
        // Install with pip in virtual environment
        println!("[PACKAGE] Installing Python package {}=={}", pypi_name, version);
        let venv_python = self.get_venv_python();
        println!("[PACKAGE] Using Python executable: {}", venv_python.display());
        let output = Command::new(&venv_python)
            .args(["-m", "pip", "install", "-r", "requirements.txt", "--target", "."])
            .current_dir(&package_dir)
            .output()
            .map_err(|e| {
                Diagnostic::error(format!("Failed to run pip install: {}", e))
            })?;
        
        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(Diagnostic::error(format!(
                "Failed to install Python package {}: {}", pypi_name, stderr
            )));
        }
        
        // Create installed marker
        fs::File::create(&installed_marker).map_err(|e| {
            Diagnostic::error(format!("Failed to create install marker: {}", e))
        })?;
        
        println!("[PACKAGE] Successfully installed {}=={}", pypi_name, version);
        
        let package = PythonPackage {
            pypi_name: pypi_name.to_string(),
            version: version.to_string(),
            import_name: import_name.to_string(),
            install_path: package_dir,
            is_installed: true,
            is_compiled: false,
            exported_functions: Vec::new(),
        };
        self.packages.insert(import_name.to_string(), package);
        
        Ok(())
    }

    // =============================================================================
    // PYTHON PACKAGE PROCESSING
    // =============================================================================

    /// Process all Python packages (install only, no compilation needed)
    pub fn process_python_packages(&mut self) -> Result<(), Diagnostic> {
        let package_names: Vec<String> = self.packages.keys().cloned().collect();
        for package_name in package_names {
            let should_process = {
                let package = self.packages.get(&package_name).unwrap();
                package.is_installed && !package.is_compiled
            };
            
            if should_process {
                self.mark_package_ready(&package_name)?;
            }
        }
        Ok(())
    }

    fn mark_package_ready(&mut self, package_name: &str) -> Result<(), Diagnostic> {
        println!("[PACKAGE] Marking {} as ready for Python C API interop", package_name);
        
        // Mark as compiled (ready for use)
        if let Some(package) = self.packages.get_mut(package_name) {
            package.is_compiled = true;
        }
        
        println!("[PACKAGE] Successfully prepared {}", package_name);
        Ok(())
    }

    // =============================================================================
    // BUILD SYSTEM INTEGRATION
    // =============================================================================

    /// Get include paths for all compiled packages
    pub fn get_include_paths(&self) -> Vec<PathBuf> {
        self.packages
            .values()
            .filter(|p| p.is_compiled)
            .map(|p| p.install_path.clone())
            .collect()
    }

    /// Get library paths for all compiled packages
    pub fn get_library_paths(&self) -> Vec<PathBuf> {
        self.packages
            .values()
            .filter(|p| p.is_compiled)
            .map(|p| p.install_path.clone())
            .collect()
    }

    /// Get list of object files to link (none needed for Python C API interop)
    pub fn get_object_files(&self) -> Vec<PathBuf> {
        // No object files needed - Python C API interop is handled at runtime
        Vec::new()
    }

    // =============================================================================
    // COMPILATION INTEGRATION
    // =============================================================================

    /// Get linking flags for compiled Python packages
    pub fn get_linking_flags(&self) -> Vec<String> {
        let mut flags = Vec::new();
        
        // If we have any Python packages, add Python development headers and linking flags
        let has_python_packages = self.packages.values().any(|p| p.is_compiled);
        if has_python_packages {
            let venv_python = self.get_venv_python();
            
            // Add Python include directories from venv
            if let Ok(output) = Command::new(&venv_python)
                .args(["-c", "import sysconfig; print(sysconfig.get_path('include'))"])
                .output() 
            {
                if output.status.success() {
                    let include_path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                    flags.push("-I".to_string());
                    flags.push(include_path.clone());
                    println!("[PACKAGE] Added Python include path: {}", include_path);
                }
            }
            
            // Also try platinclude for platform-specific headers
            if let Ok(output) = Command::new(&venv_python)
                .args(["-c", "import sysconfig; print(sysconfig.get_path('platinclude'))"])
                .output() 
            {
                if output.status.success() {
                    let platinclude_path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                    // Only add if different from include path
                    let include_path = if let Ok(inc_output) = Command::new(&venv_python)
                        .args(["-c", "import sysconfig; print(sysconfig.get_path('include'))"])
                        .output() 
                    {
                        String::from_utf8_lossy(&inc_output.stdout).trim().to_string()
                    } else {
                        String::new()
                    };
                    
                    if platinclude_path != include_path {
                        flags.push("-I".to_string());
                        flags.push(platinclude_path.clone());
                        println!("[PACKAGE] Added Python platinclude path: {}", platinclude_path);
                    }
                }
            }
            
            // Add Python library linking flags from venv
            if let Ok(output) = Command::new(&venv_python)
                .args(["-c", "import sysconfig; libdir = sysconfig.get_config_var('LIBDIR'); print(libdir if libdir else '')"])
                .output()
            {
                if output.status.success() {
                    let lib_path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                    if !lib_path.is_empty() {
                        flags.push("-L".to_string());
                        flags.push(lib_path.clone());
                        println!("[PACKAGE] Added Python library path: {}", lib_path);
                    }
                }
            }
            
            // Add Python library name from venv
            if let Ok(output) = Command::new(&venv_python)
                .args(["-c", "import sysconfig; v=sysconfig.get_python_version(); print(f'-lpython{v}')"])
                .output()
            {
                if output.status.success() {
                    let lib_name = String::from_utf8_lossy(&output.stdout).trim().to_string();
                    flags.push(lib_name.clone());
                    println!("[PACKAGE] Added Python library: {}", lib_name);
                }
            }
        }
        
        // Python C API interop - no external libraries to link
        // The Python packages are loaded dynamically at runtime via Python imports
        
        flags
    }

    /// Get include directories for Python packages (not needed for Python C API interop)
    pub fn get_include_dirs(&self) -> Vec<String> {
        // Python C API interop doesn't require package-specific include directories
        Vec::new()
    }

    // =============================================================================
    // PUBLIC API
    // =============================================================================

    /// Full package management pipeline
    pub fn process_packages(&mut self, program: &Program) -> Result<(), Diagnostic> {
        println!("[PACKAGE] Starting package management pipeline");
        
        // Parse imports from the program
        self.parse_imports(program)?;
        
        if self.imports.is_empty() {
            println!("[PACKAGE] No package imports found");
            return Ok(());
        }
        
        // Setup directory structure
        self.setup_directories()?;
        
        // Install packages
        self.install_packages()?;
        
        // Process packages (mark as ready for Python C API interop)
        self.process_python_packages()?;
        
        println!("[PACKAGE] Package management pipeline completed successfully");
        Ok(())
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/// Parse a Python import string like "pypi::numpy::2.3.1"
pub fn parse_python_import_string(import_str: &str) -> Option<(String, String)> {
    if !import_str.starts_with("pypi::") {
        return None;
    }
    
    let parts: Vec<&str> = import_str.split("::").collect();
    if parts.len() != 3 {
        return None;
    }
    
    Some((parts[1].to_string(), parts[2].to_string()))
}

/// Convert Python function name to C function name
pub fn python_to_c_function_name(package: &str, function: &str) -> String {
    format!("{}_{}", package, function)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::diagnostic::SourceLocation;
    
    #[test]
    fn test_parse_python_import_string() {
        assert_eq!(
            parse_python_import_string("pypi::numpy::2.3.1"),
            Some(("numpy".to_string(), "2.3.1".to_string()))
        );
        assert_eq!(parse_python_import_string("invalid"), None);
    }
    
    #[test]
    fn test_python_to_c_function_name() {
        assert_eq!(
            python_to_c_function_name("numpy", "array"),
            "numpy_array"
        );
    }
    
    #[test]
    fn test_package_manager_creation() {
        let temp_dir = std::env::temp_dir().join("flint_test");
        let pm = PackageManager::new(temp_dir.clone());
        
        assert_eq!(pm.project_dir, temp_dir);
        assert_eq!(pm.flint_dir, temp_dir.join(".flint"));
        assert_eq!(pm.python_dir, temp_dir.join(".flint/lib/python"));
    }
}
