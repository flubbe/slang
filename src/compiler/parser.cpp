/**
 * slang - a simple scripting language.
 *
 * the parser. generates an AST from the lexer output.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <set>

#include "shared/type_utils.h"
#include "parser.h"

// FIXME This is not ideal, but we need to access `current_token` a lot in this file,
//       and there are checks for validation.
// NOLINTBEGIN(bugprone-unchecked-optional-access)

namespace ty = slang::typing;

namespace slang
{

/*
 * Keyword list.
 */

// clang-format off
static const std::set<std::string> keywords = {
  "import", 
  "let", 
  "i32", "f32", "str", 
  "void", 
  "as", 
  "struct", "null",
  "fn", "return", 
  "if", "else", 
  "while", "break", "continue",
  "macro"
};
// clang-format on

/** Check whether a given string is a keyword. */
static bool is_keyword(const std::string& s)
{
    return keywords.contains(s);
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
    if(tok.type != token_type::identifier
       && tok.type != token_type::macro_identifier
       && tok.type != token_type::macro_name)
    {
        throw syntax_error(tok, std::format("Expected <identifier>, got '{}'.", tok.s));
    }

    if(is_keyword(tok.s))
    {
        throw syntax_error(tok, std::format("Expected <identifier>, got keyword '{}'.", tok.s));
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
        throw syntax_error(tok, std::format("Expected <type>, got '{}'.", tok.s));
    }

    // check built-in types.
    if(ty::is_builtin_type(tok.s))
    {
        return;
    }

    // check for keywords.
    if(is_keyword(tok.s))
    {
        throw syntax_error(tok, std::format("Expected <type>, got keyword '{}'.", tok.s));
    }
}

/*
 * Exceptions.
 */

syntax_error::syntax_error(const token& tok, const std::string& message)
: std::runtime_error{std::format("{}: {}", to_string(tok.location), message)}
{
}

parser_error::parser_error(const token& tok, const std::string& message)
: std::runtime_error{std::format("{}: {}", to_string(tok.location), message)}
{
}

/*
 * Parser implementation.
 */

std::unique_ptr<ast::expression> parser::parse_top_level_statement()
{
    if(current_token->s == "#")
    {
        auto directive = get_directive();

        // evaluate expression within the context of this directive.
        push_directive(directive.first, directive.second);
        auto expr = std::make_unique<ast::directive_expression>(
          std::move(directive.first),
          std::move(directive.second),
          parse_top_level_statement());
        pop_directive();

        return expr;
    }

    if(current_token->s == "import")
    {
        return parse_import();
    }

    if(current_token->s == "struct")
    {
        return parse_struct();
    }

    if(current_token->s == "let")
    {
        return parse_variable();
    }

    if(current_token->s == "const")
    {
        return parse_const();
    }

    if(current_token->s == "fn")
    {
        return parse_definition();
    }

    if(current_token->s == "macro")
    {
        return parse_macro();
    }

    throw syntax_error(*current_token, std::format("Unexpected token '{}'", current_token->s));
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
            throw syntax_error(*current_token, std::format("Expected <identifier>, got '{}'.", current_token->s));
        }
        import_path.emplace_back(*current_token);

        token last_token = *current_token;    // store token for error reporting
        get_next_token();

        if(!current_token.has_value())
        {
            throw syntax_error(last_token, "Expected ';'.");
        }

        if(current_token->s == ";")
        {
            break;
        }

        if(current_token->s == "::")
        {
            get_next_token();    // skip "::"
            continue;
        }

        throw syntax_error(last_token, std::format("Expected ';', got '{}'.", last_token.s));
    }

    return std::make_unique<ast::import_expression>(std::move(import_path));
}

// prototype ::= 'fn' identifier '(' args ')' -> return_type
// args ::= identifier ':' type_id | identifier ':' type_id ',' args
std::unique_ptr<ast::prototype_ast> parser::parse_prototype()
{
    source_location loc = current_token->location;
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
        throw syntax_error(*current_token, std::format("Expected '(', got '{}'.", current_token->s));
    }

    get_next_token();
    std::vector<std::pair<token, std::unique_ptr<ast::type_expression>>> args;
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
            throw syntax_error(*current_token, std::format("Expected ':', got '{}'.", current_token->s));
        }

        get_next_token();

        auto arg_type = parse_type();
        args.emplace_back(std::move(arg_name), std::move(arg_type));

        if(current_token->s != ",")
        {
            break;
        }

        get_next_token();    // skip ","
    }

    if(current_token->s != ")")
    {
        throw syntax_error(*current_token, std::format("Expected ')', got '{}'.", current_token->s));
    }
    get_next_token();    // skip ')'

    if(current_token->s != "->")
    {
        throw syntax_error(*current_token, std::format("Expected '->', got '{}'.", current_token->s));
    }
    get_next_token();    // skip '->'

    auto return_type = parse_type();
    return std::make_unique<ast::prototype_ast>(loc, std::move(name), std::move(args), std::move(return_type));
}

// function ::= prototype ';'
//            | prototype block_expr
std::unique_ptr<ast::function_expression> parser::parse_definition()
{
    source_location loc = current_token->location;
    std::unique_ptr<ast::prototype_ast> proto = parse_prototype();
    if(!current_token.has_value())
    {
        throw syntax_error("Unexpected end of file.");
    }

    // check if we have a function body.
    if(current_token->s == ";")
    {
        return std::make_unique<ast::function_expression>(loc, std::move(proto), nullptr);
    }
    return std::make_unique<ast::function_expression>(loc, std::move(proto), parse_block(false));
}

// variable_decl ::= 'let' identifier ':' identifier
//                 | 'let' identifier ':' identifier = expression
//                 | 'let' identifier ':' [identifier] = expression
std::unique_ptr<ast::variable_declaration_expression> parser::parse_variable()
{
    source_location loc = current_token->location;
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
        throw syntax_error(*current_token, std::format("Expected ': <identifier>', got '{}'.", current_token->s));
    }

    get_next_token();
    if(current_token->type != token_type::identifier && current_token->s != "[")
    {
        throw syntax_error(*current_token, std::format("Expected '<identifier>' or '[<identifier>; <length>]', got '{}'.", current_token->s));
    }

    auto type = parse_type();
    if(current_token->s == ";")
    {
        return std::make_unique<ast::variable_declaration_expression>(loc, std::move(name), std::move(type), nullptr);
    }

    if(current_token->s == "=")
    {
        get_next_token();    // skip '='.
        return std::make_unique<ast::variable_declaration_expression>(loc, std::move(name), std::move(type), parse_expression());
    }

    throw syntax_error(*current_token, std::format("Expected '=', got '{}'.", current_token->s));
}

// const_def ::= 'const' identifier ':' identifier '=' literal_expression
std::unique_ptr<ast::constant_declaration_expression> parser::parse_const()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'const'.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, "Expected <identifier>.");
    }

    token name = *current_token;
    validate_identifier_name(name);

    get_next_token();
    if(current_token->s != ":")
    {
        throw syntax_error(*current_token, std::format("Expected ': <identifier>', got '{}'.", current_token->s));
    }

    get_next_token();
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, std::format("Expected '<identifier>', got '{}'.", current_token->s));
    }

    auto type = parse_type();
    if(current_token->s != "=")
    {
        throw syntax_error(*current_token, std::format("Expected '=' for constant initialization, got '{}'.", current_token->s));
    }

    get_next_token();    // skip '='.

    return std::make_unique<ast::constant_declaration_expression>(
      loc,
      std::move(name),
      std::move(type),
      parse_expression());
}

std::unique_ptr<ast::type_expression> parser::parse_type()
{
    bool is_array_type{false};
    auto location = current_token->location;

    if(current_token->s == "[")
    {
        // parse array definition.
        get_next_token();
        is_array_type = true;
    }

    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, std::format("Expected '<identifier>', got '{}'.", current_token->s));
    }

    std::vector<token> components;
    while(true)
    {
        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(*current_token, std::format("Expected '<identifier>', got '{}'.", current_token->s));
        }

        components.emplace_back(*current_token);
        get_next_token();

        if(current_token->s != "::")
        {
            break;
        }

        get_next_token();
    }

    if(current_token->s == "]")
    {
        if(!is_array_type)
        {
            throw syntax_error(*current_token, std::format("Expected ']', got '{}'.", current_token->s));
        }

        get_next_token();
    }

    validate_base_type(components.back());

    token type = components.back();
    components.pop_back();

    return std::make_unique<ast::type_expression>(location, std::move(type), std::move(components), is_array_type);
}

// array_initializer_expr ::= '[' exprs ']'
// exprs ::= expression ',' exprs
//         | expression
std::unique_ptr<ast::array_initializer_expression> parser::parse_array_initializer_expression()
{
    source_location loc = current_token->location;
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

        if(current_token->s == ",")
        {
            get_next_token();    // skip ';'.
        }
        else
        {
            throw syntax_error(*current_token, std::format("Expected ',' or ']', got '{}'.", current_token->s));
        }
    }

    return std::make_unique<ast::array_initializer_expression>(loc, std::move(exprs));
}

// struct_expr ::= 'struct' identifier '{' variable_declaration* '}'
// variable_declaration ::= identifier ':' type
std::unique_ptr<ast::struct_definition_expression> parser::parse_struct()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'struct'.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, std::format("Expected '<identifier>', got '{}'.", current_token->s));
    }

    token name = *current_token;
    validate_identifier_name(name);

    get_next_token();
    if(current_token->s != "{")
    {
        throw syntax_error(*current_token, std::format("Expected '{{', got '{}'.", current_token->s));
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
                throw syntax_error(*current_token, std::format("Expected ': <identifier>', got '{}'.", current_token->s));
            }
            get_next_token();    // skip ':'.

            if(current_token->type != token_type::identifier && current_token->s != "[")
            {
                throw syntax_error(*current_token, std::format("Expected '<identifier>' or '[<identifier>; <length>]', got '{}'.", current_token->s));
            }

            auto member_type = parse_type();
            members.emplace_back(std::make_unique<ast::variable_declaration_expression>(loc, std::move(member_name), std::move(member_type), nullptr));
        }

        if(current_token->s == "}")
        {
            break;
        }

        if(current_token->s != ",")
        {
            throw syntax_error(*current_token, std::format("Expected '}}' or ',', got '{}'.", current_token->s));
        }

        get_next_token();    // skip ','.
    }

    get_next_token();    // skip "}"
    return std::make_unique<ast::struct_definition_expression>(loc, std::move(name), std::move(members));
}

// directive ::= '#[' directive '(' args ')' ']'
// args ::= (arg_name '=' arg_value)
//        | (arg_name '=' arg_value) ',' args
std::pair<token, std::vector<std::pair<token, token>>> parser::get_directive()
{
    get_next_token();    // skip '#'.

    if(current_token->s != "[")
    {
        throw syntax_error(*current_token, std::format("Expected '[', got '{}'.", current_token->s));
    }
    get_next_token();

    token name = *current_token;
    if(name.type != token_type::identifier)
    {
        throw syntax_error(name, std::format("Expected <identifier> as directive name, got '{}'.", name.s));
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
                throw syntax_error(key, std::format("Expected <identifier> as a key in directive."));
            }
            get_next_token();

            token value;
            if(current_token->s != "=")
            {
                // only key, no value.
            }
            else
            {
                // key-value pair.
                get_next_token();

                value = *current_token;
                if(value.type != token_type::fp_literal
                   && value.type != token_type::int_literal
                   && value.type != token_type::str_literal
                   && value.type != token_type::identifier)
                {
                    throw syntax_error(value, std::format("Value in directive can only be an i32-, f32- or string literal, or an identifier."));
                }
                get_next_token();
            }

            args.emplace_back(std::move(key), std::move(value));
        }
        get_next_token();    // skip ')'
    }

    if(current_token->s != "]")
    {
        throw syntax_error(*current_token, std::format("Expected ']', got '{}'.", current_token->s));
    }
    get_next_token();    // skip ']'

    return std::make_pair(std::move(name), std::move(args));
}

// block_expr ::= '{' stmts_exprs '}'
// stmts_exprs ::= block_expr
//               | (statement | expression ';')
//               | (statement | expression ';') stmts_exprs
// statement ::= if_statement
//             | while_statement
//             | break_statement
//             | continue_statement
//             | return_statement
std::unique_ptr<ast::block> parser::parse_block(bool skip_closing_brace)
{
    source_location loc = current_token->location;
    if(current_token->s != "{")
    {
        throw syntax_error(*current_token, std::format("Expected '{{', got '{}'.", current_token->s));
    }
    get_next_token();

    std::vector<std::unique_ptr<ast::expression>> stmts_exprs;
    while(current_token->s != "}")
    {
        if(current_token->s == "#")
        {
            auto directive = get_directive();

            // evaluate expression within the context of this directive.
            push_directive(directive.first, directive.second);
            auto expr = parse_block_stmt_expr();
            pop_directive();

            if(expr)
            {
                stmts_exprs.emplace_back(std::make_unique<ast::directive_expression>(
                  std::move(directive.first),
                  std::move(directive.second),
                  std::move(expr)));
            }
        }
        else if(current_token->s == "{")
        {
            stmts_exprs.emplace_back(parse_block());
        }
        else
        {
            auto expr = parse_block_stmt_expr();
            if(expr)
            {
                stmts_exprs.emplace_back(std::move(expr));
            }
        }
    }

    if(skip_closing_brace)
    {
        get_next_token(false);    // skip "}". (We might hit the end of the input.)
    }

    return std::make_unique<ast::block>(loc, std::move(stmts_exprs));
}

std::unique_ptr<ast::expression> parser::parse_block_stmt_expr()
{
    if(current_token->s == ";")
    {
        get_next_token();
        return {};
    }

    if(current_token->s == "let")
    {
        return parse_variable();
    }

    if(current_token->s == "if")
    {
        return parse_if();
    }

    if(current_token->s == "while")
    {
        return parse_while();
    }

    if(current_token->s == "break")
    {
        return parse_break();
    }

    if(current_token->s == "continue")
    {
        return parse_continue();
    }

    if(current_token->s == "return")
    {
        return parse_return();
    }

    if(is_keyword(current_token->s))
    {
        throw syntax_error(*current_token, std::format("Unexpected keyword '{}'.", current_token->s));
    }

    auto expr = parse_expression();

    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, std::format("Expected ';', got '{}'.", current_token->s));
    }
    get_next_token();

    return expr;
}

// primary_expr ::= identifier_expr | literal_expr | paren_expr
//                | (identifier_expr | literal_expr | paren_expr) as type_expr
std::unique_ptr<ast::expression> parser::parse_primary()
{
    std::unique_ptr<ast::expression> expr;

    if(current_token->type == token_type::identifier
       || current_token->type == token_type::macro_identifier
       || current_token->type == token_type::macro_name)
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
        throw syntax_error(*current_token, std::format("Expected <primary-expression>, got '{}'.", current_token->s));
    }

    return expr;
}

/** Binary operator precedences. */
static const std::unordered_map<std::string, int> bin_op_precedence = {
  // clang-format off
  {"::", 13},
  {".", 12},
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
    if(!current_token.has_value())
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
static const std::unordered_map<std::string, associativity> bin_op_associativity = {
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
    if(!current_token.has_value())
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
        source_location loc = current_token->location;

        int tok_prec = get_token_precedence();
        if(tok_prec < prec)
        {
            return lhs;
        }

        token bin_op = *current_token;
        get_next_token();

        bool is_access = (bin_op.s == ".");
        std::unique_ptr<ast::expression> rhs = parse_unary(is_access);

        int next_prec = get_token_precedence();
        std::optional<associativity> assoc = get_token_associativity();
        if(tok_prec < next_prec || (tok_prec == next_prec && assoc && *assoc == associativity::right_to_left))
        {
            rhs = parse_bin_op_rhs(tok_prec, std::move(rhs));
        }

        // special case for access expressions, since we treat them separately.
        if(is_access)
        {
            lhs = std::make_unique<ast::access_expression>(std::move(lhs), std::move(rhs));

            if(current_token->s == "as")
            {
                lhs = parse_type_cast_expression(std::move(lhs));
            }
        }
        else
        {
            lhs = std::make_unique<ast::binary_expression>(loc, std::move(bin_op), std::move(lhs), std::move(rhs));
        }
    }
}

// unary ::= primary
//         | ('++' | '--' | '-' | '+' | '~' | '!') primary
//         | 'new' primary
//         | 'null'
std::unique_ptr<ast::expression> parser::parse_unary(bool ignore_type_cast)
{
    std::unique_ptr<ast::expression> expr;
    auto location = current_token->location;

    if(current_token->s == "new")
    {
        // parse the new operator.
        expr = parse_new();
    }
    else if(current_token->s == "null")
    {
        // parse 'null'.
        get_next_token();
        expr = std::make_unique<ast::null_expression>(location);
    }
    else
    {
        // if we're not parsing a unary operator, it must be a primary expression.
        token op = *current_token;
        if(op.s != "++" && op.s != "--" && op.s != "+" && op.s != "-" && op.s != "~" && op.s != "!")
        {
            expr = parse_primary();
        }
        else
        {
            get_next_token();
            expr = std::make_unique<ast::unary_expression>(location, std::move(op), parse_unary());
        }
    }

    if(!ignore_type_cast && current_token->s == "as")
    {
        expr = parse_type_cast_expression(std::move(expr));
    }

    return expr;
}

// new_expr ::= 'new' type_expr '[' expr ']'
std::unique_ptr<ast::expression> parser::parse_new()
{
    token new_token = *current_token;
    auto location = current_token->location;
    get_next_token();

    auto type_expr = parse_type();
    if(current_token->s != "[")
    {
        throw syntax_error(*current_token, std::format("Expected '[', got '{}'.", current_token->s));
    }
    get_next_token();    // skip "["

    auto expr = parse_expression();

    if(current_token->s != "]")
    {
        throw syntax_error(*current_token, std::format("Expected ']', got '{}'.", current_token->s));
    }
    get_next_token();    // skip "]"

    return std::make_unique<ast::new_expression>(location, std::move(type_expr), std::move(expr));
}

// identifierexpr ::= identifier
//                  | identifier ('++' | '--')
//                  | identifier '[' primary ']'
//                  | (identifier | macro_name) '(' expression* ')'
//                  | (identifier | macro_name) '(' expression* ')' '[' primary ']'
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

    if(current_token->s == "(")    // function call or macro invocation.
    {
        get_next_token();    // skip "("

        std::vector<std::unique_ptr<ast::expression>> args;
        if(current_token->s != ")")
        {
            while(true)
            {
                args.emplace_back(parse_expression());

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
                throw syntax_error(*current_token, std::format("Expected ']', got '{}'.", current_token->s));
            }

            get_next_token();    // skip ']'

            if(identifier.type == token_type::macro_name)
            {
                return std::make_unique<ast::macro_invocation>(
                  std::move(identifier), std::move(args), std::move(index_expression));
            }

            return std::make_unique<ast::call_expression>(std::move(identifier), std::move(args), std::move(index_expression));
        }

        if(identifier.type == token_type::macro_name)
        {
            return std::make_unique<ast::macro_invocation>(std::move(identifier), std::move(args));
        }

        return std::make_unique<ast::call_expression>(std::move(identifier), std::move(args));
    }

    if(current_token->s == "::")    // namespace access
    {
        get_next_token();    // skip "::"

        if(current_token->type != token_type::identifier
           && current_token->type != token_type::macro_name)
        {
            throw syntax_error(*current_token, "Expected <identifier>.");
        }

        return std::make_unique<ast::namespace_access_expression>(
          std::move(identifier), parse_identifier_expression());
    }

    if(current_token->s == ".")    // element access
    {
        token access_op = *current_token;
        get_next_token();    // skip "."

        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(*current_token, "Expected <identifier>.");
        }

        return parse_access_expression(
          std::make_unique<ast::variable_reference_expression>(std::move(identifier)));
    }

    if(current_token->s == "{")    // initializer list
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
                        if(!initializers.empty())
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

                if(current_token->s != ",")
                {
                    throw syntax_error(*current_token, "Expected '}}' or ','.");
                }
                get_next_token();
            }
        }
        get_next_token();    // skip "}"

        if(named_initializers)
        {
            std::vector<std::unique_ptr<ast::named_initializer>> named_initializer_vector;
            named_initializer_vector.reserve(member_names.size());
            for(std::size_t i = 0; i < member_names.size(); ++i)
            {
                if(!member_names[i]->is_named_expression())
                {
                    throw parser_error(
                      std::format(
                        "{}: Unnamed member in initializer expression.",
                        ::slang::to_string(member_names[i]->get_location())));
                }

                const auto* named_member_expr = member_names[i]->as_named_expression();
                named_initializer_vector.emplace_back(
                  std::make_unique<ast::named_initializer>(
                    named_member_expr->get_name(), std::move(initializers[i])));
            }

            return std::make_unique<ast::struct_named_initializer_expression>(
              std::move(identifier),
              std::move(named_initializer_vector));
        }

        return std::make_unique<ast::struct_anonymous_initializer_expression>(std::move(identifier), std::move(initializers));
    }

    if(current_token->s == "[")    // array access.
    {
        get_next_token();    // skip '['
        auto index_expression = parse_expression();
        if(current_token->s != "]")
        {
            throw syntax_error(*current_token, std::format("Expected ']', got '{}'.", current_token->s));
        }

        get_next_token();    // skip ']'

        return std::make_unique<ast::variable_reference_expression>(std::move(identifier), std::move(index_expression));
    }

    // variable reference
    return std::make_unique<ast::variable_reference_expression>(std::move(identifier));
}

// access_expression ::= identifier
//                     | identifier '.' access_expression
//                     | '(' identifier 'as' type_expr ')' '.' access_expression
std::unique_ptr<ast::expression> parser::parse_access_expression(std::unique_ptr<ast::expression> lhs)
{
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, "Expected <identifier>.");
    }

    token identifier = *current_token;
    get_next_token();

    if(current_token->s == "as")
    {
        // type cast.
        return parse_type_cast_expression(
          std::make_unique<ast::access_expression>(std::move(lhs),
                                                   std::make_unique<ast::variable_reference_expression>(
                                                     std::move(identifier))));
    }

    if(current_token->s == ".")
    {
        get_next_token();    // skip '.'

        // nested access.
        return std::make_unique<ast::access_expression>(
          std::move(lhs),
          parse_access_expression(
            std::make_unique<ast::variable_reference_expression>(std::move(identifier))));
    }

    return std::make_unique<ast::access_expression>(
      std::move(lhs), std::make_unique<ast::variable_reference_expression>(std::move(identifier)));
}

// literal_expression ::= int_literal | fp_literal | string_literal
std::unique_ptr<ast::literal_expression> parser::parse_literal_expression()
{
    token tok = *current_token;
    get_next_token();

    if(!tok.value.has_value())
    {
        throw syntax_error(tok, std::format("Expected <literal>, got '{}'.", tok.s));
    }

    return std::make_unique<ast::literal_expression>(tok.location, std::move(tok));
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
    source_location loc = current_token->location;
    get_next_token();    // skip 'as'.

    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(*current_token, std::format("Expected <identifier>, got '{}'.", current_token->s));
    }

    return std::make_unique<ast::type_cast_expression>(loc, std::move(expr), parse_type());
}

// ifexpr ::= '(' expression ')' block 'else' (ifexpr | block)
std::unique_ptr<ast::if_statement> parser::parse_if()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'if'.
    if(current_token->s != "(")
    {
        throw syntax_error(*current_token, std::format("Expected '(', got '{}'.", current_token->s));
    }
    std::unique_ptr<ast::expression> condition = parse_expression();    // '(' expression ')'
    std::unique_ptr<ast::expression> if_block = parse_block();
    std::unique_ptr<ast::expression> else_block{nullptr};

    if(current_token->s == "else")
    {
        get_next_token();
        if(current_token->s == "if")
        {
            else_block = parse_if();
        }
        else
        {
            else_block = parse_block();
        }
    }

    return std::make_unique<ast::if_statement>(loc, std::move(condition), std::move(if_block), std::move(else_block));
}

std::unique_ptr<ast::expression> parser::parse_while()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'while'.
    if(current_token->s != "(")
    {
        throw syntax_error(*current_token, std::format("Expected '(', got '{}'.", current_token->s));
    }
    std::unique_ptr<ast::expression> condition = parse_expression();    // '(' expression ')'
    std::unique_ptr<ast::expression> while_block = parse_block();

    return std::make_unique<ast::while_statement>(loc, std::move(condition), std::move(while_block));
}

std::unique_ptr<ast::expression> parser::parse_break()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'break'.
    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, std::format("Expected ';', got '{}'.", current_token->s));
    }
    return std::make_unique<ast::break_statement>(loc);
}

std::unique_ptr<ast::expression> parser::parse_continue()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'continue'.
    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, std::format("Expected ';', got '{}'.", current_token->s));
    }
    return std::make_unique<ast::continue_statement>(loc);
}

std::unique_ptr<ast::return_statement> parser::parse_return()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'return'.

    std::unique_ptr<ast::expression> expr{nullptr};
    if(current_token->s != ";")
    {
        expr = parse_expression();
    }

    if(current_token->s != ";")
    {
        throw syntax_error(*current_token, std::format("Expected ';', got '{}'.", current_token->s));
    }

    return std::make_unique<ast::return_statement>(loc, std::move(expr));
}

std::unique_ptr<ast::macro_expression> parser::parse_macro()
{
    source_location loc = current_token->location;
    get_next_token();    // skip 'macro'.

    if(current_token->type != token_type::macro_name)
    {
        throw syntax_error(*current_token, "Expected <macro-name>.");
    }

    token name = *current_token;
    validate_identifier_name(name);
    get_next_token();

    if(current_token->s != "{")
    {
        throw syntax_error(*current_token, std::format("Expected '{{', got '{}'.", current_token->s));
    }
    get_next_token();

    std::vector<std::unique_ptr<ast::macro_branch>> branches;
    while(current_token->s == "(")
    {
        branches.emplace_back(parse_macro_branch());
    }

    if(current_token->s != "}")
    {
        throw syntax_error(*current_token, std::format("Expected '}}', got '{}'.", current_token->s));
    }

    // don't skip closing brace, as this is done by the caller.

    return std::make_unique<ast::macro_expression>(loc, std::move(name), std::move(branches));
}

std::unique_ptr<ast::macro_branch> parser::parse_macro_branch()
{
    auto location = current_token->location;
    get_next_token();    // skip '('.

    std::vector<std::pair<token, token>> args;
    bool args_end_with_list = false;

    while(current_token->s != ")")
    {
        if(current_token->type != token_type::macro_identifier)
        {
            throw syntax_error(
              *current_token,
              std::format("Expected <macro-identifier>, got '{}'.", current_token->s));
        }
        token arg_name = *current_token;
        get_next_token();

        if(current_token->s != ":")
        {
            throw syntax_error(
              *current_token,
              std::format("Expected ':', got '{}'.", current_token->s));
        }
        get_next_token();

        if(current_token->s != "expr")
        {
            throw syntax_error(
              *current_token,
              std::format("Expected 'expr', got '{}'.", current_token->s));
        }
        token type_name = *current_token;
        get_next_token();

        args.emplace_back(std::move(arg_name), std::move(type_name));

        if(current_token->s == "...")
        {
            // has to be the last token in the argument list.
            args_end_with_list = true;

            get_next_token();
            if(current_token->s != ")")
            {
                throw syntax_error(
                  *current_token,
                  std::format("Expected ')', got '{}'.", current_token->s));
            }

            break;
        }

        if(current_token->s == ",")
        {
            get_next_token();
        }
    }

    // skip ')'.
    get_next_token();

    if(current_token->s != "=>")
    {
        throw syntax_error(
          *current_token,
          std::format("Expected '=>', got '{}'.", current_token->s));
    }
    get_next_token();    // skip "=>".

    auto block = parse_block();

    if(current_token->s != ";")
    {
        throw syntax_error(
          *current_token,
          std::format("Expected ';', got '{}'.", current_token->s));
    }
    get_next_token(false);    // skip ";".

    return std::make_unique<ast::macro_branch>(
      location,
      std::move(args),
      args_end_with_list,
      std::move(block));
}

void parser::push_directive(const token& name, [[maybe_unused]] const std::vector<std::pair<token, token>>& args)
{
    if(name.s == "native")
    {
        directive_stack.emplace_back(
          name,
          [this, parsing_native = this->parsing_native]()
          {
              this->parsing_native = parsing_native;
          });

        parsing_native = true;
    }
    else
    {
        // push empty entry to stack.
        directive_stack.emplace_back(name, []() {});
    }
}

void parser::pop_directive()
{
    if(directive_stack.empty())
    {
        throw parser_error("Cannot pop directive: empty directive stack.");
    }

    directive_stack.back().second();
    directive_stack.pop_back();
}

void parser::parse(lexer& lexer)
{
    current_lexer = &lexer;
    auto start_location = current_lexer->get_location();

    std::vector<std::unique_ptr<ast::expression>> exprs;
    while((current_token = get_next_token(false)).has_value())
    {
        // skip empty statements.
        if(current_token->s == ";")
        {
            continue;
        }

        exprs.emplace_back(parse_top_level_statement());

        // Add macros.
        if(exprs.back()->is_macro_expression())
        {
            macro_asts.emplace_back(exprs.back().get());
        }
    }

    if(!lexer.eof())
    {
        throw parser_error("Not all tokens parsed.");
    }

    ast = std::make_unique<ast::block>(start_location, std::move(exprs));
}

}    // namespace slang

// NOLINTEND(bugprone-unchecked-optional-access)
