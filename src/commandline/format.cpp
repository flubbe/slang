/**
 * slang - a simple scripting language.
 *
 * `fmt` command implementation.
 */

#include <fstream>
#include <format>
#include <print>

#include "formatter/formatter.h"
#include "commandline.h"

namespace slang::commandline
{

namespace
{

static std::string read_file(const fs::path& path)
{
    std::ifstream in{path, std::ios::in | std::ios::binary};
    if(!in)
    {
        throw std::runtime_error(std::format("Unable to open '{}' for reading.", path.string()));
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
    const auto files = result["files"].as<std::vector<std::string>>();

    formatter::source_formatter formatter{
      formatter::options{
        .indent_size = 4,
        .max_line_length = line_length,
        .validate_syntax = true,
        .ensure_trailing_newline = true}};

    std::size_t changed_count = 0;
    for(const auto& file_arg: files)
    {
        const fs::path file{file_arg};
        const std::string original = read_file(file);
        const std::string formatted = formatter.format_text(original);

        if(original == formatted)
        {
            continue;
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
