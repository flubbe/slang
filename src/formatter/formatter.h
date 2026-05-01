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

namespace slang::formatter
{

struct options
{
    std::size_t indent_size{4};
    std::size_t max_line_length{0};
    bool validate_syntax{true};
    bool ensure_trailing_newline{true};
};

class source_formatter
{
    options opts;

public:
    source_formatter() = default;
    explicit source_formatter(options opts);

    [[nodiscard]] std::string format_text(const std::string& source) const;
    [[nodiscard]] std::string format_file(const std::filesystem::path& file) const;

    /**
     * Format a file in place.
     *
     * @return true if the file contents changed.
     */
    bool format_file_in_place(const std::filesystem::path& file) const;
};

}    // namespace slang::formatter
