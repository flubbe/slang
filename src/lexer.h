/**
 * slang - a simple scripting language.
 *
 * the lexer.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <exception>
#include <optional>
#include <string>
#include <variant>

#include <fmt/core.h>

#include "token.h"

namespace slang
{

/**
 * An error during lexing.
 */
class lexical_error : public std::runtime_error
{
public:
    /**
     * Construct a lexical_error.
     *
     * @param message The error message.
     */
    lexical_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * The lexer. Generates tokens from an input string.
 */
class lexer
{
    /** Lexer input. */
    std::string input;

    /** Current position in the input string. */
    std::string::iterator position;

    /** Current location (i.e., line and column). */
    token_location location;

    /** Tab size. */
    std::size_t tab_size{default_tab_size};

protected:
    /**
     * Look ahead one character.
     *
     * @return The next character, or std::nullopt if the end of the stream was reached.
     */
    std::optional<char> peek() const
    {
        if(position >= input.end())
        {
            return std::nullopt;
        }

        return *position;
    }

    /**
     * Get a character and advance the current position. Updates the location info.
     *
     * @return The character at the current position, or std::nullopt if the end of the stream was reached.
     */
    std::optional<char> get()
    {
        if(position >= input.end())
        {
            return std::nullopt;
        }

        std::optional<char> c = *(position++);
        if(!c)
        {
            return std::nullopt;
        }

        if(c == '\t')
        {
            location.col += tab_size;
        }
        else if(c == '\n')
        {
            ++location.line;
            location.col = 1;
        }
        else if(c == '\r')
        {
            location.col = 1;
        }
        else if(c == '\v')
        {
            ++location.line;
        }
        else
        {
            ++location.col;
        }

        return c;
    }

public:
    /** Default tab size (4). */
    inline static constexpr std::size_t default_tab_size{4};

    /** Construct a new lexer instance. */
    lexer()
    : position{input.begin()}
    {
    }

    /**
     * Construct a new lexer instance with an input string.
     *
     * @param str The input string.
     */
    lexer(const std::string& str)
    : input{str}
    , position{input.begin()}
    {
    }

    /** Copy and move constructors. */
    lexer(const lexer&) = default;
    lexer(lexer&&) = default;

    /** Destructor. */
    ~lexer() = default;

    /** Assignment. */
    lexer& operator=(const lexer&) = default;
    lexer& operator=(lexer&&) = default;

    /**
     * Set the input string.
     */
    void set_input(const std::string& str)
    {
        input = str;
        position = input.begin();
    }

    /**
     * Check if we are at the input string's end.
     *
     * @return Whether we are at the input string's end.
     */
    bool eof() const
    {
        return position >= input.end();
    }

    /**
     * Get the next tokens.
     *
     * @return The token produced by the lexer, or std::nullopt if the end of the stream was reached.
     * @throws lexical_error on errors, including eof.
     */
    std::optional<token> next();

    /**
     * Set the horizonal tab size.
     *
     * @param s How many spaces a tab is interpreted as. Must be at least 1.
     * @throws Throws std::runtime_error if the size was zero.
     */
    void set_tab_size(std::size_t s)
    {
        if(s == 0)
        {
            throw std::runtime_error("Invalid tab size.");
        }
        tab_size = s;
    }

    /**
     * Get the horizontal tab size.
     *
     * @return The current horizontal tab size.
     */
    std::size_t get_tab_size() const
    {
        return tab_size;
    }

    /**
     * Get the current location.
     *
     * @return The current location.
     */
    token_location get_location() const
    {
        return location;
    }

    /**
     * Return the input.
     *
     * @return The lexer's input string.
     */
    const std::string& get_input() const
    {
        return input;
    }
};

}    // namespace slang