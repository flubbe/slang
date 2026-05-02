/**
 * slang - a simple scripting language.
 *
 * source formatter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <filesystem>
#include <string>

/*
 * Forward declarations.
 */
namespace slang
{
class file_manager; /* file_manager.h */
}    // namespace slang

namespace slang::formatter
{

/** Formatting options. */
struct options
{
    /** Indentation size. */
    std::size_t indent_size{4};

    /** Maximum line length. */
    std::size_t max_line_length{0};

    /** Whether to validate the syntax. */
    bool validate_syntax{true};

    /** Whether to ensure every file has a trailing newline. */
    bool ensure_trailing_newline{true};
};

/** The source formatter. */
class source_formatter
{
    /** File manager. */
    file_manager& file_mgr;

    /** Formatting options. */
    options opts;

public:
    /**
     * Construct a source formatter from options.
     *
     * @param file_mgr The file manager to use.
     * @param opts The options to use.
     */
    source_formatter(
      file_manager& file_mgr,
      options opts = {});

    /**
     * Format source text.
     *
     * @param source The input source text.
     * @return A pair `(formatted, was_formatted)`.
     */
    [[nodiscard]]
    std::pair<std::string, bool> format_text(
      const std::string& source) const;

    /**
     * Format source file.
     *
     * @param file The input source file path.
     * @return A pair `(formatted, was_formatted)`.
     */
    [[nodiscard]]
    std::pair<std::string, bool> format_file(
      const std::filesystem::path& file) const;

    /**
     * Format a file in place.
     *
     * @return true if the file contents changed.
     */
    bool format_file_in_place(
      const std::filesystem::path& file) const;
};

}    // namespace slang::formatter
