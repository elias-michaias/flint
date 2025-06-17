use std::fmt;

/// ANSI color codes for terminal output
pub struct Colors;

impl Colors {
    pub const RESET: &'static str = "\x1b[0m";
    pub const RED: &'static str = "\x1b[31m";
    pub const YELLOW: &'static str = "\x1b[33m";
    pub const BLUE: &'static str = "\x1b[34m";
    pub const GREEN: &'static str = "\x1b[32m";
    pub const CYAN: &'static str = "\x1b[36m";
    pub const WHITE: &'static str = "\x1b[37m";
    pub const GRAY: &'static str = "\x1b[90m";
    pub const BOLD: &'static str = "\x1b[1m";
    pub const UNDERLINE: &'static str = "\x1b[4m";
}

/// Represents a location in source code
#[derive(Debug, Clone, PartialEq)]
pub struct SourceLocation {
    pub file: String,
    pub line: usize,
    pub column: usize,
    pub length: usize,
}

impl SourceLocation {
    pub fn new(file: String, line: usize, column: usize, length: usize) -> Self {
        Self { file, line, column, length }
    }
    
    /// Create a location for a single character
    pub fn single(file: String, line: usize, column: usize) -> Self {
        Self::new(file, line, column, 1)
    }
}

impl fmt::Display for SourceLocation {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.line == 0 {
            write!(f, "{}", self.file)
        } else {
            write!(f, "{}:{}:{}", self.file, self.line, self.column)
        }
    }
}

/// Diagnostic severity levels
#[derive(Debug, Clone, PartialEq)]
pub enum Severity {
    Error,
    Warning,
    Info,
    Note,
}

impl fmt::Display for Severity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Severity::Error => write!(f, "{}{}error{}", Colors::BOLD, Colors::RED, Colors::RESET),
            Severity::Warning => write!(f, "{}{}warning{}", Colors::BOLD, Colors::YELLOW, Colors::RESET),
            Severity::Info => write!(f, "{}{}info{}", Colors::BOLD, Colors::BLUE, Colors::RESET),
            Severity::Note => write!(f, "{}{}note{}", Colors::BOLD, Colors::CYAN, Colors::RESET),
        }
    }
}

/// A diagnostic message with location and context
#[derive(Debug, Clone)]
pub struct Diagnostic {
    pub severity: Severity,
    pub message: String,
    pub location: Option<SourceLocation>,
    pub source_line: Option<String>,
    pub help: Option<String>,
    pub note: Option<String>,
}

impl Diagnostic {
    /// Create a new error diagnostic
    pub fn error(message: String) -> Self {
        Self {
            severity: Severity::Error,
            message,
            location: None,
            source_line: None,
            help: None,
            note: None,
        }
    }
    
    /// Create a new warning diagnostic
    pub fn warning(message: String) -> Self {
        Self {
            severity: Severity::Warning,
            message,
            location: None,
            source_line: None,
            help: None,
            note: None,
        }
    }
    
    /// Add location information
    pub fn with_location(mut self, location: SourceLocation) -> Self {
        self.location = Some(location);
        self
    }
    
    /// Add source line context
    pub fn with_source_line(mut self, source_line: String) -> Self {
        self.source_line = Some(source_line);
        self
    }
    
    /// Add a help message
    pub fn with_help(mut self, help: String) -> Self {
        self.help = Some(help);
        self
    }
    
    /// Add a note
    pub fn with_note(mut self, note: String) -> Self {
        self.note = Some(note);
        self
    }
    
    /// Set source text for context extraction
    pub fn with_source_text(mut self, source_text: String) -> Self {
        if let Some(ref location) = self.location {
            // Extract the source line from the full source text
            let lines: Vec<&str> = source_text.lines().collect();
            if location.line > 0 && location.line <= lines.len() {
                self.source_line = Some(lines[location.line - 1].to_string());
            }
        }
        self
    }
    
    /// Print the diagnostic to stderr
    pub fn emit(&self) {
        eprintln!("{}", self);
    }
}

impl fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Main diagnostic line
        if let Some(location) = &self.location {
            writeln!(f, "{}: {}", self.severity, self.message)?;
            writeln!(f, "{}  --> {}{}:{}:{}", 
                Colors::BLUE, Colors::WHITE, location.file, location.line, location.column)?;
        } else {
            writeln!(f, "{}: {}", self.severity, self.message)?;
        }
        
        // Source line with underlined error
        if let (Some(location), Some(source_line)) = (&self.location, &self.source_line) {
            // Line number gutter
            let line_num = location.line;
            let gutter_width = format!("{}", line_num).len().max(2);
            
            writeln!(f, "{}{:width$} |{}", 
                Colors::BLUE, "", Colors::RESET, width = gutter_width)?;
            
            // Source line
            writeln!(f, "{}{:width$} | {}{}{}", 
                Colors::BLUE, line_num, Colors::WHITE, source_line, Colors::RESET, 
                width = gutter_width)?;
            
            // Error underline
            write!(f, "{}{:width$} | {}", 
                Colors::BLUE, "", Colors::RESET, width = gutter_width)?;
            
            // Add spaces before the underline
            for _ in 0..location.column.saturating_sub(1) {
                write!(f, " ")?;
            }
            
            // Add the underline
            let underline_color = match self.severity {
                Severity::Error => Colors::RED,
                Severity::Warning => Colors::YELLOW,
                Severity::Info => Colors::BLUE,
                Severity::Note => Colors::CYAN,
            };
            
            write!(f, "{}{}", underline_color, Colors::BOLD)?;
            for _ in 0..location.length {
                write!(f, "^")?;
            }
            writeln!(f, "{}", Colors::RESET)?;
            
            // Empty line after underline
            writeln!(f, "{}{:width$} |{}", 
                Colors::BLUE, "", Colors::RESET, width = gutter_width)?;
        }
        
        // Help message
        if let Some(help) = &self.help {
            writeln!(f, "{}help{}: {}", 
                Colors::BOLD, Colors::RESET, help)?;
        }
        
        // Note
        if let Some(note) = &self.note {
            writeln!(f, "{}note{}: {}", 
                Colors::BOLD, Colors::RESET, note)?;
        }
        
        Ok(())
    }
}

/// Helper to create diagnostics from source text
pub struct DiagnosticBuilder {
    source_file: String,
    source_text: String,
    lines: Vec<String>,
}

impl DiagnosticBuilder {
    pub fn new(source_file: String, source_text: String) -> Self {
        let lines: Vec<String> = source_text.lines().map(|s| s.to_string()).collect();
        Self {
            source_file,
            source_text,
            lines,
        }
    }
    
    /// Create an error diagnostic at a specific position
    pub fn error_at(&self, message: String, line: usize, column: usize, length: usize) -> Diagnostic {
        let location = SourceLocation::new(self.source_file.clone(), line, column, length);
        let source_line = self.lines.get(line.saturating_sub(1))
            .cloned()
            .unwrap_or_else(|| "<source unavailable>".to_string());
        
        Diagnostic::error(message)
            .with_location(location)
            .with_source_line(source_line)
    }
    
    /// Create a warning diagnostic at a specific position
    pub fn warning_at(&self, message: String, line: usize, column: usize, length: usize) -> Diagnostic {
        let location = SourceLocation::new(self.source_file.clone(), line, column, length);
        let source_line = self.lines.get(line.saturating_sub(1))
            .cloned()
            .unwrap_or_else(|| "<source unavailable>".to_string());
        
        Diagnostic::warning(message)
            .with_location(location)
            .with_source_line(source_line)
    }
    
    /// Create an error diagnostic for an unexpected token
    pub fn unexpected_token(&self, expected: &str, found: &str, line: usize, column: usize, length: usize) -> Diagnostic {
        self.error_at(
            format!("unexpected token `{}`", found),
            line, column, length
        )
        .with_help(format!("expected {}", expected))
    }
    
    /// Create an error diagnostic for invalid syntax
    pub fn invalid_syntax(&self, description: &str, line: usize, column: usize, length: usize) -> Diagnostic {
        self.error_at(
            format!("invalid syntax: {}", description),
            line, column, length
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_diagnostic_display() {
        let location = SourceLocation::new("test.ll".to_string(), 5, 12, 3);
        let diagnostic = Diagnostic::error("unexpected token".to_string())
            .with_location(location)
            .with_source_line("let x = foo + bar;".to_string())
            .with_help("try using a different operator".to_string());
        
        // Just ensure it doesn't panic when formatting
        let _ = format!("{}", diagnostic);
    }
    
    #[test]
    fn test_diagnostic_builder() {
        let source = "line 1\nline 2 with error\nline 3";
        let builder = DiagnosticBuilder::new("test.ll".to_string(), source.to_string());
        
        let diagnostic = builder.error_at("test error".to_string(), 2, 8, 4);
        assert_eq!(diagnostic.severity, Severity::Error);
        assert!(diagnostic.location.is_some());
        assert!(diagnostic.source_line.is_some());
    }
}
