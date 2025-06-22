use nom::{
    branch::alt,
    bytes::complete::{tag, take_until, take_while1},
    character::complete::{alpha1, alphanumeric1, char, digit1, multispace0, multispace1},
    combinator::{map, opt, recognize},
    multi::many0,
    sequence::{delimited, pair, preceded},
    IResult,
};

use crate::diagnostic::{Diagnostic, SourceLocation};

#[derive(Debug, Clone, PartialEq)]
pub struct TokenSpan {
    pub token: Token,
    pub line: usize,
    pub column: usize,
    pub length: usize,
}

impl TokenSpan {
    pub fn new(token: Token, line: usize, column: usize, length: usize) -> Self {
        Self { token, line, column, length }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    // Keywords
    Import,      // import
    As,          // as
    Record,      // record
    Effect,      // effect
    Handler,     // handler
    For,         // for
    Let,         // let
    In,          // in
    Match,       // match
    With,        // with
    Handle,      // handle
    Using,       // using
    Main,        // main
    True,        // true
    False,       // false
    
    // Type keywords
    I32,         // i32
    Str,         // str
    Bool,        // bool
    List,        // List
    Unit,        // unit or ()
    
    // Literals
    Integer(i64),
    String(String),
    
    // Identifiers and variables
    Identifier(String),     // regular identifier
    LogicVar(String),      // $variable
    CModule(String),       // C module reference
    
    // Operators
    Arrow,          // ->
    FatArrow,       // =>
    DoubleColon,    // ::
    Pipe,           // |
    PipeGt,         // |>
    Ampersand,      // &
    Plus,           // +
    Minus,          // -
    Multiply,       // *
    Divide,         // /
    Modulo,         // %
    Equal,          // =
    EqualEqual,     // ==
    NotEqual,       // !=
    Less,           // <
    LessEqual,      // <=
    Greater,        // >
    GreaterEqual,   // >=
    And,            // &&
    Or,             // ||
    Not,            // !
    Dot,            // .
    
    // Punctuation
    LeftParen,      // (
    RightParen,     // )
    LeftBrace,      // {
    RightBrace,     // }
    LeftBracket,    // [
    RightBracket,   // ]
    Comma,          // ,
    Semicolon,      // ;
    Colon,          // :
    Underscore,     // _ (wildcard)
    
    // Special
    Eof,
}

/// Parse an identifier (starts with letter or underscore)
fn identifier(input: &str) -> IResult<&str, String> {
    map(
        recognize(pair(
            alt((alpha1, tag("_"))),
            many0(alt((alphanumeric1, tag("_")))),
        )),
        |s: &str| s.to_string(),
    )(input)
}

/// Helper function to parse a keyword followed by a word boundary
fn keyword_with_boundary(keyword: &str) -> impl Fn(&str) -> IResult<&str, &str> + '_ {
    move |input: &str| {
        let (rest, matched) = tag(keyword)(input)?;
        // Check that the next character is not alphanumeric or underscore (word boundary)
        if !rest.is_empty() {
            let next_char = rest.chars().next().unwrap();
            if next_char.is_alphanumeric() || next_char == '_' {
                return Err(nom::Err::Error(nom::error::Error::new(input, nom::error::ErrorKind::Tag)));
            }
        }
        Ok((rest, matched))
    }
}

/// Parse a logic variable (prefixed with $)
fn logic_variable(input: &str) -> IResult<&str, String> {
    map(
        recognize(pair(
            char('$'),
            pair(
                alt((alpha1, tag("_"))),
                many0(alt((alphanumeric1, tag("_")))),
            ),
        )),
        |s: &str| s[1..].to_string(), // Remove the $ prefix
    )(input)
}

/// Parse an integer literal
fn integer(input: &str) -> IResult<&str, i64> {
    map(
        recognize(pair(opt(char('-')), digit1)),
        |s: &str| s.parse().unwrap(),
    )(input)
}

/// Parse a string literal
fn string_literal(input: &str) -> IResult<&str, String> {
    delimited(char('"'), take_until("\""), char('"'))(input)
        .map(|(rest, s)| (rest, s.to_string()))
}

/// Parse C module reference (C.ModuleName)
fn c_module(input: &str) -> IResult<&str, String> {
    map(
        recognize(pair(
            tag("C."),
            pair(
                alpha1,
                many0(alt((alphanumeric1, tag("_")))),
            ),
        )),
        |s: &str| s[2..].to_string(), // Remove the "C." prefix
    )(input)
}

/// Parse a single token
fn token(input: &str) -> IResult<&str, Token> {
    alt((
        parse_keywords,
        parse_type_keywords,
        parse_operators,
        parse_punctuation,
        parse_literals,
        map(logic_variable, Token::LogicVar),
        map(c_module, Token::CModule),
        map(identifier, Token::Identifier),
    ))(input)
}

/// Parse keywords
fn parse_keywords(input: &str) -> IResult<&str, Token> {
    alt((
        map(keyword_with_boundary("import"), |_| Token::Import),
        map(keyword_with_boundary("as"), |_| Token::As),
        map(keyword_with_boundary("record"), |_| Token::Record),
        map(keyword_with_boundary("effect"), |_| Token::Effect),
        map(keyword_with_boundary("handler"), |_| Token::Handler),
        map(keyword_with_boundary("for"), |_| Token::For),
        map(keyword_with_boundary("let"), |_| Token::Let),
        map(keyword_with_boundary("in"), |_| Token::In),
        map(keyword_with_boundary("match"), |_| Token::Match),
        map(keyword_with_boundary("with"), |_| Token::With),
        map(keyword_with_boundary("handle"), |_| Token::Handle),
        map(keyword_with_boundary("using"), |_| Token::Using),
        map(keyword_with_boundary("main"), |_| Token::Main),
        map(keyword_with_boundary("true"), |_| Token::True),
        map(keyword_with_boundary("false"), |_| Token::False),
    ))(input)
}

/// Parse type keywords
fn parse_type_keywords(input: &str) -> IResult<&str, Token> {
    alt((
        map(keyword_with_boundary("i32"), |_| Token::I32),
        map(keyword_with_boundary("str"), |_| Token::Str),
        map(keyword_with_boundary("bool"), |_| Token::Bool),
        map(keyword_with_boundary("List"), |_| Token::List),
        map(keyword_with_boundary("unit"), |_| Token::Unit),
    ))(input)
}

/// Parse operators
fn parse_operators(input: &str) -> IResult<&str, Token> {
    alt((
        parse_operators_part1,
        parse_operators_part2,
    ))(input)
}

/// Parse operators part 1 (comparison and arrow operators)
fn parse_operators_part1(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag("=>"), |_| Token::FatArrow),
        map(tag("->"), |_| Token::Arrow),
        map(tag("::"), |_| Token::DoubleColon),
        map(tag("|>"), |_| Token::PipeGt),
        map(tag("=="), |_| Token::EqualEqual),
        map(tag("!="), |_| Token::NotEqual),
        map(tag("<="), |_| Token::LessEqual),
        map(tag(">="), |_| Token::GreaterEqual),
        map(tag("&&"), |_| Token::And),
        map(tag("||"), |_| Token::Or),
        map(tag("|"), |_| Token::Pipe),
    ))(input)
}

/// Parse operators part 2 (arithmetic and other operators)
fn parse_operators_part2(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag("&"), |_| Token::Ampersand),
        map(tag("+"), |_| Token::Plus),
        map(tag("-"), |_| Token::Minus),
        map(tag("*"), |_| Token::Multiply),
        map(tag("/"), |_| Token::Divide),
        map(tag("%"), |_| Token::Modulo),
        map(tag("="), |_| Token::Equal),
        map(tag("<"), |_| Token::Less),
        map(tag(">"), |_| Token::Greater),
        map(tag("!"), |_| Token::Not),
        map(tag("."), |_| Token::Dot),
    ))(input)
}

/// Parse punctuation
fn parse_punctuation(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag("("), |_| Token::LeftParen),
        map(tag(")"), |_| Token::RightParen),
        map(tag("{"), |_| Token::LeftBrace),
        map(tag("}"), |_| Token::RightBrace),
        map(tag("["), |_| Token::LeftBracket),
        map(tag("]"), |_| Token::RightBracket),
        map(tag(","), |_| Token::Comma),
        map(tag(";"), |_| Token::Semicolon),
        map(tag(":"), |_| Token::Colon),
        map(tag("_"), |_| Token::Underscore),
    ))(input)
}

/// Parse literals
fn parse_literals(input: &str) -> IResult<&str, Token> {
    alt((
        map(integer, Token::Integer),
        map(string_literal, Token::String),
    ))(input)
}

/// Lexing errors
#[derive(Debug, Clone)]
pub struct LexError {
    pub diagnostic: Diagnostic,
}

impl LexError {
    pub fn new(message: String, file: String, line: usize, column: usize, length: usize) -> Self {
        let location = SourceLocation::new(file, line, column, length);
        let diagnostic = Diagnostic::error(format!("Lexer error: {}", message))
            .with_location(location);
        Self { diagnostic }
    }
    
    pub fn with_source_text(mut self, source_text: String) -> Self {
        self.diagnostic = self.diagnostic.with_source_text(source_text);
        self
    }
}

/// Lexing result
#[derive(Debug)]
pub struct LexResult {
    pub tokens: Vec<TokenSpan>,
    pub errors: Vec<LexError>,
}

impl LexResult {
    pub fn has_errors(&self) -> bool {
        !self.errors.is_empty()
    }
}

/// Tokenize the entire input
pub fn tokenize(input: &str) -> Result<LexResult, String> {
    tokenize_with_filename(input, "input".to_string())
}

/// Tokenize with a specific filename for better error reporting
pub fn tokenize_with_filename(input: &str, filename: String) -> Result<LexResult, String> {
    let mut tokens = Vec::new();
    let mut errors = Vec::new();
    let mut remaining = input;
    let mut line = 1;
    let mut column = 1;
    
    while !remaining.is_empty() {
        // Skip whitespace and track position
        match multispace0::<&str, nom::error::Error<&str>>(remaining) {
            Ok((rest, whitespace)) => {
                for ch in whitespace.chars() {
                    if ch == '\n' {
                        line += 1;
                        column = 1;
                    } else {
                        column += 1;
                    }
                }
                remaining = rest;
            }
            Err(_) => break,
        }
        
        if remaining.is_empty() {
            break;
        }
        
        // Skip comments (lines starting with //)
        if remaining.starts_with("//") {
            // Skip to end of line
            if let Some(newline_pos) = remaining.find('\n') {
                remaining = &remaining[newline_pos + 1..];
                line += 1;
                column = 1;
            } else {
                // Comment goes to end of file
                break;
            }
            continue;
        }
        
        // Parse token and calculate its length
        let token_start_column = column;
        match token(remaining) {
            Ok((rest, tok)) => {
                let token_length = remaining.len() - rest.len();
                tokens.push(TokenSpan::new(tok, line, token_start_column, token_length));
                
                // Update position for consumed characters
                for ch in remaining[..token_length].chars() {
                    if ch == '\n' {
                        line += 1;
                        column = 1;
                    } else {
                        column += 1;
                    }
                }
                
                remaining = rest;
            }
            Err(_) => {
                // Skip the problematic character and report an error
                let error_char = remaining.chars().next().unwrap_or(' ');
                let error = LexError::new(
                    format!("Unexpected character: '{}'", error_char),
                    filename.clone(),
                    line,
                    column,
                    1,
                ).with_source_text(input.to_string());
                errors.push(error);
                
                // Skip one character
                let mut char_indices = remaining.char_indices();
                char_indices.next();
                if let Some((next_index, _)) = char_indices.next() {
                    remaining = &remaining[next_index..];
                } else {
                    remaining = &remaining[1..];
                }
                
                if error_char == '\n' {
                    line += 1;
                    column = 1;
                } else {
                    column += 1;
                }
            }
        }
    }
    
    // Add EOF token
    tokens.push(TokenSpan::new(Token::Eof, line, column, 0));
    
    Ok(LexResult { tokens, errors })
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_basic_tokens() {
        let input = "reverse :: List<T> -> List<T>";
        let result = tokenize(input).unwrap();
        
        assert!(!result.has_errors());
        let tokens: Vec<Token> = result.tokens.into_iter().map(|ts| ts.token).collect();
        
        assert_eq!(tokens, vec![
            Token::Identifier("reverse".to_string()),
            Token::DoubleColon,
            Token::List,
            Token::Less,
            Token::Identifier("T".to_string()),
            Token::Greater,
            Token::Arrow,
            Token::List,
            Token::Less,
            Token::Identifier("T".to_string()),
            Token::Greater,
            Token::Eof,
        ]);
    }
    
    #[test]
    fn test_logic_variables() {
        let input = "reverse :: ([$h|$t]) => reverse($t)";
        let result = tokenize(input).unwrap();
        
        assert!(!result.has_errors());
        let tokens: Vec<Token> = result.tokens.into_iter().map(|ts| ts.token).collect();
        
        // Check that logic variables are properly tokenized
        assert!(tokens.contains(&Token::LogicVar("h".to_string())));
        assert!(tokens.contains(&Token::LogicVar("t".to_string())));
    }
    
    #[test]
    fn test_c_imports() {
        let input = "import C \"stdio.h\" as IO";
        let result = tokenize(input).unwrap();
        
        assert!(!result.has_errors());
        let tokens: Vec<Token> = result.tokens.into_iter().map(|ts| ts.token).collect();
        
        assert_eq!(tokens[0], Token::Import);
        assert_eq!(tokens[1], Token::Identifier("C".to_string()));
        assert_eq!(tokens[2], Token::String("stdio.h".to_string()));
        assert_eq!(tokens[3], Token::As);
        assert_eq!(tokens[4], Token::Identifier("IO".to_string()));
    }
    
    #[test]
    fn test_effect_syntax() {
        let input = "API :: effect { fetch_users :: () -> List<User> using IO, C }";
        let result = tokenize(input).unwrap();
        
        assert!(!result.has_errors());
        let tokens: Vec<Token> = result.tokens.into_iter().map(|ts| ts.token).collect();
        
        assert!(tokens.contains(&Token::Effect));
        assert!(tokens.contains(&Token::Using));
    }
}
