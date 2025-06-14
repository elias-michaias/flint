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
    
    // Operators
    Arrow,      // ->
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
    ColonDash,   // :-
    Question,    // ?
    Period,      // .
    Pipe,        // |
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
    ))(input)
}

/// Parse operators
fn parse_operators(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag("-o"), |_| Token::LinearArrow),
        map(tag("->"), |_| Token::Arrow),
        map(tag("=="), |_| Token::EqualEqual),
        map(tag("&&"), |_| Token::And),
        map(tag("||"), |_| Token::Or),
        map(tag("+"), |_| Token::Plus),
        map(tag("-"), |_| Token::Minus),
        map(tag("*"), |_| Token::Multiply),
        map(tag("/"), |_| Token::Divide),
        map(tag("="), |_| Token::Equal),
        map(tag("<"), |_| Token::Less),
        map(tag(">"), |_| Token::Greater),
    ))(input)
}

/// Parse punctuation
fn parse_punctuation(input: &str) -> IResult<&str, Token> {
    alt((
        map(tag(":-"), |_| Token::ColonDash),
        map(tag("("), |_| Token::LeftParen),
        map(tag(")"), |_| Token::RightParen),
        map(tag("{"), |_| Token::LeftBrace),
        map(tag("}"), |_| Token::RightBrace),
        map(tag(","), |_| Token::Comma),
        map(tag("."), |_| Token::Period),
        map(tag(";"), |_| Token::Semicolon),
        map(tag("?"), |_| Token::Question),
        map(tag("|"), |_| Token::Pipe),
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
pub fn tokenize(mut input: &str) -> Result<Vec<Token>, String> {
    let mut tokens = Vec::new();
    
    while !input.is_empty() {
        // Skip whitespace
        match multispace0::<&str, nom::error::Error<&str>>(input) {
            Ok((rest, _)) => input = rest,
            Err(_) => break,
        }
        
        if input.is_empty() {
            break;
        }
        
        // Parse token
        match token(input) {
            Ok((rest, tok)) => {
                tokens.push(tok);
                input = rest;
            }
            Err(_) => {
                return Err(format!("Unexpected character at: {}", &input[..10.min(input.len())]));
            }
        }
    }
    
    tokens.push(Token::Eof);
    Ok(tokens)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_tokenize_simple() {
        let input = "let x = 42 in x + 1";
        let tokens = tokenize(input).unwrap();
        assert_eq!(tokens, vec![
            Token::Let,
            Token::Identifier("x".to_string()),
            Token::Equal,
            Token::Integer(42),
            Token::In,
            Token::Identifier("x".to_string()),
            Token::Plus,
            Token::Integer(1),
            Token::Eof,
        ]);
    }
    
    #[test]
    fn test_tokenize_lambda() {
        let input = "lam x. x + 1";
        let tokens = tokenize(input).unwrap();
        assert_eq!(tokens, vec![
            Token::Lam,
            Token::Identifier("x".to_string()),
            Token::Dot,
            Token::Identifier("x".to_string()),
            Token::Plus,
            Token::Integer(1),
            Token::Eof,
        ]);
    }
}
