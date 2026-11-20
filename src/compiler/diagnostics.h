/**
 * slang - a simple scripting language.
 *
 * Compiler diagnostics subsystem.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <cstdio>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "shared/location.h"
#include "shared/token.h"

namespace slang::compiler::diagnostics
{

/** Severity levels for diagnostics. */
enum class severity
{
    error,
    warning,
    note,
    help
};

/** Convert severity to string ("error", "warning", "note", "help"). */
std::string_view to_string(
  severity s);

/** A span within source code, defined by a 1-based line/col location and length in characters. */
struct source_span
{
    source_location location{1, 1};
    std::size_t length{1};

    source_span() = default;

    source_span(
      source_location loc,
      std::size_t len = 1)
    : location{loc}
    , length{len == 0 ? 1 : len}
    {
    }

    explicit source_span(
      const token& tok)
    : location{tok.location}
    , length{tok.s.empty() ? 1 : tok.s.length()}
    {
    }

    bool operator==(const source_span& other) const = default;
};

/** A secondary note, help message, or sub-diagnostic attached to a primary diagnostic. */
struct sub_diagnostic
{
    severity sev{severity::note};
    std::string message;
    std::optional<source_span> span{std::nullopt};
    std::optional<std::string> label{std::nullopt};
};

/** A complete compiler diagnostic. */
struct diagnostic
{
    severity sev{severity::error};

    /** E.g. "E0123" (optional). */
    std::optional<std::string> code;

    /** E.g. "unknown variable `foo`". */
    std::string message;

    /** Source file path/name. */
    std::optional<std::string> filename;

    /** Source span for the primary location. */
    std::optional<source_span> span;

    /** Span underline label, e.g. "unknown variable". */
    std::optional<std::string> label;

    /** Sub-diagnostics (notes, help). */
    std::vector<sub_diagnostic> children;

    diagnostic() = default;

    diagnostic(
      severity sev,
      std::string msg,
      std::optional<std::string> filename = std::nullopt,
      std::optional<source_span> span = std::nullopt)
    : sev{sev}
    , message{std::move(msg)}
    , filename{std::move(filename)}
    , span{span}
    {
    }

    diagnostic& with_code(
      std::string c)
    {
        code = std::move(c);
        return *this;
    }

    diagnostic& with_label(
      std::string l)
    {
        label = std::move(l);
        return *this;
    }

    diagnostic& with_note(
      std::string msg,
      std::optional<source_span> span = std::nullopt,
      std::optional<std::string> label = std::nullopt)
    {
        children.push_back(
          {severity::note,
           std::move(msg),
           span,
           std::move(label)});
        return *this;
    }

    diagnostic& with_help(
      std::string msg,
      std::optional<source_span> span = std::nullopt,
      std::optional<std::string> label = std::nullopt)
    {
        children.push_back(
          {severity::help,
           std::move(msg),
           span,
           std::move(label)});
        return *this;
    }
};

/** Exception thrown when unrecoverable diagnostics require stopping a compilation pass. */
struct diagnostic_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;

    diagnostic_error()
    : std::runtime_error{""}
    {
    }
};

/** Calculate the Levenshtein distance between two strings. */
std::size_t levenshtein_distance(
  std::string_view a,
  std::string_view b);

/**
 * Find the closest match to a string from a list of candidates.
 *
 * @param target The misspelled identifier.
 * @param candidates Potential candidate names.
 * @param max_distance Maximum allowed edit distance, or `std::nullopt` for the default of `max(target.size(), 3) / 3`.
 * @return The closest candidate if `distance <= max_distance`, or `std::nullopt`.
 */
std::optional<std::string> find_closest_match(
  std::string_view target,
  const std::vector<std::string>& candidates,
  std::optional<std::size_t> max_distance = std::nullopt);

/**
 * Diagnostics context. Manages source files, collects diagnostics,
 * formats them with Rust-style source snippets, and tracks error counts.
 */
class context
{
    std::unordered_map<std::string, std::string> source_files;
    std::vector<diagnostic> diagnostics;
    std::size_t error_count{0};
    std::size_t warning_count{0};

public:
    context() = default;

    /** Register file contents for snippet rendering. */
    void register_file(
      std::string filename,
      std::string content);

    /** Check if a file is registered. */
    bool has_file(
      const std::string& filename) const;

    /** Get a specific line from a registered file (1-based line number). */
    std::optional<std::string_view> get_line(
      const std::string& filename,
      std::size_t line) const;

    /** Report a diagnostic. */
    diagnostic& report(
      diagnostic diag);

    /** Convenience helper for reporting an error. */
    diagnostic& report_error(
      std::optional<std::string> filename,
      std::optional<source_span> span,
      std::string message,
      std::optional<std::string> code = std::nullopt);

    /** Convenience helper for reporting a warning. */
    diagnostic& report_warning(
      std::optional<std::string> filename,
      std::optional<source_span> span,
      std::string message,
      std::optional<std::string> code = std::nullopt);

    /** Check if any errors have been reported. */
    bool has_errors() const
    {
        return error_count > 0;
    }

    /** Check if any warnings have been reported. */
    bool has_warnings() const
    {
        return warning_count > 0;
    }

    /** Number of reported errors. */
    std::size_t get_error_count() const
    {
        return error_count;
    }

    /** Number of reported warnings. */
    std::size_t get_warning_count() const
    {
        return warning_count;
    }

    /** Access all collected diagnostics. */
    const std::vector<diagnostic>& get_diagnostics() const
    {
        return diagnostics;
    }

    /** Clear all diagnostics. */
    void clear();

    /** Format a single diagnostic to a Rust-style string. */
    std::string format(
      const diagnostic& diag) const;

    /** Format all diagnostics to a string, separated by blank lines. */
    std::string format_all() const;

    /**
     * Format a one-line summary of the collected diagnostics.
     *
     * Returns the error count when there are errors, otherwise the warning
     * count, otherwise an empty string. This is meant to be printed as the
     * final line of the diagnostic output, e.g. "error: 2 errors generated".
     */
    std::string format_summary() const;

    /**
     * Emit all diagnostics to a stream.
     *
     * @param stream The stream to write to.
     * @param with_summary Whether to append the error/warning count line,
     *         preceded by a blank line when there are diagnostics.
     */
    void emit_all(
      std::FILE* stream = stderr,
      bool with_summary = false) const;
};

}    // namespace slang::compiler::diagnostics
