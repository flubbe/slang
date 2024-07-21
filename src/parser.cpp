/**
 * slang - a simple scripting language.
 *
 * the parser. generates an AST from the lexer output.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <unordered_map>
#include <set>

#include "parser.h"

namespace slang
{

/*
 * Keyword list.
 */

// clang-format off
static std::set<std::string> keywords = {
  "import", 
  "let", 
  "i32", "f32", "str", 
  "void", 
  "as", 
  "struct", 
  "fn", "return", 
  "if", "else", 
  "while", "break", "continue"
};
// clang-format on

/** Check whether a given string is a keyword. */
static bool is_keyword(const std::string& s)
{
    return keywords.find(s) != keywords.end();
}

/**
 * Check whether a name can be used as an identifier and throw a syntax_error
 * if this is not the case.
 *
 * @param tok The token to validate.
 */
static void validate_identifier_name(const token& tok)
{
    // we probably already know this, but validate anyway.
    if(tok.type != token_type::identifier)
    {
        throw syntax_error(tok, fmt::format("Expected <identifier>, got '{}'.", tok.s));
    }

    if(is_keyword(tok.s))
    {
        throw syntax_error(tok, fmt::format("Expected <identifier>, got keyword '{}'.", tok.s));
    }
}

/**
 * Check whether a name can be used as a type name and throw a syntax_error
 * if this is not the case.
 *
 * @param tok The token to validate.
 */
static void validate_base_type(const token& tok)
{
    if(tok.type != token_type::identifier)
    {
        throw syntax_error(tok, fmt::format("Expected <type>, got '{}'.", tok.s));
    }

    // check built-in types.
    if(tok.s == "void" || tok.s == "i32" || tok.s == "f32" || tok.s == "str")
    {
        return;
    }

    // check for keywords.
    if(is_keyword(tok.s))
    {
        throw syntax_error(tok, fmt::format("Expected <type>, got keyword '{}'.", tok.s));
    }
}

/*
 * Exceptions.
 */

syntax_error::syntax_error(const token& tok, const std::string& message)
: std::runtime_error{fmt::format("{}: {}", to_string(tok.location), message)}
{
}

parser_error::parser_error(const token& tok, const std::string& message)
: std::runtime_error{fmt::format("{}: {}", to_string(tok.location), message)}
{
}

/*
 * Parser implementation.
 */

std::unique_ptr<ast::expression> parser::parse_top_level_statement()
{
    if(current_token->s == "import")
    {
        return parse_import();
    }
    else if(current_token->s == "struct")
    {
        return parse_struct();
    }
    else if(current_token->s == "let")
    {
        return parse_variable();
    }
    else if(current_token->s == "fn")
    {
        return parse_definition();
    }
    else if(current_token->s == "#")
    {
        return parse_directive();
    }
    else
    {
        throw syntax_error(*current_token, fmt::format("Unexpected token '{}'", current_token->s));
    }
}

// import ::= 'import' path_expr ';'
// path_expr ::= path | path '::' path_expr
std::unique_ptr<ast::import_expression> parser::parse_import()
{
    get_next_token();    // skip "import" token.

    // parse path.
    std::vector<token> import_path;
    while(true)
    {
        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(*current_token, fmt::format("Expected <identifier>, got '{}'.", current_token->s));
        }
        import_path.emplace_back(*current_token);

        token last_token = *current_token;    // store token for error reporting
        get_next_token();

        if(current_token == std::nullopt)
        {
            throw syntax_error(last_token, "Expected ';'.");
        }
        else if(current_token->s == ";")
        {
            break;
        }
        else if(current_token->s == "::")
        {
            get_next_token();    // skip "::"
            continue;
        }
        else
        {
            throw syntax_error(last_token, fmt::format("Expected ';', got '{}'.", last_token.s));
        }
    }

    return std::make_unique<ast::import_expression>(std::move(import_path));
}

// prototype ::= 'fn' identifier '(' args ')' -> return_type
// args ::= identifier ':' type_id | identifier ':' type_id ',' args
std::unique_ptr<ast::prototype_ast> parser::parse_prototype()
{
    token_location loc = current_token->location;
    get_next_token();    // skip "fn" token.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, "Expected <identifier>.");
    }

    token name = *current_token;
    validate_identifier_name(name);

    get_next_token();
    if(current_token->s != "(")
    {
        throw syntax_error(*current_token, fmt::format("Expected '(', got '{}'.", current_token->s));
    }

    get_next_token();
    std::vector<std::tuple<token, token, bool>> args;
    while(true)
    {
        if(current_token->type != token_type::identifier)
        {
            break;
        }

        token arg_name = *current_token;
        validate_identifier_name(arg_name);

        get_next_token();
        if(current_token->s != ":")
        {
            throw syntax_error(*current_token, fmt::format("Expected ':', got '{}'.", current_token->s));
        }

        get_next_token();

        auto [arg_type, is_array_type] = parse_type_name();
        args.emplace_back(std::make_tuple(std::move(arg_name), std::move(arg_type), is_array_type));

        get_next_token();
        if(current_token->s != ",")
        {
            break;
        }

        get_next_token();    // skip ","
    }

    if(current_token->s != ")")
    {
        throw syntax_error(*current_token, fmt::format("Expected ')', got '{}'.", current_token->s));
    }
    get_next_token();    // skip ')'

    if(current_token->s != "->")
    {
        throw syntax_error(*current_token, fmt::format("Expected '->', got '{}'.", current_token->s));
    }
    get_next_token();    // skip '->'

    auto return_type = parse_type_name();
    get_next_token();

    return std::make_unique<ast::prototype_ast>(std::move(loc), std::move(name), std::move(args), std::move(return_type));
}

// function ::= prototype ';'
//            | prototype block_expr
std::unique_ptr<ast::function_expression> parser::parse_definition()
{
    token_location loc = current_token->location;
    std::unique_ptr<ast::prototype_ast> proto = parse_prototype();
    if(current_token == std::nullopt)
    {
        throw syntax_error("Unexpected end of file.");
    }

    // check if we have a function body.
    if(current_token->s == ";")
    {
        return std::make_unique<ast::function_expression>(std::move(loc), std::move(proto), nullptr);
    }
    return std::make_unique<ast::function_expression>(std::move(loc), std::move(proto), parse_block(false));
}

// variable_decl ::= 'let' identifier ':' identifier
//                 | 'let' identifier ':' identifier = expression
//                 | 'let' identifier ':' [identifier] = expression
std::unique_ptr<ast::variable_declaration_expression> parser::parse_variable()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'let'.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, "Expected <identifier>.");
    }

    token name = *current_token;
    validate_identifier_name(name);

    get_next_token();
    if(current_token->s != ":")
    {
        throw syntax_error(*current_token, fmt::format("Expected ': <identifier>', got '{}'.", current_token->s));
    }

    get_next_token();
    if(current_token->type != token_type::identifier && current_token->s != "[")
    {
        throw syntax_error(*current_token, fmt::format("Expected '<identifier>' or '[<identifier>; <length>]', got '{}'.", current_token->s));
    }

    auto [type, array] = parse_type_name();
    get_next_token();

    if(current_token->s == ";")
    {
        return std::make_unique<ast::variable_declaration_expression>(std::move(loc), std::move(name), std::move(type), array, nullptr);
    }
    else if(current_token->s == "=")
    {
        get_next_token();    // skip '='.
        return std::make_unique<ast::variable_declaration_expression>(std::move(loc), std::move(name), std::move(type), array, parse_expression());
    }

    throw syntax_error(*current_token, fmt::format("Expected '=', got '{}'.", current_token->s));
}

std::pair<token, bool> parser::parse_type_name()
{
    token type = *current_token;
    bool is_array_type{false};

    if(current_token->s == "[")
    {
        // parse array definition.
        get_next_token();
        if(current_token->type != token_type::identifier)
        {
            if(!parsing_native || current_token->s != "]")
            {
                throw syntax_error(*current_token, fmt::format("Expected '<identifier>', got '{}'.", current_token->s));
            }

            type = token{"@array", current_token->location};
        }
        else
        {
            is_array_type = true;

            type = *current_token;
            validate_base_type(type);

            get_next_token();
            if(current_token->s != "]")
            {
                throw syntax_error(*current_token, fmt::format("Expected ']', got '{}'.", current_token->s));
            }
        }
    }

    return std::make_pair(type, is_array_type);
}

// array_initializer_expr ::= '[' exprs ']'
// exprs ::= expression ',' exprs
//         | expression
std::unique_ptr<ast::array_initializer_expression> parser::parse_array_initializer_expression()
{
    token_location loc = current_token->location;
    get_next_token();    // skip '['.

    std::vector<std::unique_ptr<ast::expression>> exprs;
    while(true)
    {
        exprs.emplace_back(parse_expression());

        if(current_token->s == "]")
        {
            // skip ']';
            get_next_token();

            break;
        }
        else if(current_token->s == ",")
        {
            get_next_token();    // skip ';'.
        }
        else
        {
            throw syntax_error(*current_token, fmt::format("Expected ',' or ']', got '{}'.", current_token->s));
        }
    }

    return std::make_unique<ast::array_initializer_expression>(std::move(loc), std::move(exprs));
}

// struct_expr ::= 'struct' identifier '{' variable_declaration* '}'
// variable_declaration ::= identifier ':' type
std::unique_ptr<ast::struct_definition_expression> parser::parse_struct()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'struct'.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, fmt::format("Expected '<identifier>', got '{}'.", current_token->s));
    }

    token name = *current_token;
    validate_identifier_name(name);

    get_next_token();
    if(current_token->s != "{")
    {
        throw syntax_error(*current_token, fmt::format("Expected '{{', got '{}'.", current_token->s));
    }

    get_next_token();    // skip '{'.
    std::vector<std::unique_ptr<ast::variable_declaration_expression>> members;
    while(true)
    {
        if(current_token->type == token_type::identifier)
        {
            token member_name = *current_token;
            validate_identifier_name(member_name);

            get_next_token();

            if(current_token->s != ":")
            {
                throw syntax_error(*current_token, fmt::format("Expected ': <identifier>', got '{}'.", current_token->s));
            }
            get_next_token();    // skip ':'.

            if(current_token->type != token_type::identifier && current_token->s != "[")
            {
                throw syntax_error(*current_token, fmt::format("Expected '<identifier>' or '[<identifier>; <length>]', got '{}'.", current_token->s));
            }

            auto [member_type, is_array_type] = parse_type_name();
            get_next_token();

            members.emplace_back(std::make_unique<ast::variable_declaration_expression>(std::move(loc), std::move(member_name), std::move(member_type), is_array_type, nullptr));
        }

        if(current_token->s == "}")
        {
            break;
        }
        else if(current_token->s != ",")
        {
            throw syntax_error(*current_token, fmt::format("Expected '}}' or ',', got '{}'.", current_token->s));
        }
        get_next_token();    // skip ','.
    }

    get_next_token();    // skip "}"
    return std::make_unique<ast::struct_definition_expression>(std::move(loc), std::move(name), std::move(members));
}

// directive ::= '#[' directive '(' args ')' ']'
// args ::= (arg_name '=' arg_value)
//        | (arg_name '=' arg_value) ',' args
std::unique_ptr<ast::directive_expression> parser::parse_directive()
{
    token_location loc = current_token->location;
    get_next_token();    // skip '#'.

    if(current_token->s != "[")
    {
        throw syntax_error(*current_token, fmt::format("Expected '[', got '{}'.", current_token->s));
    }
    get_next_token();

    token name = *current_token;
    if(name.type != token_type::identifier)
    {
        throw syntax_error(name, fmt::format("Expected <identifier> as directive name, got '{}'.", name.s));
    }
    get_next_token();

    // parse arguments (if any).
    std::vector<std::pair<token, token>> args;
    if(current_token->s == "(")
    {
        get_next_token();    // skip '('
        while(current_token->s != ")")
        {
            token key = *current_token;
            if(key.type != token_type::identifier)
            {
                throw syntax_error(key, fmt::format("Expected <identifier> as a key in directive."));
            }
            get_next_token();

            if(current_token->s != "=")
            {
                throw syntax_error(*current_token, fmt::format("Expected '=', got '{}'.", current_token->s));
            }
            get_next_token();

            token value = *current_token;
            if(value.type != token_type::fp_literal
               && value.type != token_type::int_literal
               && value.type != token_type::str_literal
               && value.type != token_type::identifier)
            {
                throw syntax_error(value, fmt::format("Value in directive can only be an i32-, f32- or string literal, or an identifier."));
            }
            get_next_token();

            args.emplace_back(std::make_pair(std::move(key), std::move(value)));
        }
        get_next_token();    // skip ')'
    }

    if(current_token->s != "]")
    {
        throw syntax_error(*current_token, fmt::format("Expected ']', got '{}'.", current_token->s));
    }
    get_next_token();    // skip ']'

    // evaluate expression within the context of this directive.
    push_directive(name, args);
    auto expr = std::make_unique<ast::directive_expression>(std::move(name), std::move(args), parse_top_level_statement());
    pop_directive();

    return expr;
}

// block_expr ::= '{' stmts_exprs '}'
// stmts_exprs ::= (statement | expression ';')
//               | (statement | expression ';') stmts_exprs
// statement ::= if_statement
//             | while_statement
//             | break_statement
//             | continue_statement
//             | return_statement
std::unique_ptr<ast::block> parser::parse_block(bool skip_closing_brace)
{
    token_location loc = current_token->location;
    if(current_token->s != "{")
    {
        throw syntax_error(*current_token, fmt::format("Expected '{{', got '{}'.", current_token->s));
    }
    get_next_token();

    std::vector<std::unique_ptr<ast::expression>> stmts_exprs;
    while(current_token->s != "}")
    {
        if(current_token->s == ";")
        {
            get_next_token();
        }
        else if(current_token->s == "let")
        {
            stmts_exprs.emplace_back(parse_variable());
        }
        else if(current_token->s == "if")
        {
            stmts_exprs.emplace_back(parse_if());
        }
        else if(current_token->s == "while")
        {
            stmts_exprs.emplace_back(parse_while());
        }
        else if(current_token->s == "break")
        {
            stmts_exprs.emplace_back(parse_break());
        }
        else if(current_token->s == "continue")
        {
            stmts_exprs.emplace_back(parse_continue());
        }
        else if(current_token->s == "return")
        {
            stmts_exprs.emplace_back(parse_return());
        }
        else if(is_keyword(current_token->s))
        {
            throw syntax_error(*current_token, fmt::format("Unexpected keyword '{}'.", current_token->s));
        }
        else if(current_token->type == token_type::identifier)
        {
            stmts_exprs.emplace_back(parse_expression());

            if(current_token->s != ";")
            {
                throw syntax_error(*current_token, fmt::format("Expected ';', got '{}'.", current_token->s));
            }
            get_next_token();
        }
        else
        {
            throw syntax_error(*current_token, fmt::format("Expected <expression> or <statement>, got '{}'.", current_token->s));
        }
    }

    if(skip_closing_brace)
    {
        get_next_token(false);    // skip "}". (We might hit the end of the input.)
    }

    return std::make_unique<ast::block>(std::move(loc), std::move(stmts_exprs));
}

// primary_expr ::= identifier_expr | literal_expr | paren_expr
//                | (identifier_expr | literal_expr | paren_expr) as type_expr
std::unique_ptr<ast::expression> parser::parse_primary()
{
    std::unique_ptr<ast::expression> expr;

    if(current_token->type == token_type::identifier)
    {
        expr = parse_identifier_expression();
    }
    else if(current_token->type == token_type::int_literal
            || current_token->type == token_type::fp_literal
            || current_token->type == token_type::str_literal)
    {
        expr = parse_literal_expression();
    }
    else if(current_token->s == "(")
    {
        expr = parse_paren_expression();
    }
    else if(current_token->s == "[")
    {
        expr = parse_array_initializer_expression();
    }
    else
    {
        throw syntax_error(*current_token, fmt::format("Expected <primary-expression>, got '{}'.", current_token->s));
    }

    if(current_token->s == "as")
    {
        expr = parse_type_cast_expression(std::move(expr));
    }

    return expr;
}

/** Binary operator precedences. */
static std::unordered_map<std::string, int> bin_op_precedence = {
  // clang-format off
  {"::", 13},
  {".", 12}, {"->", 12},
  {"*", 11}, {"/", 11}, {"%", 11},
  {"+", 10}, {"-", 10},
  {"<<", 9}, {">>", 9},
  {"<", 8}, {"<=", 8}, {">", 8}, {">=", 8},
  {"==", 7}, {"!=", 7},
  {"&", 6},
  {"^", 5},
  {"|", 4},
  {"&&", 3},
  {"||", 2},
  {"=", 1}, {"+=", 1}, {"-=", 1}, {"*=", 1}, {"/=", 1}, {"%=", 1},
  {"<<=", 1}, {">>=", 1}, {"&=", 1}, {"^=", 1}, {"|=", 1},
  // clang-format on
};

int parser::get_token_precedence() const
{
    if(current_token == std::nullopt)
    {
        return -1;
    }

    auto it = bin_op_precedence.find(current_token->s);
    if(it == bin_op_precedence.end())
    {
        return -1;
    }

    return it->second;
}

/** Binary operator associativities. */
static std::unordered_map<std::string, associativity> bin_op_associativity = {
  {"::", associativity::left_to_right},
  {".", associativity::left_to_right},
  {"*", associativity::left_to_right},
  {"/", associativity::left_to_right},
  {"%", associativity::left_to_right},
  {"+", associativity::left_to_right},
  {"-", associativity::left_to_right},
  {"<<", associativity::left_to_right},
  {">>", associativity::left_to_right},
  {"<", associativity::left_to_right},
  {"<=", associativity::left_to_right},
  {">", associativity::left_to_right},
  {">=", associativity::left_to_right},
  {"==", associativity::left_to_right},
  {"!=", associativity::left_to_right},
  {"&", associativity::left_to_right},
  {"^", associativity::left_to_right},
  {"|", associativity::left_to_right},
  {"&&", associativity::left_to_right},
  {"||", associativity::left_to_right},
  {"=", associativity::right_to_left},
  {"+=", associativity::right_to_left},
  {"-=", associativity::right_to_left},
  {"*=", associativity::right_to_left},
  {"/=", associativity::right_to_left},
  {"%=", associativity::right_to_left},
  {"<<=", associativity::right_to_left},
  {">>=", associativity::right_to_left},
  {"&=", associativity::right_to_left},
  {"^=", associativity::right_to_left},
  {"|=", associativity::right_to_left},
};

std::optional<associativity> parser::get_token_associativity() const
{
    if(current_token == std::nullopt)
    {
        return std::nullopt;
    }

    auto it = bin_op_associativity.find(current_token->s);
    if(it == bin_op_associativity.end())
    {
        return std::nullopt;
    }

    return it->second;
}

// binoprhs ::= ('+' primary)*
std::unique_ptr<ast::expression> parser::parse_bin_op_rhs(int prec, std::unique_ptr<ast::expression> lhs)
{
    while(true)
    {
        token_location loc = current_token->location;

        int tok_prec = get_token_precedence();
        if(tok_prec < prec)
        {
            return lhs;
        }

        token bin_op = *current_token;
        get_next_token();

        std::unique_ptr<ast::expression> rhs = parse_unary();

        int next_prec = get_token_precedence();
        std::optional<associativity> assoc = get_token_associativity();
        if(tok_prec < next_prec || (tok_prec == next_prec && assoc && *assoc == associativity::right_to_left))
        {
            rhs = parse_bin_op_rhs(tok_prec, std::move(rhs));
        }

        lhs = std::make_unique<ast::binary_expression>(std::move(loc), std::move(bin_op), std::move(lhs), std::move(rhs));
    }
}

// unary ::= primary
//         | ('++' | '--' | '-' | '+' | '~' | '!') primary
//         | 'new' primary
std::unique_ptr<ast::expression> parser::parse_unary()
{
    token op = *current_token;
    token_location loc = current_token->location;

    // parse the new operator.
    if(op.s == "new")
    {
        return parse_new();
    }

    // if we're not parsing a unary operator, it must be a primary expression.
    if(op.s != "++" && op.s != "--" && op.s != "+" && op.s != "-" && op.s != "~" && op.s != "!")
    {
        return parse_primary();
    }

    get_next_token();
    return std::make_unique<ast::unary_ast>(current_token->location, std::move(op), parse_unary());
}

// new_expr ::= 'new' identifier '[' expr ']'
std::unique_ptr<ast::expression> parser::parse_new()
{
    token new_token = *current_token;
    get_next_token();

    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, fmt::format("Expected <identifier> got '{}'.", current_token->s));
    }
    token type = *current_token;
    get_next_token();

    if(current_token->s != "[")
    {
        throw syntax_error(*current_token, fmt::format("Expected '[', got '{}'.", current_token->s));
    }
    get_next_token();    // skip "["

    auto expr = parse_expression();

    if(current_token->s != "]")
    {
        throw syntax_error(*current_token, fmt::format("Expected ']', got '{}'.", current_token->s));
    }
    get_next_token();    // skip "]"

    return std::make_unique<ast::new_expression>(current_token->location, std::move(type), std::move(expr));
}

// identifierexpr ::= identifier
//                  | identifier ('++' | '--')
//                  | identifier '[' primary ']'
//                  | identifier '(' expression* ')'
//                  | identifier '(' expression* ')' '[' primary ']'
//                  | identifier '::' identifierexpr
//                  | identifier '.' identifierexpr
//                  | identifier '{' primary* '}'
//                  | identifier '{' (identifier ':' primary_expr)* '}'
std::unique_ptr<ast::expression> parser::parse_identifier_expression()
{
    token identifier = *current_token;
    get_next_token();    // skip identifier

    if(current_token->s == "++" || current_token->s == "--")    // postfix operators.
    {
        token postfix_op = *current_token;
        get_next_token();

        return std::make_unique<ast::postfix_expression>(
          std::make_unique<ast::variable_reference_expression>(std::move(identifier)), std::move(postfix_op));
    }
    else if(current_token->s == "(")    // function call.
    {
        get_next_token();    // skip "("

        std::vector<std::unique_ptr<ast::expression>> args;
        if(current_token->s != ")")
        {
            while(true)
            {
                args.emplace_back(std::move(parse_expression()));

                if(current_token->s == ")")
                {
                    break;
                }

                if(current_token->s != ",")
                {
                    throw syntax_error(*current_token, "Expected ')' or ','.");
                }
                get_next_token();
            }
        }
        get_next_token();    // skip ")"

        if(current_token->s == "[")    // array access.
        {
            get_next_token();    // skip '['
            auto index_expression = parse_expression();
            if(current_token->s != "]")
            {
                throw syntax_error(*current_token, fmt::format("Expected ']', got '{}'.", current_token->s));
            }

            get_next_token();    // skip ']'

            return std::make_unique<ast::call_expression>(std::move(identifier), std::move(args), std::move(index_expression));
        }

        return std::make_unique<ast::call_expression>(std::move(identifier), std::move(args));
    }
    else if(current_token->s == "::")    // namespace access
    {
        get_next_token();    // skip "::"

        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(*current_token, "Expected <identifier>.");
        }

        return std::make_unique<ast::scope_expression>(std::move(identifier), std::move(parse_identifier_expression()));
    }
    else if(current_token->s == ".")    // element access
    {
        get_next_token();    // skip "."

        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(*current_token, "Expected <identifier>.");
        }

        return std::make_unique<ast::access_expression>(std::move(identifier), std::move(parse_identifier_expression()));
    }
    else if(current_token->s == "{")    // initializer list
    {
        get_next_token();    // skip '{'

        std::vector<std::unique_ptr<ast::expression>> initializers;
        std::vector<std::unique_ptr<ast::expression>> member_names;

        bool named_initializers = false;
        if(current_token->s != "}")
        {
            while(true)
            {
                auto initializer_expr = parse_expression();

                if(!named_initializers)
                {
                    if(current_token->s == ":")
                    {
                        if(initializers.size() != 0)
                        {
                            throw syntax_error(*current_token, "Unexpected ':' in anonymous struct initialization.");
                        }
                        named_initializers = true;
                    }
                    else
                    {
                        initializers.emplace_back(std::move(initializer_expr));
                    }
                }

                if(named_initializers)
                {
                    member_names.emplace_back(std::move(initializer_expr));

                    get_next_token();    // skip ':'
                    initializers.emplace_back(parse_expression());
                }

                if(current_token->s == "}")
                {
                    break;
                }
                else if(current_token->s != ",")
                {
                    throw syntax_error(*current_token, "Expected '}}' or ','.");
                }
                get_next_token();
            }
        }
        get_next_token();    // skip "}"

        if(named_initializers)
        {
            return std::make_unique<ast::struct_named_initializer_expression>(std::move(identifier), std::move(member_names), std::move(initializers));
        }
        else
        {
            return std::make_unique<ast::struct_anonymous_initializer_expression>(std::move(identifier), std::move(initializers));
        }
    }
    else if(current_token->s == "[")    // array access.
    {
        get_next_token();    // skip '['
        auto index_expression = parse_expression();
        if(current_token->s != "]")
        {
            throw syntax_error(*current_token, fmt::format("Expected ']', got '{}'.", current_token->s));
        }

        get_next_token();    // skip ']'

        return std::make_unique<ast::variable_reference_expression>(std::move(identifier), std::move(index_expression));
    }

    // variable reference
    return std::make_unique<ast::variable_reference_expression>(std::move(identifier));
}

// literal_expression ::= int_literal | fp_literal | string_literal
std::unique_ptr<ast::literal_expression> parser::parse_literal_expression()
{
    token_location loc = current_token->location;
    token tok = *current_token;
    get_next_token();

    if(tok.value == std::nullopt)
    {
        throw syntax_error(tok, fmt::format("Expected <literal>, got '{}'.", tok.s));
    }

    return std::make_unique<ast::literal_expression>(current_token->location, std::move(tok));
}

std::unique_ptr<ast::expression> parser::parse_paren_expression()
{
    get_next_token();    // skip '('
    auto expr = parse_expression();

    if(current_token->s != ")")
    {
        throw syntax_error(*current_token, "Expected ')'.");
    }
    get_next_token();    // skip ')'

    return expr;
}

// expression ::= unary binoprhs
std::unique_ptr<ast::expression> parser::parse_expression()
{
    return parse_bin_op_rhs(0, parse_unary());
}

std::unique_ptr<ast::expression> parser::parse_type_cast_expression(std::unique_ptr<ast::expression> expr)
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'as'.

    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, fmt::format("Expected <identifier>, got '{}'.", current_token->s));
    }

    // check we're casting to primitive type.
    token type = *current_token;
    if(type.s != "i32" && type.s != "f32")
    {
        throw syntax_error(type, fmt::format("Expected <primitive-type>, got '{}'.", type.s));
    }
    get_next_token();

    return std::make_unique<ast::type_cast_expression>(std::move(loc), std::move(expr), std::move(type));
}

// ifexpr ::= '(' expression ')' block 'else' (ifexpr | block)
std::unique_ptr<ast::if_statement> parser::parse_if()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'if'.
    if(current_token->s != "(")
    {
        throw syntax_error(*current_token, fmt::format("Expected '(', got '{}'.", current_token->s));
    }
    std::unique_ptr<ast::expression> condition = parse_expression();    // '(' expression ')'
    std::unique_ptr<ast::expression> if_block = parse_block();
    std::unique_ptr<ast::expression> else_block{nullptr};

    if(current_token->s == "else")
    {
        get_next_token();
        if(current_token->s == "if")
        {
            else_block = std::move(parse_if());
        }
        else
        {
            else_block = std::move(parse_block());
        }
    }

    return std::make_unique<ast::if_statement>(std::move(loc), std::move(condition), std::move(if_block), std::move(else_block));
}

std::unique_ptr<ast::expression> parser::parse_while()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'while'.
    if(current_token->s != "(")
    {
        throw syntax_error(*current_token, fmt::format("Expected '(', got '{}'.", current_token->s));
    }
    std::unique_ptr<ast::expression> condition = parse_expression();    // '(' expression ')'
    std::unique_ptr<ast::expression> while_block = parse_block();

    return std::make_unique<ast::while_statement>(std::move(loc), std::move(condition), std::move(while_block));
}

std::unique_ptr<ast::expression> parser::parse_break()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'break'.
    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, fmt::format("Expected ';', got '{}'.", current_token->s));
    }
    return std::make_unique<ast::break_statement>(std::move(loc));
}

std::unique_ptr<ast::expression> parser::parse_continue()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'continue'.
    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, fmt::format("Expected ';', got '{}'.", current_token->s));
    }
    return std::make_unique<ast::continue_statement>(std::move(loc));
}

std::unique_ptr<ast::return_statement> parser::parse_return()
{
    token_location loc = current_token->location;
    get_next_token();    // skip 'return'.

    std::unique_ptr<ast::expression> expr{nullptr};
    if(current_token->s != ";")
    {
        expr = parse_expression();
    }

    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, fmt::format("Expected ';', got '{}'.", current_token->s));
    }

    return std::make_unique<ast::return_statement>(std::move(loc), std::move(expr));
}

void parser::push_directive(const token& name, const std::vector<std::pair<token, token>>& args)
{
    if(name.s == "native")
    {
        directive_stack.push_back(std::make_pair(
          name,
          [this, parsing_native = this->parsing_native]()
          {
              this->parsing_native = parsing_native;
          }));

        parsing_native = true;
    }
    else
    {
        // push empty entry to stack.
        directive_stack.push_back(std::make_pair(token{}, []() {}));
    }
}

void parser::pop_directive()
{
    if(directive_stack.size() == 0)
    {
        throw parser_error("Cannot pop directive: empty directive stack.");
    }

    directive_stack.back().second();
    directive_stack.pop_back();
}

void parser::parse(lexer& in_lexer)
{
    m_lexer = &in_lexer;

    std::vector<std::unique_ptr<ast::expression>> exprs;
    while((current_token = get_next_token(false)) != std::nullopt)
    {
        // skip empty statements.
        if(current_token->s == ";")
        {
            continue;
        }

        exprs.emplace_back(std::move(parse_top_level_statement()));
    }

    if(!in_lexer.eof())
    {
        throw parser_error("Not all tokens parsed.");
    }
    else
    {
        ast = std::make_unique<ast::block>(current_token->location, std::move(exprs));
    }
}

}    // namespace slang