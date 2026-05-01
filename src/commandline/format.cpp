/**
 * slang - a simple scripting language.
 *
 * `fmt` command implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fstream>
#include <format>
#include <print>
#include <set>
#include <string_view>

#include "formatter/formatter.h"
#include "filemanager.h"
#include "commandline.h"

namespace slang::commandline
{

namespace
{

static std::string read_file(
  const fs::path& path)
{
    std::ifstream in{
      path,
      std::ios::in | std::ios::binary};
    if(!in)
    {
        throw std::runtime_error(
          std::format(
            "Unable to open '{}' for reading.",
            path.string()));
    }

    std::string content;
    in.seekg(0, std::ios::end);
    content.resize(static_cast<std::size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(content.data(), static_cast<std::streamsize>(content.size()));
    return content;
}

static void write_file(const fs::path& path, const std::string& content)
{
    std::ofstream out{path, std::ios::out | std::ios::binary | std::ios::trunc};
    if(!out)
    {
        throw std::runtime_error(std::format("Unable to open '{}' for writing.", path.string()));
    }

    out << content;
}

static bool is_supported_glob(const std::string& p)
{
    return p.find("**/*") != std::string::npos
           || p.ends_with("/**")
           || p.ends_with("\\**");
}

static std::vector<fs::path> collect_recursive_with_extension(
  const fs::path& base_path,
  const std::string& wanted_extension)
{
    if(!fs::exists(base_path) || !fs::is_directory(base_path))
    {
        throw std::runtime_error(
          std::format(
            "Glob base path '{}' does not exist or is not a directory.",
            base_path.string()));
    }

    std::vector<fs::path> out;
    for(const auto& entry: fs::recursive_directory_iterator(base_path))
    {
        if(!entry.is_regular_file())
        {
            continue;
        }

        if(entry.path().extension() == wanted_extension)
        {
            out.emplace_back(entry.path());
        }
    }

    return out;
}

static std::string with_file_context(const fs::path& file, const std::string& message)
{
    // Parser/lexer diagnostics are usually "<line>:<col>: <message>".
    // Rewrite to "<path>:<line>:<col>: <message>" for clickable editor output.
    const auto first_colon = message.find(':');
    if(first_colon != std::string::npos)
    {
        const auto second_colon = message.find(':', first_colon + 1);
        if(second_colon != std::string::npos)
        {
            const auto line_part = message.substr(0, first_colon);
            const auto col_part = message.substr(first_colon + 1, second_colon - first_colon - 1);

            const bool line_is_num = !line_part.empty() && std::ranges::all_of(line_part, ::isdigit);
            const bool col_is_num = !col_part.empty() && std::ranges::all_of(col_part, ::isdigit);
            if(line_is_num && col_is_num)
            {
                return std::format("{}:{}", file.string(), message);
            }
        }
    }

    return std::format("{}: {}", file.string(), message);
}

static std::vector<fs::path> expand_glob(const std::string& pattern)
{
    if(pattern.ends_with("/**") || pattern.ends_with("\\**"))
    {
        const auto base = pattern.substr(0, pattern.size() - 3);
        const fs::path base_path = base.empty() ? fs::path{"."} : fs::path{base};
        return collect_recursive_with_extension(base_path, ".sl");
    }

    const auto wildcard_pos = pattern.find("**/*");
    if(wildcard_pos == std::string::npos)
    {
        return {fs::path{pattern}};
    }

    const std::string base = pattern.substr(0, wildcard_pos);
    const std::string suffix = pattern.substr(wildcard_pos + std::string("**/*").size());

    std::string wanted_extension = ".sl";
    if(!suffix.empty())
    {
        if(suffix.starts_with("."))
        {
            wanted_extension = suffix;
        }
        else
        {
            throw std::runtime_error(
              std::format(
                "Unsupported glob pattern '{}'. Use '**/*' or '**/*.ext'.",
                pattern));
        }
    }

    fs::path base_path = base.empty() ? fs::path{"."} : fs::path{base};
    return collect_recursive_with_extension(base_path, wanted_extension);
}

}    // namespace

fmt::fmt()
: command{"fmt"}
{
}

void fmt::invoke(const std::vector<std::string>& args)
{
    cxxopts::Options options = make_cxxopts_options();

    // clang-format off
    options.add_options()
        ("check", "Check formatting without rewriting files.")
        ("line-length", "Maximum line length (0 disables wrapping).", cxxopts::value<std::size_t>()->default_value("0"))
        ("files", "Files to format.", cxxopts::value<std::vector<std::string>>());
    // clang-format on

    options.parse_positional({"files"});
    options.positional_help("files...");
    auto result = parse_args(options, args);

    if(result.count("files") < 1)
    {
        std::println("{}", options.help());
        return;
    }

    const bool check_only = result.count("check") > 0;
    const std::size_t line_length = result["line-length"].as<std::size_t>();
    const auto file_args = result["files"].as<std::vector<std::string>>();
    std::vector<fs::path> files;
    std::set<fs::path> seen;
    for(const auto& arg: file_args)
    {
        const bool explicit_extension_glob = arg.find("**/*.") != std::string::npos;
        std::vector<fs::path> expanded;
        if(is_supported_glob(arg))
        {
            expanded = expand_glob(arg);
        }
        else
        {
            fs::path p{arg};
            if(fs::exists(p) && fs::is_directory(p))
            {
                expanded = collect_recursive_with_extension(p, ".sl");
            }
            else
            {
                expanded = {std::move(p)};
            }
        }

        for(const auto& p: expanded)
        {
            if(fs::exists(p)
               && fs::is_regular_file(p)
               && p.extension() != ".sl"
               && !explicit_extension_glob)
            {
                continue;    // skip non-source files by default
            }

            if(seen.insert(p).second)
            {
                files.emplace_back(p);
            }
        }
    }

    if(files.empty())
    {
        throw std::runtime_error("No files matched the provided input patterns.");
    }

    file_manager file_mgr;
    file_mgr.add_search_path(".");

    formatter::source_formatter formatter{
      file_mgr,
      formatter::options{
        .indent_size = 4,
        .max_line_length = line_length,
        .validate_syntax = true,
        .ensure_trailing_newline = true}};

    std::size_t changed_count = 0;
    for(const auto& file: files)
    {
        std::string formatted;
        try
        {
            auto [result, changed] = formatter.format_file(file);
            if(!changed)
            {
                continue;
            }

            formatted = std::move(result);
        }
        catch(const std::exception& e)
        {
            throw std::runtime_error(with_file_context(file, e.what()));
        }

        ++changed_count;

        if(check_only)
        {
            std::println("Would reformat: {}", file.string());
            continue;
        }

        write_file(file, formatted);
        std::println("Reformatted: {}", file.string());
    }

    if(check_only && changed_count > 0)
    {
        throw std::runtime_error(
          std::format("Formatting required for {} file(s).", changed_count));
    }
}

std::string fmt::get_description() const
{
    return "Format source files.";
}

}    // namespace slang::commandline
