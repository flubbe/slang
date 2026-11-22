/**
 * slang - a simple scripting language.
 *
 * source location definition.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "archives/archive.h"

namespace slang
{

/** A location in the source. */
struct source_location
{
    std::size_t line{1}; /** Line. */
    std::size_t col{1};  /** Column. */

    /**
     * Comparison.
     *
     * @param other The source_location to compare with.
     */
    bool operator==(
      const source_location& other) const
    {
        return line == other.line && col == other.col;
    }

    /**
     * Advance the source column by an offset.
     *
     * @param offset Number of bytes to advance.
     * @returns Returns the advanced source location.
     */
    source_location advance(
      std::size_t offset) const
    {
        auto result = *this;
        result.col += offset;
        return result;
    }

    /**
     * Advance the source location by an offset, respecting line breaks.
     *
     * @param source The complete source text corresponding to this location.
     * @param offset The byte offset relative to the beginning of the source.
     * @returns The resulting source location.
     */
    source_location advance(
      std::string_view source,
      std::size_t offset) const;
};

/**
 * `source_location` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param loc The token location to serialize.
 */
archive& operator&(
  archive& ar,
  source_location& loc);

/**
 * Convert a token location to a string.
 *
 * @param loc The token location.
 * @return A string of the form "(line, col)".
 */
std::string to_string(
  const source_location& loc);

}    // namespace slang
