/**
 * slang - a simple scripting language.
 *
 * the parser. generates an AST from the lexer output.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <exception>

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
     * @param message The error message.
     */
    parser_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
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
     * @param message The error message.
     */
    syntax_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * The parser class. Uses a lexer to turn tokens into an abstract syntax tree.
 */
class parser
{
protected:
    /** The lexer common to all internal parse calls. */
    lexer* m_lexer{nullptr};

    /** The parsed AST. */
    std::unique_ptr<ast::block> ast;

    /** Token buffer. */
    std::optional<lexical_token> current_token{std::nullopt};

    /**
     * Get the next token and store it in the token buffer `current_token`.
     *
     * @param throw_on_eof Whether to throw a `syntax_error` if the next token indicates the end of the file.
     * @return Returns the read token.
     * @throws A `parser_error` if no lexer is available.
     *         A `syntax_error` if `throw_on_eof` is set and no token is available.
     */
    std::optional<lexical_token> get_next_token(bool throw_on_eof = true)
    {
        if(!m_lexer)
        {
            throw parser_error("No lexer set for parsing.");
        }

        current_token = m_lexer->next();
        if(current_token == std::nullopt && throw_on_eof)
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
    int get_token_precedence();

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

    /** Parse a struct definition. */
    std::unique_ptr<ast::struct_definition_expression> parse_struct();

    /** Parse any block of expressions between { and }. */
    std::unique_ptr<ast::block> parse_block();

    /** Parse a primary expression. */
    std::unique_ptr<ast::expression> parse_primary();

    /** Parse the right-hand side of an expression. */
    std::unique_ptr<ast::expression> parse_bin_op_rhs(int prec, std::unique_ptr<ast::expression> lhs);

    /** Parse identifier expression. */
    std::unique_ptr<ast::expression> parse_identifier_expression();

    /** Parse literal expression. */
    std::unique_ptr<ast::literal_expression> parse_literal_expression();

    /** Parse parenthesized expression. */
    std::unique_ptr<ast::expression> parse_paren_expression();

    /** Parse expression. */
    std::unique_ptr<ast::expression> parse_expression();

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
     * @param in_lexer The lexer to use.
     */
    void parse(lexer& in_lexer);

    /**
     * Get the AST.
     */
    const ast::block* get_ast() const
    {
        return ast.get();
    }
};

}    // namespace slang