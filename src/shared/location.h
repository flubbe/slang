/**
 * slang - a simple scripting language.
 *
 * source location definition.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <string>

#include "archives/archive.h"

namespace slang
{

/** A location in the source. */
struct source_location
{
    std::size_t line{1}; /** Line. */
    std::size_t col{1};  /** Column. */

    /** Default constructors. */
    source_location() = default;
    source_location(const source_location&) = default;
    source_location(source_location&&) = default;

    /** Default destructor. */
    ~source_location() = default;

    /** Assignment. */
    source_location& operator=(const source_location&) = default;
    source_location& operator=(source_location&&) = default;

    /**
     * Initialize line and col.
     *
     * @param l The line.
     * @param c The column.
     */
    source_location(std::size_t l, std::size_t c)
    : line{l}
    , col{c}
    {
    }

    /**
     * Comparison.
     *
     * @param other The source_location to compare with.
     */
    bool operator==(const source_location& other) const
    {
        return line == other.line && col == other.col;
    }
};

/**
 * `source_location` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param loc The token location to serialize.
 */
archive& operator&(archive& ar, source_location& loc);

/**
 * Convert a token location to a string.
 *
 * @param loc The token location.
 * @return A string of the form "(line, col)".
 */
std::string to_string(const source_location& loc);

}    // namespace slang
