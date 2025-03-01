/**
 * slang - a simple scripting language.
 *
 * the parser. generates an AST from the lexer output.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ast.h"
#include "lexer.h"

namespace slang
{

/**
 * An error during parsing.
 */
class parser_error : public std::runtime_error
{
public:
    /**
     * Construct a parser_error.
     *
     * @note Use the other constructor if you want to include token location information in the error message.
     *
     * @param message The error message.
     */
    explicit parser_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a syntax_error.
     *
     * @param tok The token where the error occured.
     * @param message The error message.
     */
    parser_error(const token& tok, const std::string& message);
};

/**
 * A syntax error.
 */
class syntax_error : public std::runtime_error
{
public:
    /**
     * Construct a syntax_error.
     *
     * @note Use the other constructor if you want to include token location information in the error message.
     *
     * @param message The error message.
     */
    explicit syntax_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a syntax_error.
     *
     * @param tok The token where the error occured.
     * @param message The error message.
     */
    syntax_error(const token& tok, const std::string& message);
};

/** Operator associativity. */
enum class associativity
{
    left_to_right,
    right_to_left
};

/**
 * This class implements a recursive descent LL(1) parser.
 * It uses a lexer to turn tokens into an abstract syntax tree.
 */
class parser
{
protected:
    /** The lexer common to all internal parse calls. */
    lexer* current_lexer{nullptr};

    /** The parsed AST. */
    std::shared_ptr<ast::expression> ast;

    /** Token buffer. */
    std::optional<token> current_token{std::nullopt};

    /** Whether we are parsing a native function declaration. Enables untyped array parsing. */
    bool parsing_native{false};

    /** Directive stack with entries `(name, restore_function)`. */
    std::vector<std::pair<token, std::function<void(void)>>> directive_stack;

    /**
     * Get the next token and store it in the token buffer `current_token`.
     *
     * @param throw_on_eof Whether to throw a `syntax_error` if the next token indicates the end of the file.
     * @return Returns the read token.
     * @throws A `parser_error` if no lexer is available.
     *         A `syntax_error` if `throw_on_eof` is set and no token is available.
     */
    std::optional<token> get_next_token(bool throw_on_eof = true)
    {
        if(!current_lexer)
        {
            throw parser_error("No lexer set for parsing.");
        }

        current_token = current_lexer->next();
        if(!current_token.has_value() && throw_on_eof)
        {
            throw syntax_error("Unexpected end of file.");
        }

        return current_token;
    }

    /**
     * Get the precedence value for the current token.
     *
     * @return The precedence value for the current token. Returns -1 if the current token is not a binary operator.
     */
    int get_token_precedence() const;

    /**
     * Get the associativity for the current token.
     *
     * @return The associativity for the current token, or std::nullopt.
     */
    std::optional<associativity> get_token_associativity() const;

    /** Parse a top level statement. */
    std::unique_ptr<ast::expression> parse_top_level_statement();

    /** Parse an import statement. */
    std::unique_ptr<ast::import_expression> parse_import();

    /** Parse a function prototype. */
    std::unique_ptr<ast::prototype_ast> parse_prototype();

    /** Parse a function definition. */
    std::unique_ptr<ast::function_expression> parse_definition();

    /** Parse a variable declaration. */
    std::unique_ptr<ast::variable_declaration_expression> parse_variable();

    /** Parse a constant expression. */
    std::unique_ptr<ast::constant_declaration_expression> parse_const();

    /**
     * Parse a type.
     *
     * @return Returns a type expression.
     * */
    std::unique_ptr<ast::type_expression> parse_type();

    /** Parse an array initializer expression. */
    std::unique_ptr<ast::array_initializer_expression> parse_array_initializer_expression();

    /** Parse a struct definition. */
    std::unique_ptr<ast::struct_definition_expression> parse_struct();

    /** Parse a compiler directive. */
    std::pair<token, std::vector<std::pair<token, token>>> get_directive();

    /**
     * Parse any block of expressions between { and }.
     *
     * Function definitions don't skip the closing brace, since that is done
     * by the parser loop.
     *
     * @param skip_closing_brace Whether to skip the block's closing brace.
     */
    std::unique_ptr<ast::block> parse_block(bool skip_closing_brace = true);

    /**
     * Parse a statement or expression in a block. Returns an empty `std::unique_ptr`
     * if the statement/expression was empty.
     */
    std::unique_ptr<ast::expression> parse_block_stmt_expr();

    /** Parse a primary expression. */
    std::unique_ptr<ast::expression> parse_primary();

    /** Parse the right-hand side of a binary operator expression. */
    std::unique_ptr<ast::expression> parse_bin_op_rhs(int prec, std::unique_ptr<ast::expression> lhs);

    /** Parse a unary operator expression. */
    std::unique_ptr<ast::expression> parse_unary(bool ignore_type_cast = false);

    /** Parse the new operator. */
    std::unique_ptr<ast::expression> parse_new();

    /** Parse identifier expression. */
    std::unique_ptr<ast::expression> parse_identifier_expression();

    /** Parse a member access expression. */
    std::unique_ptr<ast::expression> parse_access_expression(std::unique_ptr<ast::expression> lhs);

    /** Parse literal expression. */
    std::unique_ptr<ast::literal_expression> parse_literal_expression();

    /** Parse parenthesized expression. */
    std::unique_ptr<ast::expression> parse_paren_expression();

    /** Parse expression. */
    std::unique_ptr<ast::expression> parse_expression();

    /** Parse a type cast expression. */
    std::unique_ptr<ast::expression> parse_type_cast_expression(std::unique_ptr<ast::expression> expr);

    /** Parse an if statement. */
    std::unique_ptr<ast::if_statement> parse_if();

    /** Parse an while statement. */
    std::unique_ptr<ast::expression> parse_while();

    /** Parse an break statement. */
    std::unique_ptr<ast::expression> parse_break();

    /** Parse an continue statement. */
    std::unique_ptr<ast::expression> parse_continue();

    /** Parse an return statement. */
    std::unique_ptr<ast::return_statement> parse_return();

    /** Parse a macro. */
    std::unique_ptr<ast::macro_expression> parse_macro();

    /** Push a directive onto the directive stack. */
    void push_directive(const token& name, const std::vector<std::pair<token, token>>& args);

    /** Pop last directive from directive stack. */
    void pop_directive();

public:
    /** Default constructor. */
    parser() = default;

    /** Copy and move constructors. */
    parser(const parser&) = delete;
    parser(parser&&) = default;

    /** Destructor. */
    ~parser() = default;

    /** Assignment. */
    parser& operator=(const parser&) = delete;
    parser& operator=(parser&&) = default;

    /**
     * Parse tokens from a lexer into an abstract syntax tree.
     *
     * @param lexer The lexer to use.
     */
    void parse(lexer& lexer);

    /** Get the AST. */
    std::shared_ptr<ast::expression> get_ast() const
    {
        return ast;
    }
};

}    // namespace slang
