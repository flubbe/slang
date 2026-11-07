/**
 * slang - a simple scripting language.
 *
 * The lexer.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <array>
#include <algorithm>
#include <format>
#include <utility>

#include "lexer.h"
#include "utils.h"

namespace
{

using namespace slang;

/*
 * Helper functions.
 */

/**
 * Horizontal whitespace (without line breaks): space, tab, form feed / new page
 *
 * @param c The `std::optional<char>` to check.
 * @return `true` for horizontal whitespaces and false otherwise (including `std::nullopt`'s).
 */
bool is_horizontal_whitespace(
  const std::optional<char>& c)
{
    return c.has_value()
           && (c.value() == ' '        // space
               || c.value() == '\t'    // tab
               || c.value() == '\f'    // form feed / new page
           );
}

/**
 * Vertical whitespace: carriage return, vertical tab, line feed / new line
 *
 * @param c The `std::optional<char>` to check.
 * @return `true` for vertical whitespaces and false otherwise (including `std::nullopt`'s).
 */
bool is_vertical_whitespace(
  const std::optional<char>& c)
{
    return c.has_value()
           && (c.value() == '\n'       // line feed / new line
               || c.value() == '\r'    // carriage return
               || c.value() == '\v'    // vertical tab
           );
}

/**
 * Check if an `std::optional<char>` is a whitespace character. whitespace
 * characters are: space, tab, line feed / new line, carriage return,
 * form feed / new page, and vertical tab.
 *
 * @param c The `std::optional<char>` to check.
 * @return `true` for whitespaces and false otherwise (including `std::nullopt`'s).
 */
bool is_whitespace(
  const std::optional<char>& c)
{
    return is_horizontal_whitespace(c)
           || is_vertical_whitespace(c);
}

/**
 * Check if a character is valid in an identifier, i.e., if
 * it is a-z, A-Z or _, or 0-9 when not the first character.
 *
 * @param c The std::optional<char> to check.
 * @param first_char Whether we are checking the first character.
 * @return Whether the character starts an identifier. Returns false if c was std::nullopt.
 */
bool is_identifier(
  const std::optional<char>& c,
  bool first_char)
{
    if(!c.has_value())
    {
        return false;
    }

    if(first_char)
    {
        return utils::is_alpha(c.value())
               || c.value() == '_';
    }
    return utils::is_alnum(c.value())
           || c.value() == '_';
}

/**
 * Check if a character is a hex digit, i.e., 0-9, a-f or A-F.
 *
 * @param c The std::optional<char> to check.
 * @return Whether the character is a hex digit. Returns false if c was std::nullopt
 */
bool is_hexdigit(
  const std::optional<char>& c)
{
    if(!c.has_value())
    {
        return false;
    }

    return std::isdigit(c.value()) != 0
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
constexpr std::array<std::string_view, operator_count> operators = {
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
constexpr std::array<char, operator_chars_count> operator_chars = {
  // clang-format off
  '+', '-', '*', '/', '%', '&', '^', '|', '!', '~', '<', '>', '=', ':',    // NOLINT(readability-trailing-comma)
  // clang-format on
};

/**
 * Check whether a character starts an operator.
 *
 * @param c The `std::optional<char>` to check.
 * @return Whether the character starts an operator. Returns `false` if `c` was `std::nullopt`.
 */
bool is_operator(
  const std::optional<char>& c)
{
    return c.has_value()
           && std::ranges::contains(
             std::as_const(operator_chars),
             c.value());
}

/**
 * Evaluate a string, given its token type. If type is not one of `token_type::int_literal`,
 * `token_type::fp_literal` or `token_type::str_literal`, `std::nullopt` is returned.
 *
 * @param loc Source location for error reporting.
 * @param s The string to evaluate.
 * @param type The string's token type.
 * @return Returns the evaluated token or std::nullopt.
 * @throws Throws a `lexical_error` if evaluation failed.
 */
std::optional<const_value> eval(
  source_location loc,
  const std::string& s,
  token_type type)
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
                return std::stol(s, nullptr, BASE);
            }
            return std::stoll(s);
        }
        catch(const std::invalid_argument&)
        {
            throw lexical_error{
              std::format(
                "{}: Invalid argument for integer conversion: '{}'.",
                to_string(loc),
                s)};
        }
        catch(const std::out_of_range& e)
        {
            throw lexical_error{
              std::format(
                "{}: Argument out of range for integer conversion: '{}'.",
                to_string(loc),
                s)};
        }
    }
    else if(type == token_type::fp_literal)
    {
        try
        {
            return std::stod(s);
        }
        catch(const std::invalid_argument& e)
        {
            throw lexical_error{
              std::format(
                "{}: Invalid argument for floating point conversion: '{}'.",
                to_string(loc),
                s)};
        }
        catch(const std::out_of_range& e)
        {
            throw lexical_error{
              std::format(
                "{}: Argument out of range for floating point conversion: '{}'.",
                to_string(loc),
                s)};
        }
    }

    return std::nullopt;
}

/**
 * Validate suffix: Make sure the suffix is one of: i8, i16, i32, i64, f32, f64.
 *
 * @param loc Source location for error reporting.
 * @param suffix The suffix to validate.
 */
void validate_suffix(
  source_location loc,
  const std::optional<numeric_suffix>& suffix)
{
    if(!suffix.has_value())
    {
        return;
    }

    const auto& s = suffix.value();
    if(s.ty == suffix_type::integer
       && (s.width != 8         // NOLINT(readability-magic-numbers)
           && s.width != 16     // NOLINT(readability-magic-numbers)
           && s.width != 32     // NOLINT(readability-magic-numbers)
           && s.width != 64)    // NOLINT(readability-magic-numbers)
    )
    {
        throw lexical_error{
          std::format(
            "{}: Invalid width {} for integer type suffix. Valid widths are 8, 16, 32, 64.",
            slang::to_string(loc),
            s.width)};
    }

    if(s.ty == suffix_type::floating_point
       && (s.width != 32         // NOLINT(readability-magic-numbers)
           && s.width != 64))    // NOLINT(readability-magic-numbers)
    {
        throw lexical_error{
          std::format(
            "{}: Invalid width {} for floating point type suffix. Valid widths are 32, 64.",
            slang::to_string(loc),
            s.width)};
    }
}

}    // namespace

namespace slang
{

/*
 * Lexer implementation.
 */

std::optional<token> lexer::next()
{
    std::string current_token;
    std::string eval_token;    // token without suffix, for evaluation.
    token_type type = token_type::unknown;
    std::optional<numeric_suffix> suffix;
    source_location loc;
    std::vector<comment_trivia> leading_comments;
    std::vector<comment_trivia> trailing_comments;
    bool has_blank_line_before_token = false;

    auto parse_line_comment = [this]
    {
        comment_trivia trivia;
        trivia.is_block = false;

        trivia.text += *get();    // '/'
        trivia.text += *get();    // '/'

        while(auto c = peek())
        {
            if(c.value() == '\n')
            {
                break;
            }
            trivia.text += *get();    // consume comment body
        }

        return trivia;
    };

    auto parse_block_comment = [this]
    {
        comment_trivia trivia;
        trivia.is_block = true;

        trivia.text += *get();    // '/'
        trivia.text += *get();    // '*'

        while(auto c = get())
        {
            trivia.text += c.value();
            if(c.value() == '*'
               && peek().has_value()
               && *peek() == '/')
            {
                trivia.text += *get();    // '/'
                return trivia;
            }
        }

        throw lexical_error{
          std::format(
            "{}: Missing terminating '*/' for block comment.",
            to_string(get_location()))};
    };

    // collect whitespace/comments preceding the next token.
    while(!eof())
    {
        std::size_t newline_count = 0;
        while(is_whitespace(peek()))
        {
            if(is_vertical_whitespace(get()))
            {
                ++newline_count;
            }
        }

        if(newline_count > 1)
        {
            has_blank_line_before_token = true;
        }

        if(peek().has_value()
           && *peek() == '/')
        {
            auto p2 = position;
            ++p2;
            if(p2 != input.end()
               && *p2 == '/')
            {
                auto trivia = parse_line_comment();
                trivia.has_blank_line_before = newline_count > 1;
                leading_comments.emplace_back(
                  std::move(trivia));
                continue;
            }
            if(p2 != input.end()
               && *p2 == '*')
            {
                auto trivia = parse_block_comment();
                trivia.has_blank_line_before = newline_count > 1;
                leading_comments.emplace_back(
                  std::move(trivia));
                continue;
            }
        }

        break;
    }

    if(eof())
    {
        return std::nullopt;
    }

    // get the next token.
    type = token_type::unknown;    // reset type on each iteration.
    current_token.clear();         // clear token on each iteration.
    eval_token.clear();            // clear evaluation token on each iteration.
    suffix = std::nullopt;         // clear numeric suffix on each iteration.

    loc = get_location();

    std::optional<char> c = get();
    if(!c.has_value())
    {
        return std::nullopt;
    }

    current_token = {c.value()};
    while(true)
    {
        bool macro_identifier = false;
        if(c.value() == '$')    // macro identifiers.
        {
            c = get();
            if(!is_identifier(c, true))
            {
                throw lexical_error{
                  std::format(
                    "{}: Expected identifier, got '{}'.",
                    to_string(loc),
                    c.value())};
            }

            current_token += c.value();
            macro_identifier = true;
        }

        if(is_identifier(c, true)
           || macro_identifier)
        {
            while(is_identifier(peek(), false))
            {
                current_token += *get();
            }

            if(macro_identifier)
            {
                type = token_type::macro_identifier;
            }
            else if(peek() == '!')
            {
                current_token += *get();
                type = token_type::macro_name;
            }
            else
            {
                type = token_type::identifier;
            }

            break;
        }

        if(is_operator(c))
        {
            // match the longest operator.
            while((c = peek()).has_value())
            {
                std::string temp_token = current_token + c.value();
                if(!std::ranges::contains(std::as_const(operators), temp_token))
                {
                    break;
                }

                current_token += c.value();
                get();    // advance to next character
            }

            break;
        }

        if(c.value() == '(' || c.value() == ')'
           || c.value() == '[' || c.value() == ']'
           || c.value() == '{' || c.value() == '}')    // parentheses, brackets and braces
        {
            break;
        }

        if(std::isdigit(c.value()) != 0
           || (c.value() == '.'
               && peek().has_value()
               && std::isdigit(*peek()) != 0))    //  integer or floating-point literal
        {
            if(c.value() == '0'
               && peek().has_value()
               && *peek() == 'x')
            {
                current_token += *get();

                while((c = peek()).has_value()
                      && (std::isdigit(c.value()) != 0
                          || is_hexdigit(c)))
                {
                    current_token += *get();
                }

                if(c = peek();
                   c.has_value()
                   && utils::is_alpha(c.value()))
                {
                    throw lexical_error{
                      std::format(
                        "{}: Expected non-alphabetic character, got '{}'.",
                        to_string(loc),
                        c.value())};
                }

                type = token_type::int_literal;
                eval_token = current_token;

                break;
            }

            if(c.value() != '.')
            {
                while((c = peek()).has_value()
                      && std::isdigit(c.value()) != 0)
                {
                    current_token += *get();
                }

                type = token_type::int_literal;    // this might get adjusted below.
                c = peek();
            }

            if(c.has_value()
               && c.value() == '.')
            {
                current_token += *get();

                while((c = peek()).has_value()
                      && std::isdigit(c.value()) != 0)
                {
                    current_token += *get();
                }

                type = token_type::fp_literal;
                c = peek();
            }

            if(c.has_value()
               && (c.value() == 'e' || c.value() == 'E'))
            {
                current_token += *get();

                if(c = peek();
                   c.has_value()
                   && (c.value() == '+' || c.value() == '-'))
                {
                    current_token += *get();
                }

                while(peek().has_value()
                      && std::isdigit(*peek()) != 0)
                {
                    current_token += *get();
                }

                type = token_type::fp_literal;
            }

            eval_token = current_token;

            if(c = peek();
               c.has_value()
               && utils::is_alpha(c.value()))
            {
                suffix_type type = [this, loc, &current_token]
                {
                    auto type_char = *get();
                    current_token += type_char;

                    if(type_char == 'i')
                    {
                        return suffix_type::integer;
                    }

                    if(type_char == 'f')
                    {
                        return suffix_type::floating_point;
                    }

                    throw lexical_error{
                      std::format(
                        "{}: Invalid numeric suffix: Unknown type identifier.",
                        to_string(loc))};
                }();

                if(c = peek();
                   !c.has_value()
                   || std::isdigit(c.value()) == 0)
                {
                    throw lexical_error{
                      std::format(
                        "{}: Invalid numeric suffix: Expected width specifier.",
                        to_string(loc))};
                }

                std::string width_specifier;
                while(peek().has_value()
                      && (std::isdigit(*peek()) != 0))
                {
                    width_specifier += *get();
                }
                current_token += width_specifier;

                suffix = numeric_suffix{
                  .ty = type,
                  .width = static_cast<std::uint32_t>(stoi(width_specifier)),
                };
            }

            break;
        }

        if(c.value() == '.')    // element access or ellipsis. Needs to come after parsing floating-point literals.
        {
            if(c = peek();
               c.has_value()
               && c.value() == '.')
            {
                // ellipsis.
                current_token += *get();

                if(c = peek();
                   c.has_value()
                   && c.value() != '.')
                {
                    throw lexical_error{
                      std::format(
                        "{}: Expected '.', got '{}'.",
                        to_string(loc),
                        c.value())};
                }

                current_token += *get();
            }

            break;
        }

        if(c.value() == '"')    // string literals.
        {
            bool escaped = false;
            while((c = get()))
            {
                if(c.value() == '\\'
                   && !escaped)
                {
                    escaped = true;
                    continue;
                }

                if(!escaped)
                {
                    current_token += c.value();

                    if(c.value() == '"')
                    {
                        break;
                    }

                    if(c.value() == '\n')
                    {
                        throw lexical_error{
                          std::format(
                            "{}: Missing terminating character '\"'.",
                            to_string(loc))};
                    }
                }
                else
                {
                    // handle escape sequences.
                    escaped = false;

                    if(c.value() == 't') /* tab */
                    {
                        current_token += '\t';
                    }
                    else if(c.value() == 'n') /* line feed / new line */
                    {
                        current_token += '\n';
                    }
                    else if(c.value() == 'r') /* carriage return */
                    {
                        current_token += '\r';
                    }
                    else if(c.value() == 'f') /* form feed / new page */
                    {
                        current_token += '\f';
                    }
                    else if(c.value() == 'v') /* vertial tab */
                    {
                        current_token += '\v';
                    }
                    else if(c.value() == '"') /* double quote */
                    {
                        current_token += '"';
                    }
                    else if(c.value() == '\'') /* single quote */
                    {
                        current_token += '\'';
                    }
                    else if(c.value() == '\\') /* backslash */
                    {
                        current_token += '\\';
                    }
                    else
                    {
                        throw lexical_error{
                          std::format(
                            "{}: Unknown escape sequence '\\{}'.",
                            to_string(loc),
                            c.value())};
                    }
                }
            }

            if(c = peek();
               c.has_value()
               && utils::is_alpha(c.value()))
            {
                throw lexical_error{
                  std::format(
                    "{}: Invalid suffix '{}' on string literal.",
                    to_string(loc),
                    c.value())};
            }

            type = token_type::str_literal;
            eval_token = current_token;

            break;
        }

        if(c.value() == ',')    // comma operator / separator.
        {
            break;
        }

        if(c.value() == ';')    // statement end
        {
            break;
        }

        if(c.value() == '#')    // directives
        {
            break;
        }

        throw lexical_error{
          std::format(
            "{}: Unexpected character '{}' (0x{:x})",
            to_string(loc),
            c.value(),
            static_cast<int>(c.value()))};
    }

    if(current_token.empty())
    {
        if(eof())
        {
            return std::nullopt;
        }
        throw lexical_error{
          "lext::next: No token parsed."};
    }

    validate_suffix(loc, suffix);

    // collect trailing comments after the token, if present on the same line.
    while(true)
    {
        while(is_horizontal_whitespace(peek()))
        {
            get();
        }

        if(!peek().has_value()
           || is_vertical_whitespace(peek()))
        {
            break;
        }

        if(*peek() != '/')
        {
            break;
        }

        auto p2 = position;
        ++p2;
        if(p2 == input.end())
        {
            break;
        }

        if(*p2 == '/')
        {
            trailing_comments.emplace_back(
              parse_line_comment());
            continue;
        }

        if(*p2 == '*')
        {
            trailing_comments.emplace_back(
              parse_block_comment());
            continue;
        }

        break;
    }

    return slang::token{
      current_token,
      loc,
      type,
      suffix,
      eval(loc, eval_token, type),
      has_blank_line_before_token,
      std::move(leading_comments),
      std::move(trailing_comments)};
}

token_stream lexer::lex(
  std::string_view input)
{
    lexer lexer{std::string{input}};

    std::optional<token> current_token;
    std::vector<token> tokens;

    while((current_token = lexer.next()).has_value())
    {
        tokens.push_back(current_token.value());
    }

    return token_stream{
      std::move(tokens)};
}

}    // namespace slang
