/**
 * slang - a simple scripting language.
 *
 * the parser. generates an AST from the lexer output.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "parser.h"

namespace slang
{

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
    else
    {
        throw syntax_error(fmt::format("{}: Unexpected token '{}'", to_string(current_token->location), current_token->s));
    }
}

// import ::= 'import' path_expr ';'
// path_expr ::= path | path '::' path_expr
std::unique_ptr<ast::import_expression> parser::parse_import()
{
    get_next_token();    // skip "import" token.

    // parse path.
    std::vector<std::string> import_path;
    while(true)
    {
        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(fmt::format("{}: Expected <identifier>, got '{}'.", to_string(current_token->location), current_token->s));
        }
        import_path.emplace_back(std::move(current_token->s));

        lexical_token last_token = *current_token;    // store token for error reporting
        get_next_token();

        if(current_token == std::nullopt)
        {
            throw syntax_error(fmt::format("{}: Expected ';'.", to_string(last_token.location)));
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
            throw syntax_error(fmt::format("{}: Expected ';', got '{}'.", to_string(last_token.location), last_token.s));
        }
    }

    return std::make_unique<ast::import_expression>(std::move(import_path));
}

// prototype ::= 'fn' identifier '(' args ')' -> return_type
// args ::= identifier ':' type_id | identifier ':' type_id ',' args
std::unique_ptr<ast::prototype_ast> parser::parse_prototype()
{
    get_next_token();    // skip "fn" token.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(fmt::format("{}: Expected <identifier>.", to_string(current_token->location)));
    }
    std::string name = current_token->s;

    get_next_token();
    if(current_token->s != "(")
    {
        throw syntax_error(fmt::format("{}: Expected '(', got '{}'.", to_string(current_token->location), current_token->s));
    }

    get_next_token();
    std::vector<std::pair<std::string, std::string>> args;
    while(true)
    {
        if(current_token->type != token_type::identifier)
        {
            break;
        }
        std::string arg_name = current_token->s;

        get_next_token();
        if(current_token->s != ":")
        {
            throw syntax_error(fmt::format("{}: Expected ':', got '{}'.", to_string(current_token->location), current_token->s));
        }

        get_next_token();
        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(fmt::format("{}: Expected <identifier>, got '{}'.", to_string(current_token->location), current_token->s));
        }
        std::string arg_type = current_token->s;

        args.emplace_back(std::make_pair(std::move(arg_name), std::move(arg_type)));

        get_next_token();
        if(current_token->s != ",")
        {
            break;
        }

        get_next_token();    // skip ","
    }

    if(current_token->s != ")")
    {
        throw syntax_error(fmt::format("{}: Expected ')', got '{}'.", to_string(current_token->location), current_token->s));
    }
    get_next_token();    // skip ')'

    if(current_token->s != "->")
    {
        throw syntax_error(fmt::format("{}: Expected '->', got '{}'.", to_string(current_token->location), current_token->s));
    }
    get_next_token();    // skip '->'

    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(fmt::format("{}: Expected <identifier>.", to_string(current_token->location)));
    }
    std::string return_type = current_token->s;
    get_next_token();

    return std::make_unique<ast::prototype_ast>(std::move(name), std::move(args), std::move(return_type));
}

// function ::= protoype block_expr
std::unique_ptr<ast::function_expression> parser::parse_definition()
{
    std::unique_ptr<ast::prototype_ast> proto = parse_prototype();
    if(current_token == std::nullopt)
    {
        throw syntax_error(fmt::format("Unexpected end of file."));
    }

    return std::make_unique<ast::function_expression>(std::move(proto), parse_block());
}

// variable_decl ::= 'let' identifier
//                 | 'let' identifier = expression
std::unique_ptr<ast::variable_declaration_expression> parser::parse_variable()
{
    get_next_token();    // skip 'let'.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(fmt::format("{}: Expected <identifier>.", to_string(current_token->location)));
    }
    std::string name = current_token->s;

    get_next_token();
    if(current_token->s != ":")
    {
        throw syntax_error(fmt::format("{}: Expected ': <identifier>', got '{}'.", to_string(current_token->location), current_token->s));
    }

    get_next_token();
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(fmt::format("{}: Expected '<identifier>', got '{}'.", to_string(current_token->location), current_token->s));
    }
    std::string type = current_token->s;

    get_next_token();
    if(current_token->s == ";")
    {
        return std::make_unique<ast::variable_declaration_expression>(std::move(name), std::move(type), nullptr);
    }
    else if(current_token->s == "=")
    {
        get_next_token();    // skip '='.
        return std::make_unique<ast::variable_declaration_expression>(std::move(name), std::move(type), parse_expression());
    }

    throw syntax_error(fmt::format("{}: Expected '=', got '{}'.", to_string(current_token->location), current_token->s));
}

// struct_expr ::= 'struct' identifier '{' variable_declaration* '}'
// variable_declaration ::= identifier ':' type
std::unique_ptr<ast::struct_definition_expression> parser::parse_struct()
{
    get_next_token();    // skip 'struct'.
    if(current_token->type != token_type::identifier)
    {
        throw syntax_error(fmt::format("{}: Expected '<identifier>', got '{}'.", to_string(current_token->location), current_token->s));
    }
    std::string name = current_token->s;

    get_next_token();
    if(current_token->s != "{")
    {
        throw syntax_error(fmt::format("{}: Expected '{{', got '{}'.", to_string(current_token->location), current_token->s));
    }

    get_next_token();    // skip '{'.
    std::vector<std::unique_ptr<ast::variable_declaration_expression>> members;
    while(true)
    {
        if(current_token->type == token_type::identifier)
        {
            std::string member_name = current_token->s;
            get_next_token();

            if(current_token->s != ":")
            {
                throw syntax_error(fmt::format("{}: Expected ': <identifier>', got '{}'.", to_string(current_token->location), current_token->s));
            }
            get_next_token();    // skip ':'.

            if(current_token->type != token_type::identifier)
            {
                throw syntax_error(fmt::format("{}: Expected <identifier>, got '{}'.", to_string(current_token->location), current_token->s));
            }
            std::string member_type = current_token->s;
            get_next_token();

            members.emplace_back(std::make_unique<ast::variable_declaration_expression>(std::move(member_name), std::move(member_type), nullptr));
        }

        if(current_token->s == "}")
        {
            break;
        }
        else if(current_token->s != ",")
        {
            throw syntax_error(fmt::format("{}: Expected '}}' or ',', got '{}'.", to_string(current_token->location), current_token->s));
        }
        get_next_token();    // skip ','.
    }

    get_next_token(false);    // skip "}"
    return std::make_unique<ast::struct_definition_expression>(std::move(name), std::move(members));
}

// block_expr ::= '{' stmts_exprs '}'
// stmts_exprs ::= (statement | expression ';')
//               | (statement | expression ';') stmts_exprs
// statement ::= if_statement
//             | while_statement
//             | break_statement
//             | continue_statement
//             | return_statement
std::unique_ptr<ast::block> parser::parse_block()
{
    if(current_token->s != "{")
    {
        throw syntax_error(fmt::format("{}: Expected '{{', got '{}'.", to_string(current_token->location), current_token->s));
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
        else if(current_token->type == token_type::identifier)
        {
            stmts_exprs.emplace_back(parse_expression());

            if(current_token->s != ";")
            {
                throw syntax_error(fmt::format("{}: Expected ';', got '{}'.", to_string(current_token->location), current_token->s));
            }
            get_next_token();
        }
        else
        {
            throw syntax_error(fmt::format("{}: Expected <expression> or <statement>, got '{}'.", to_string(current_token->location), current_token->s));
        }
    }
    get_next_token(false);    // skip "}". (We might hit the end of the input.)

    return std::make_unique<ast::block>(std::move(stmts_exprs));
}

// primary_expr ::= identifier_expr | literal_expr | paren_expr
std::unique_ptr<ast::expression> parser::parse_primary()
{
    if(current_token->type == token_type::identifier)
    {
        return parse_identifier_expression();
    }
    else if(current_token->type == token_type::int_literal
            || current_token->type == token_type::fp_literal
            || current_token->type == token_type::str_literal)
    {
        return parse_literal_expression();
    }
    else if(current_token->s == "(")
    {
        return parse_paren_expression();
    }
    else
    {
        throw syntax_error(fmt::format("{}: Expected <primary-expression>, got '{}'.", to_string(current_token->location), current_token->s));
    }
}

/** Binary operator precedences. */
static std::unordered_map<std::string, int> bin_op_precedence = {
  // clang-format off
  {"::", 12},
  {".", 11}, {"->", 11},
  {"*", 10}, {"/", 10}, {"%", 10},
  {"+", 9}, {"-", 9},
  {"<<", 8}, {">>", 8},
  {"<", 7}, {"<=", 7}, {">", 7}, {">=", 7},
  {"==", 6}, {"!=", 6},
  {"&", 5},
  {"^", 4},
  {"|", 3},
  {"&&", 2},
  {"||", 1},
  {"=", 0}, {"+=", 0}, {"-=", 0}, {"*=", 0}, {"/=", 0}, {"%=", 0},
  {"<<=", 0}, {">>=", 0}, {"&=", 0}, {"^=", 0}, {"|=", 0},
  // clang-format on
};

int parser::get_token_precedence()
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

// binoprhs ::= ('+' primary)*
std::unique_ptr<ast::expression> parser::parse_bin_op_rhs(int prec, std::unique_ptr<ast::expression> lhs)
{
    while(true)
    {
        int tok_prec = get_token_precedence();
        if(tok_prec < prec)
        {
            return lhs;
        }

        lexical_token bin_op = *current_token;
        get_next_token();

        std::unique_ptr<ast::expression> rhs = parse_expression();

        int next_prec = get_token_precedence();
        if(tok_prec < next_prec)
        {
            rhs = parse_bin_op_rhs(tok_prec + 1, std::move(rhs));
        }

        lhs = std::make_unique<ast::binary_expression>(std::move(bin_op.s), std::move(lhs), std::move(rhs));
    }
}

// identifierexpr ::= identifier
//                  | identifier '(' expression* ')'
//                  | identifier '::' identifierexpr
//                  | identifier '.' identifierexpr
//                  | identifier '{' primary* '}'
//                  | identifier '{' (identifier: primary_expr)* '}'
std::unique_ptr<ast::expression> parser::parse_identifier_expression()
{
    std::string identifier = current_token->s;
    get_next_token();    // skip identifier

    if(current_token->s == "(")    // function call.
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
                    throw syntax_error(fmt::format("{}: Expected ')' or ','.", to_string(current_token->location)));
                }
                get_next_token();
            }
        }
        get_next_token();    // skip ")"

        return std::make_unique<ast::call_expression>(std::move(identifier), std::move(args));
    }
    else if(current_token->s == "::")    // namespace access
    {
        get_next_token();    // skip "::"

        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(fmt::format("{}: Expected <identifier>.", to_string(current_token->location)));
        }

        return std::make_unique<ast::scope_expression>(std::move(identifier), std::move(parse_identifier_expression()));
    }
    else if(current_token->s == ".")    // element access
    {
        get_next_token();    // skip "."

        if(current_token->type != token_type::identifier)
        {
            throw syntax_error(fmt::format("{}: Expected <identifier>.", to_string(current_token->location)));
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
                            throw syntax_error(fmt::format("{}: Unexpected ':' in anonymous struct initialization.", to_string(current_token->location)));
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
                    throw syntax_error(fmt::format("{}: Expected '}}' or ','.", to_string(current_token->location)));
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

    // variable reference
    return std::make_unique<ast::variable_reference_expression>(std::move(identifier));
}

// literal_expression ::= int_literal | fp_literal | string_literal
std::unique_ptr<ast::literal_expression> parser::parse_literal_expression()
{
    lexical_token token = *current_token;
    get_next_token();

    if(token.value == std::nullopt)
    {
        throw parser_error(fmt::format("{}: Expected <literal>, got '{}'.", to_string(token.location), token.s));
    }

    if(token.type == token_type::int_literal)
    {
        return std::make_unique<ast::literal_expression>(std::get<int>(*token.value));
    }
    else if(token.type == token_type::fp_literal)
    {
        return std::make_unique<ast::literal_expression>(std::get<float>(*token.value));
    }
    else if(token.type == token_type::str_literal)
    {
        return std::make_unique<ast::literal_expression>(std::get<std::string>(*token.value));
    }
    else
    {
        throw parser_error(fmt::format("{}: Unknown literal type.", to_string(token.location)));
    }
}

std::unique_ptr<ast::expression> parser::parse_paren_expression()
{
    get_next_token();    // skip '('
    auto expr = parse_expression();

    if(current_token->s != ")")
    {
        throw parser_error(fmt::format("{}: Expected ')'.", to_string(current_token->location)));
    }
    get_next_token();    // skip ')'

    return expr;
}

// expression ::= primary binoprhs
//              | '+' primary binoprhs
//              | '-' primary binoprhs
std::unique_ptr<ast::expression> parser::parse_expression()
{
    std::unique_ptr<ast::expression> lhs;
    if(current_token->s == "+" || current_token->s == "-")
    {
        std::string sign = current_token->s;
        get_next_token();

        lhs = std::make_unique<ast::signed_expression>(std::move(sign), parse_primary());
    }
    else
    {
        lhs = parse_primary();
    }

    return parse_bin_op_rhs(0, std::move(lhs));
}

// ifexpr ::= '(' expression ')' block 'else' (ifexpr | block)
std::unique_ptr<ast::if_statement> parser::parse_if()
{
    get_next_token();    // skip 'if'.
    if(current_token->s != "(")
    {
        throw syntax_error(fmt::format("{}: Expected '(', got '{}'.", to_string(current_token->location), current_token->s));
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

    return std::make_unique<ast::if_statement>(std::move(condition), std::move(if_block), std::move(else_block));
}

std::unique_ptr<ast::expression> parser::parse_while()
{
    get_next_token();    // skip 'while'.
    if(current_token->s != "(")
    {
        throw syntax_error(fmt::format("{}: Expected '(', got '{}'.", to_string(current_token->location), current_token->s));
    }
    std::unique_ptr<ast::expression> condition = parse_expression();    // '(' expression ')'
    std::unique_ptr<ast::expression> while_block = parse_block();

    return std::make_unique<ast::while_statement>(std::move(condition), std::move(while_block));
}

std::unique_ptr<ast::expression> parser::parse_break()
{
    get_next_token();    // skip 'break'.
    if(current_token->s != ";")
    {
        throw syntax_error(fmt::format("{}: Expected ';', got '{}'.", to_string(current_token->location), current_token->s));
    }
    return std::make_unique<ast::break_statement>();
}

std::unique_ptr<ast::expression> parser::parse_continue()
{
    get_next_token();    // skip 'continue'.
    if(current_token->s != ";")
    {
        throw syntax_error(fmt::format("{}: Expected ';', got '{}'.", to_string(current_token->location), current_token->s));
    }
    return std::make_unique<ast::continue_statement>();
}

std::unique_ptr<ast::return_statement> parser::parse_return()
{
    get_next_token();    // skip 'return'.

    std::unique_ptr<ast::expression> expr{nullptr};
    if(current_token->s != ";")
    {
        expr = parse_expression();
    }

    if(current_token->s != ";")
    {
        throw syntax_error(fmt::format("{}: Expected ';', got '{}'.", to_string(current_token->location), current_token->s));
    }

    return std::make_unique<ast::return_statement>(std::move(expr));
}

void parser::parse(lexer& in_lexer)
{
    m_lexer = &in_lexer;

    std::vector<std::unique_ptr<ast::expression>> exprs;
    while((current_token = in_lexer.next()) != std::nullopt)
    {
        exprs.emplace_back(std::move(parse_top_level_statement()));
    }

    if(!in_lexer.eof())
    {
        throw parser_error("Not all tokens parsed.");
    }
    else
    {
        ast = std::make_unique<ast::block>(std::move(exprs));
    }
}

}    // namespace slang