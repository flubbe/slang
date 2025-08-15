/**
 * slang - a simple scripting language.
 *
 * the lexer.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <array>
#include <algorithm>
#include <format>
#include <utility>

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
        return std::isalpha(*c) != 0 || *c == '_';
    }
    return std::isalnum(*c) != 0 || *c == '_';
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

    return std::isdigit(*c) != 0
           || (c >= 'a' && c <= 'f')
           || (c >= 'A' && c <= 'F');
}

/** The count of operators the lexer supports. */
constexpr std::size_t operator_count = 36;

/**
 * Supported operators.
 *
 * @note We treat the access operator `.` separately, since it could also start a floating-point literal.
 */
static const std::array<std::string, operator_count> operators = {
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
  "->",                                // return type annotation
  "=>",                                // macro definition.
  // clang-format on
};

/** The count of different starting characters of the operators. */
constexpr std::size_t operator_chars_count = 14;

/** The starting characters of the operators. */
static const std::array<char, operator_chars_count> operator_chars = {
  // clang-format off
  '+', '-', '*', '/', '%', '&', '^', '|', '!', '~', '<', '>', '=', ':'
  // clang-format on
};

/**
 * Check whether a character starts an operator.
 *
 * @param c The `std::optional<char>` to check.
 * @return Whether the character starts an operator. Returns `false` if `c` was `std::nullopt`.
 */
static bool is_operator(const std::optional<char>& c)
{
    return c && std::ranges::find(std::as_const(operator_chars), *c) != operator_chars.cend();
}

/**
 * Evaluate a string, given its token type. If type is not one of `token_type::int_literal`,
 * `token_type::fp_literal` or `token_type::str_literal`, `std::nullopt` is returned.
 *
 * @param s The string to evaluate.
 * @param type The string's token type.
 * @return Returns the evaluated token or std::nullopt.
 * @throws Throws a `lexical_error` if evaluation failed.
 */
static std::optional<std::variant<int, float, std::string>> eval(const std::string& s, token_type type)
{
    if(type == token_type::str_literal)
    {
        return s.substr(1, s.length() - 2);    // remove quotes
    }

    if(type == token_type::int_literal)
    {
        try
        {
            if(s.starts_with("0x"))
            {
                constexpr int BASE = 16;
                return std::stoi(s, nullptr, BASE);
            }
            return std::stoi(s);
        }
        catch(const std::invalid_argument&)
        {
            throw lexical_error(std::format("Invalid argument for integer conversion: '{}'.", s));
        }
        catch(const std::out_of_range& e)
        {
            throw lexical_error(std::format("Argument out of range for integer conversion: '{}'.", s));
        }
    }
    else if(type == token_type::fp_literal)
    {
        try
        {
            return std::stof(s);
        }
        catch(const std::invalid_argument& e)
        {
            throw lexical_error(std::format("Invalid argument for floating point conversion: '{}'.", s));
        }
        catch(const std::out_of_range& e)
        {
            throw lexical_error(std::format("Argument out of range for floating point conversion: '{}'.", s));
        }
    }

    return std::nullopt;
}

/*
 * lexer implementation.
 */

std::optional<token> lexer::next()
{
    std::string current_token;
    token_type type = token_type::unknown;
    source_location loc;

    while(!eof())    // this loop is only here for catching comments
    {
        type = token_type::unknown;    // reset type on each iteration
        current_token.clear();         // clear token on each iteration.

        while(is_whitespace(peek()))
        {
            get();
        }

        loc = get_location();

        std::optional<char> c = get();
        if(!c.has_value())
        {
            return std::nullopt;
        }

        current_token = {*c};

        bool macro_identifier = false;
        if(*c == '$')    // macro identifiers.
        {
            c = get();
            current_token += *c;    // NOLINT(bugprone-unchecked-optional-access)

            if(!is_identifier(c, true))
            {
                throw lexical_error(
                  std::format(
                    "{}: Expected identifier, got '{}'.",
                    to_string(loc),
                    *c));    // NOLINT(bugprone-unchecked-optional-access)
            }

            macro_identifier = true;
        }

        if(is_identifier(c, true) || macro_identifier)
        {
            while(is_identifier(peek(), false))
            {
                // NOTE peek() ensures that get() returns a valid value.
                current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
            }

            if(macro_identifier)
            {
                type = token_type::macro_identifier;
            }
            else if(peek() == '!')
            {
                current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
                type = token_type::macro_name;
            }
            else
            {
                type = token_type::identifier;
            }

            break;
        }

        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        if(*c == '/' && peek().has_value() && *peek() == '/')    // single-line comment
        {
            // skip single-line comment and retry.
            while((c = get()))
            {
                if(*c == '\n')
                {
                    break;
                }
            }

            // clear token here, since the outer loop condition might not be satisfied anymore.
            current_token.clear();
            continue;
        }

        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        if(*c == '/' && peek().has_value() && *peek() == '*')    // multi-line comment
        {
            // skip multi-line comment and retry.
            get();
            while((c = get()))
            {
                if(*c == '*' && peek().has_value() && peek() == '/')
                {
                    get();
                    break;
                }
            }

            // clear token here, since the outer loop condition might not be satisfied anymore.
            current_token.clear();
            continue;
        }

        if(is_operator(c))
        {
            // match the longest operator.
            while((c = peek()))
            {
                std::string temp_token = current_token + *c;
                if(std::ranges::find(std::as_const(operators), temp_token) == operators.cend())
                {
                    break;
                }
                current_token += *get();
            }

            break;
        }

        if(*c == '(' || *c == ')' || *c == '[' || *c == ']' || *c == '{' || *c == '}')    // parentheses, brackets and braces
        {
            break;
        }

        if(std::isdigit(*c) != 0
           // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
           || (*c == '.' && peek().has_value() && std::isdigit(*peek()) != 0))    //  integer or floating-point literal
        {
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            if(*c == '0' && peek().has_value() && *peek() == 'x')
            {
                current_token += *get();                                                            // NOLINT(bugprone-unchecked-optional-access)
                while(peek().has_value() && (std::isdigit(*peek()) != 0 || is_hexdigit(peek())))    // NOLINT(bugprone-unchecked-optional-access)
                {
                    current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
                }

                if(peek().has_value() && std::isalpha(*peek()) != 0)    // NOLINT(bugprone-unchecked-optional-access)
                {
                    throw lexical_error(
                      std::format(
                        "{}: Expected non-alphabetic character, got '{}'.",
                        to_string(loc),
                        *peek()));    // NOLINT(bugprone-unchecked-optional-access)
                }

                type = token_type::int_literal;
                break;
            }

            if(*c != '.')
            {
                while(peek().has_value() && std::isdigit(*peek()) != 0)    // NOLINT(bugprone-unchecked-optional-access)
                {
                    current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
                }

                type = token_type::int_literal;    // this might get adjusted below.
                c = peek();
            }

            if(c.has_value() && *c == '.')
            {
                current_token += *get();                                   // NOLINT(bugprone-unchecked-optional-access)
                while(peek().has_value() && std::isdigit(*peek()) != 0)    // NOLINT(bugprone-unchecked-optional-access)
                {
                    current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
                }

                type = token_type::fp_literal;
                c = peek();
            }

            if(c.has_value() && (*c == 'e' || *c == 'E'))
            {
                current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)

                if(peek().has_value() && (*peek() == '+' || *peek() == '-'))    // NOLINT(bugprone-unchecked-optional-access)
                {
                    current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
                }

                while(peek().has_value() && std::isdigit(*peek()) != 0)    // NOLINT(bugprone-unchecked-optional-access)
                {
                    current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
                }

                type = token_type::fp_literal;
            }

            if(peek().has_value() && std::isalpha(*peek()) != 0)    // NOLINT(bugprone-unchecked-optional-access)
            {
                throw lexical_error(
                  std::format(
                    "{}: Invalid suffix '{}' on numeric literal.",
                    to_string(loc),
                    *peek()));    // NOLINT(bugprone-unchecked-optional-access)
            }

            break;
        }

        if(*c == '.')    // element access or ellipsis. Needs to come after parsing floating-point literals.
        {
            if(peek().has_value() && *peek() == '.')    // NOLINT(bugprone-unchecked-optional-access)
            {
                // ellipsis.
                current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)

                if(peek().has_value() && *peek() != '.')    // NOLINT(bugprone-unchecked-optional-access)
                {
                    throw lexical_error(
                      std::format(
                        "{}: Expected '.', got '{}'.",
                        to_string(loc),
                        *peek()));    // NOLINT(bugprone-unchecked-optional-access)
                }

                current_token += *get();    // NOLINT(bugprone-unchecked-optional-access)
            }

            break;
        }

        if(*c == '"')    // string literals.
        {
            bool escaped = false;
            while((c = get()))
            {
                if(*c == '\\' && !escaped)
                {
                    escaped = true;
                    continue;
                }

                if(!escaped)
                {
                    current_token += *c;

                    if(*c == '"')
                    {
                        break;
                    }
                    if(*c == '\n')
                    {
                        throw lexical_error(std::format("{}: Missing terminating character '\"'.", to_string(loc)));
                    }
                }
                else
                {
                    // handle escape sequences.
                    escaped = false;

                    if(*c == 't') /* tab */
                    {
                        current_token += '\t';
                    }
                    else if(*c == 'n') /* line feed / new line */
                    {
                        current_token += '\n';
                    }
                    else if(*c == 'r') /* carriage return */
                    {
                        current_token += '\r';
                    }
                    else if(*c == 'f') /* form feed / new page */
                    {
                        current_token += '\f';
                    }
                    else if(*c == 'v') /* vertial tab */
                    {
                        current_token += '\v';
                    }
                    else if(*c == '"') /* double quote */
                    {
                        current_token += '"';
                    }
                    else if(*c == '\'') /* single quote */
                    {
                        current_token += '\'';
                    }
                    else if(*c == '\\') /* backslash */
                    {
                        current_token += '\\';
                    }
                    else
                    {
                        throw lexical_error(std::format("{}: Unknown escape sequence '\\{}'.", to_string(loc), *c));
                    }
                }
            }

            if(peek().has_value() && std::isalpha(*peek()) != 0)    // NOLINT(bugprone-unchecked-optional-access)
            {
                throw lexical_error(
                  std::format(
                    "{}: Invalid suffix '{}' on string literal.",
                    to_string(loc),
                    *peek()));    // NOLINT(bugprone-unchecked-optional-access)
            }

            type = token_type::str_literal;
            break;
        }

        if(*c == ',')    // comma operator / separator.
        {
            break;
        }

        if(*c == ';')    // statement end
        {
            break;
        }

        if(*c == '#')    // directives
        {
            break;
        }

        throw lexical_error(
          std::format(
            "{}: Unexpected character '{}' (0x{:x})",
            to_string(loc),
            *c,
            static_cast<int>(*c)));
    }

    if(current_token.empty())
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
