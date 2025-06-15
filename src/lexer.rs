use nom::{
    branch::alt,
    bytes::complete::{tag, take_until},
    character::complete::{alpha1, alphanumeric1, char, digit1, multispace0},
    combinator::{map, opt, recognize},
    multi::many0,
    sequence::{delimited, pair},
    IResult,
};

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
    Let,
    In,
    If,
    Then,
    Else,
    Lam,
    Match,
    With,
    Alloc,
    Free,
    Load,
    Store,
    
    // Literals
    Integer(i64),
    String(String),
    
    // Identifiers
    Identifier(String),
    Variable(String),  // $variable
    
    // Operators
    Arrow,      // ->
    FatArrow,   // =>
    Ampersand,  // &
    Pipe,       // |
    LinearArrow, // -o (linear implication)
    Tensor,     // *
    Plus,       // +
    Minus,      // -
    Multiply,   // *
    Divide,     // /
    Equal,      // =
    EqualEqual, // ==
    Less,       // <
    Greater,    // >
    And,        // &&
    Or,         // ||
    Bang,       // ! (clone operator)
    
    // Punctuation
    LeftParen,   // (
    RightParen,  // )
    LeftBrace,   // {
    RightBrace,  // }
    Comma,       // ,
    Dot,         // .
    Semicolon,   // ;
    
    // Special
    Eof,
    
    // Logical programming tokens
    Query,      // ?-
    Rule,       // :-
    
    // Types
    Type,       // type
    Colon,      // :
    ColonColon, // ::
}

/// Parse an identifier
fn identifier(input: &str) -> IResult<&str, String> {
    map(
        recognize(pair(
            alt((alpha1, tag("_"))),
            many0(alt((alphanumeric1, tag("_")))),
        )),
        |s: &str| s.to_string(),
    )(input)
}

/// Parse a variable (prefixed with $)
fn variable(input: &str) -> IResult<&str, String> {
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

/// Parse a single token
fn token(input: &str) -> IResult<&str, Token> {
    alt((
        parse_keywords,
        parse_operators,
        parse_punctuation,
        parse_literals,
        map(variable, Token::Variable),
        map(identifier, Token::Identifier),
    ))(input)
}

/// Parse keywords
fn parse_keywords(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag("match"), |_| Token::Match),
        map(tag("alloc"), |_| Token::Alloc),
        map(tag("store"), |_| Token::Store),
        map(tag("free"), |_| Token::Free),
        map(tag("load"), |_| Token::Load),
        map(tag("then"), |_| Token::Then),
        map(tag("else"), |_| Token::Else),
        map(tag("with"), |_| Token::With),
        map(tag("let"), |_| Token::Let),
        map(tag("lam"), |_| Token::Lam),
        map(tag("in"), |_| Token::In),
        map(tag("if"), |_| Token::If),
        map(tag("type"), |_| Token::Type),
    ))(input)
}

/// Parse operators
fn parse_operators(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag("=>"), |_| Token::FatArrow),
        map(tag("-o"), |_| Token::LinearArrow),
        map(tag("->"), |_| Token::Arrow),
        map(tag("=="), |_| Token::EqualEqual),
        map(tag("&&"), |_| Token::And),
        map(tag("||"), |_| Token::Or),
        map(tag("&"), |_| Token::Ampersand),
        map(tag("+"), |_| Token::Plus),
        map(tag("-"), |_| Token::Minus),
        map(tag("*"), |_| Token::Multiply),
        map(tag("/"), |_| Token::Divide),
        map(tag("="), |_| Token::Equal),
        map(tag("<"), |_| Token::Less),
        map(tag(">"), |_| Token::Greater),
        map(tag("!"), |_| Token::Bang),
        map(tag("|"), |_| Token::Pipe),
    ))(input)
}

/// Parse punctuation
fn parse_punctuation(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag(":-"), |_| Token::Rule),
        map(tag("?-"), |_| Token::Query),
        map(tag("::"), |_| Token::ColonColon),
        map(tag(":"), |_| Token::Colon),
        map(tag("("), |_| Token::LeftParen),
        map(tag(")"), |_| Token::RightParen),
        map(tag("{"), |_| Token::LeftBrace),
        map(tag("}"), |_| Token::RightBrace),
        map(tag(","), |_| Token::Comma),
        map(tag("."), |_| Token::Dot),
        map(tag(";"), |_| Token::Semicolon),
    ))(input)
}

/// Parse literals
fn parse_literals(input: &str) -> IResult<&str, Token> {
    alt((
        map(integer, Token::Integer),
        map(string_literal, Token::String),
    ))(input)
}

/// Tokenize the entire input
#[derive(Debug, Clone)]
pub struct LexError {
    pub message: String,
    pub line: usize,
    pub column: usize,
    pub length: usize,
}

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

pub fn tokenize(input: &str) -> Result<LexResult, String> {
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
                let consumed = &remaining[..token_length];
                for ch in consumed.chars() {
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
                // Record the error but continue lexing
                let error_char = remaining.chars().next().unwrap_or('\0');
                let error_length = if error_char == '\0' { 0 } else { 1 };
                
                errors.push(LexError {
                    message: format!("Unexpected character: '{}'", error_char),
                    line,
                    column,
                    length: error_length,
                });
                
                // Skip the problematic character and continue
                if !remaining.is_empty() {
                    let mut chars = remaining.chars();
                    let skipped_char = chars.next().unwrap();
                    remaining = chars.as_str();
                    
                    if skipped_char == '\n' {
                        line += 1;
                        column = 1;
                    } else {
                        column += 1;
                    }
                }
            }
        }
    }
    
    tokens.push(TokenSpan::new(Token::Eof, line, column, 0));
    Ok(LexResult { tokens, errors })
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_tokenize_simple() {
        let input = "let x = 42 in x + 1";
        let tokens = tokenize(input).unwrap();
        let expected_tokens = vec![
            TokenSpan::new(Token::Let, 1, 1, 3),
            TokenSpan::new(Token::Identifier("x".to_string()), 1, 5, 1),
            TokenSpan::new(Token::Equal, 1, 7, 1),
            TokenSpan::new(Token::Integer(42), 1, 9, 2),
            TokenSpan::new(Token::In, 1, 12, 2),
            TokenSpan::new(Token::Identifier("x".to_string()), 1, 15, 1),
            TokenSpan::new(Token::Plus, 1, 17, 1),
            TokenSpan::new(Token::Integer(1), 1, 19, 1),
            TokenSpan::new(Token::Eof, 1, 20, 0),
        ];
        assert_eq!(tokens, expected_tokens);
    }
    
    #[test]
    fn test_tokenize_lambda() {
        let input = "lam x. x + 1";
        let tokens = tokenize(input).unwrap();
        let expected_tokens = vec![
            TokenSpan::new(Token::Lam, 1, 1, 3),
            TokenSpan::new(Token::Identifier("x".to_string()), 1, 5, 1),
            TokenSpan::new(Token::Dot, 1, 6, 1),
            TokenSpan::new(Token::Identifier("x".to_string()), 1, 8, 1),
            TokenSpan::new(Token::Plus, 1, 10, 1),
            TokenSpan::new(Token::Integer(1), 1, 12, 1),
            TokenSpan::new(Token::Eof, 1, 13, 0),
        ];
        assert_eq!(tokens, expected_tokens);
    }
}
