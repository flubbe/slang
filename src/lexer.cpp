/**
 * slang - a simple scripting language.
 *
 * the lexer.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <array>

#include <fmt/core.h>

#include "lexer.h"

namespace slang
{

/*
 * Helper functions.
 */

/**
 * Check if an std::optional<char> is a whitespace character. whitespace
 * characters are: space, tab, line feed / new line, carriage return,
 * form feed / new page, and vertical tab.
 *
 * @param c The std::optional<char> to check.
 * @return true for whitespaces and false otherwise (including std::nullopt's).
 */
static bool is_whitespace(const std::optional<char>& c)
{
    return c && (*c == ' '        // space
                 || *c == '\t'    // tab
                 || *c == '\n'    // line feed / new line
                 || *c == '\r'    // carriage return
                 || *c == '\f'    // form feed / new page
                 || *c == '\v'    // vertial tab
           );
}

/**
 * Check if a character is valid in an identifier, i.e., if
 * it is a-z, A-Z or _, or 0-9 when not the first character.
 *
 * @param c The std::optional<char> to check.
 * @param first_char Whether we are checking the first character.
 * @return Whether the character starts an identifier. Returns false if c was std::nullopt.
 */
static bool is_identifier(const std::optional<char>& c, bool first_char)
{
    if(!c)
    {
        return false;
    }

    if(first_char)
    {
        return std::isalpha(*c) || *c == '_';
    }
    return std::isalnum(*c) || *c == '_';
}

/**
 * Check if a character is a hex digit, i.e., 0-9, a-f or A-F.
 *
 * @param c The std::optional<char> to check.
 * @return Whether the character is a hex digit. Returns false if c was std::nullopt
 */
static bool is_hexdigit(const std::optional<char>& c)
{
    if(!c)
    {
        return false;
    }

    return std::isdigit(*c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/** The count of operators the lexer supports. */
constexpr std::size_t operator_count = 34;

/**
 * Supported operators.
 *
 * NOTE We treat the access operator . separately, since it could also start a floating-point literal.
 */
static std::array<std::string, operator_count> operators = {
  // clang-format off
  "+", "-", "*", "/", "%",             // arithmetic / prefixes
  "&&", "||", "!",                     // logical
  "&", "^", "|", "~",                  // bitwise
  "<<", ">>",                          // shifts
  "==", "!=", "<", "<=", ">", ">=",    // comparisons
  "=", "+=", "-=", "*=", "/=", "%=",   // assignments
  "&=", "|=", "<<=", ">>=",
  "++", "--",                          // increment, decrement
  "::",                                // namespace access
  "->"                                 // return type annotation
  // clang-format on
};

/** The count of different starting characters of the operators. */
constexpr std::size_t operator_chars_count = 14;

/** The starting characters of the operators. */
static std::array<char, operator_chars_count> operator_chars = {
  // clang-format off
  '+', '-', '*', '/', '%', '&', '^', '|', '!', '~', '<', '>', '=', ':'
  // clang-format on
};

/**
 * Check whether a character starts an operator.
 *
 * @param c The std::optional<char> to check.
 * @return Whether the character starts an operator. Returns false if c was std::nullopt.
 */
static bool is_operator(const std::optional<char>& c)
{
    return c && std::find(operator_chars.begin(), operator_chars.end(), *c) != operator_chars.end();
}

/**
 * Evaluate a string, given its token type. If type is not one of token_type::int_literal,
 * token_type::fp_literal or token_type::str_literal, std::nullopt is returned.
 *
 * @param s The string to evaluate.
 * @param type The string's token type.
 * @return Returns the evaluated token or std::nullopt.
 */
static std::optional<std::variant<int, float, std::string>> eval(const std::string& s, token_type type)
{
    if(type == token_type::str_literal)
    {
        return s.substr(1, s.length() - 2);    // remove quotes
    }
    else if(type == token_type::int_literal)
    {
        if(s.substr(0, 2) == "0x")
        {
            return std::stoi(s, nullptr, 16);
        }
        return std::stoi(s);
    }
    else if(type == token_type::fp_literal)
    {
        return std::stof(s);
    }

    return std::nullopt;
}

/*
 * lexer implementation.
 */

std::optional<token> lexer::next()
{
    std::string current_token;
    token_type type;
    token_location loc;

    while(!eof())    // this loop is only here for catching comments
    {
        type = token_type::unknown;    // reset type on each iteration
        current_token.clear();         // clear token on each iteration.

        while(is_whitespace(peek()))
        {
            get();
        }

        loc = get_location();

        std::optional<char> c;
        if(!(c = get()))
        {
            return std::nullopt;
        }

        current_token = {*c};
        if(is_identifier(c, true))
        {
            while(is_identifier(peek(), false))
            {
                current_token += *get();
            }

            type = token_type::identifier;
            break;
        }
        else if(*c == '/' && peek() != std::nullopt && *peek() == '/')    // single-line comment
        {
            // skip single-line comment and retry.
            while((c = get()))
            {
                if(*c == '\n')
                {
                    break;
                }
            }

            // clear token here, since the outer loop condition might not be satsified anymore.
            current_token.clear();
            continue;
        }
        else if(*c == '/' && peek() != std::nullopt && *peek() == '*')    // multi-line comment
        {
            // skip multi-line comment and retry.
            get();
            while((c = get()))
            {
                if(*c == '*' && peek() != std::nullopt && peek() == '/')
                {
                    get();
                    break;
                }
            }

            // clear token here, since the outer loop condition might not be satsified anymore.
            current_token.clear();
            continue;
        }
        else if(is_operator(c))
        {
            // match the longest operator.
            while((c = peek()))
            {
                std::string temp_token = current_token + *c;
                if(std::find(operators.begin(), operators.end(), temp_token) == operators.end())
                {
                    break;
                }
                current_token += *get();
            }

            break;
        }
        else if(*c == '(' || *c == ')' || *c == '[' || *c == ']' || *c == '{' || *c == '}')    // parentheses, brackets and braces
        {
            break;
        }
        else if(std::isdigit(*c) || (*c == '.' && peek() && std::isdigit(*peek())))    //  integer or floating-point literal
        {
            if(*c == '0' && peek() && *peek() == 'x')
            {
                current_token += *get();
                while(peek() && (std::isdigit(*peek()) || is_hexdigit(peek())))
                {
                    current_token += *get();
                }

                if(peek() && std::isalpha(*peek()))
                {
                    throw lexical_error(fmt::format("{}: Expected non-alphabetic character, got '{}'.", to_string(loc), *peek()));
                }

                type = token_type::int_literal;
                break;
            }

            if(*c != '.')
            {
                while(peek() && std::isdigit(*peek()))
                {
                    current_token += *get();
                }

                type = token_type::int_literal;    // this might get adjusted below.
                c = peek();
            }

            if(*c == '.')
            {
                current_token += *get();
                while(peek() && std::isdigit(*peek()))
                {
                    current_token += *get();
                }

                type = token_type::fp_literal;
                c = peek();
            }

            if(*c == 'e' || *c == 'E')
            {
                current_token += *get();

                if(peek() && (*peek() == '+' || *peek() == '-'))
                {
                    current_token += *get();
                }

                while(peek() && std::isdigit(*peek()))
                {
                    current_token += *get();
                }

                type = token_type::fp_literal;
            }

            if(peek() && std::isalpha(*peek()))
            {
                throw lexical_error(fmt::format("{}: Invalid suffix '{}' on numeric literal.", to_string(loc), *peek()));
            }

            break;
        }
        else if(*c == '.')    // element access. Needs to come after parsing floating-point literals.
        {
            break;
        }
        else if(*c == '"')    // string literals.
        {
            while((c = get()))
            {
                current_token += *c;
                if(*c == '"')
                {
                    break;
                }
                if(*c == '\n')
                {
                    throw lexical_error(fmt::format("{}: Missing terminating character '\"'.", to_string(loc)));
                }
            }

            if(peek() && std::isalpha(*peek()))
            {
                throw lexical_error(fmt::format("{}: Invalid suffix '{}' on string literal.", to_string(loc), *peek()));
            }

            type = token_type::str_literal;
            break;
        }
        else if(*c == ',')    // comma operator / separator.
        {
            break;
        }
        else if(*c == ';')    // statement end
        {
            break;
        }
        else if(*c == '#')    // directives
        {
            break;
        }
        else
        {
            throw lexical_error(fmt::format("{}: Unexpected character '{}' (0x{:x})", to_string(loc), *c, static_cast<int>(*c)));
        }
    }

    if(current_token.length() == 0)
    {
        if(eof())
        {
            return std::nullopt;
        }
        throw lexical_error("lext::next: No token parsed.");
    }

    return slang::token{current_token, loc, type, eval(current_token, type)};
}

}    // namespace slang