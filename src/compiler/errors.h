/**
 * slang - a simple scripting language.
 *
 * Compiler error types.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include "shared/location.h"

namespace slang
{

/**
 * Base for compiler front-end errors that carry a source location.
 *
 * The location and offending text are stored on the exception so that a
 * recovery point (e.g. the command line) can turn the error into a rich
 * diagnostic without string-parsing the message.
 */
struct source_error : public std::runtime_error
{
    /** Source location where the error occurred. */
    source_location location{};

    /** Offending text (e.g. the offending token), used to size the snippet span. */
    std::optional<std::string> text{};

    /**
     * The human-readable message without any location prefix.
     *
     * `what()` carries a `"<line>:<col>: <message>"` prefix for contexts that
     * print the exception directly; `message` holds the bare message so the
     * diagnostics can reformat it.
     */
    std::string message{};

    /**
     * An optional "did you mean" suggestion, e.g. the closest matching
     * identifier when the offending name is a typo. Recovered by the
     * diagnostic reporting site and emitted as a help sub-diagnostic.
     */
    std::optional<std::string> did_you_mean{};

    /** Construct from a message only; the location defaults to the file start. */
    explicit source_error(
      const std::string& m)
    : std::runtime_error{m}
    , message{m}
    {
    }

    /**
     * Construct with a location and message.
     *
     * @param loc The location the error refers to.
     * @param m The human-readable message (without a location prefix).
     * @param text Offending text, used to size the snippet span (optional).
     */
    source_error(
      source_location loc,
      const std::string& m,
      std::optional<std::string> text = std::nullopt)
    : std::runtime_error{
        std::format(
          "{}: {}",
          to_string(loc),
          m)}
    , location{loc}
    , text{std::move(text)}
    , message{m}
    {
    }
};

/**
 * Internal compiler error for conditions that should never occur.
 *
 * These represent bugs in the compiler itself, not user errors.
 * Used for invariant violations and edge cases that are unreachable
 * under normal operation.
 */
struct internal_error : public std::runtime_error
{
    /** Source location where the error occurred, if available. */
    std::optional<source_location> location{};

    /**
     * The human-readable message without any location prefix.
     *
     * `what()` carries a `"<line>:<col>: <message>"` prefix when a location is
     * present (for contexts that print the exception directly); `message` holds
     * the bare message so the diagnostics can reformat it without parsing.
     */
    std::string message{};

    /** Construct from a message only; no location. */
    internal_error(
      const std::string& m)
    : std::runtime_error{m}
    , message{m}
    {
    }

    /**
     * Construct with a location and message.
     *
     * @param loc The location the error refers to.
     * @param m The human-readable message (without a location prefix).
     */
    internal_error(
      source_location loc,
      const std::string& m)
    : std::runtime_error{
        std::format(
          "{}: {}",
          to_string(loc),
          m)}
    , location{loc}
    , message{m}
    {
    }
};

}    // namespace slang
