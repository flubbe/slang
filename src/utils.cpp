/**
 * slang - a simple scripting language.
 *
 * utility functions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <list>
#include <string>
#include <vector>

#include <fmt/core.h>

#ifndef __linux__
#    ifdef __APPLE__
#        include <cstddef>
#        include <unistd.h>
#        include <sys/ioctl.h>
#        define IOCTL_AVAILABLE true
#    else
#        define IOCTL_AVAILABLE false
#    endif
#else
#    include <cstddef>
#    include <unistd.h>
#    include <sys/ioctl.h>
#    define IOCTL_AVAILABLE true
#endif

/** Set the default terminal width to 80, in case we cannot query it. */
constexpr std::size_t default_terminal_width = 80;

namespace slang::utils
{

std::size_t get_terminal_width()
{
#if IOCTL_AVAILABLE
    winsize ws;
    if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0
       && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0
       && ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != 0)
    {
        return default_terminal_width;
    }
    return static_cast<std::size_t>(ws.ws_col);
#else
    return default_terminal_width;
#endif
}

std::list<std::string> split(const std::string& s, const std::string& delimiter)
{
    if(s.length() == 0)
    {
        return {};
    }

    // split s at <delimiter> occurences.
    std::list<std::string> components;
    std::size_t current, last = 0;
    while((current = s.find(delimiter, last)) != std::string::npos)
    {
        components.push_back(s.substr(last, current - last));
        last = current + delimiter.length();
    }
    if(last != s.length())
    {
        components.push_back(s.substr(last));
    }

    return components;
}

std::string join(const std::vector<std::string>& v, const std::string& separator)
{
    if(v.size() == 0)
    {
        return {};
    }
    else if(v.size() == 1)
    {
        return v[0];
    }

    std::string res = v[0];
    for(auto it = std::next(v.begin()); it != v.end(); ++it)
    {
        res.append(separator);
        res.append(*it);
    }
    return res;
}

std::list<std::string> wrap_text(const std::string& s, std::size_t line_len)
{
    // break text into paragraphs.
    std::list<std::string> paragraphs = slang::utils::split(s, "\n");
    std::list<std::string> lines;

    for(auto& par: paragraphs)
    {
        if(par.length() == 0)
        {
            lines.push_back({});
            continue;
        }

        size_t curpos = 0;
        size_t nextpos = 0;
        std::string substr = par.substr(curpos, line_len + 1);

        while(substr.length() == line_len + 1 && (nextpos = substr.rfind(' ')) != s.npos)
        {
            lines.push_back(par.substr(curpos, nextpos));
            curpos += nextpos + 1;
            substr = par.substr(curpos, line_len + 1);
        }

        if(curpos != par.length())
        {
            lines.push_back(par.substr(curpos, par.npos));
        }
    }

    return lines;
}

void print_command_help(const std::string& info_text, const std::vector<std::pair<std::string, std::string>>& cmd_help)
{
    std::size_t indent = 0;
    for(auto& c: cmd_help)
    {
        indent = std::max(indent, c.first.length());
    }
    indent = std::max(7ul, indent);    // max(strlen("Command"), indent)
    indent += 3;                       // account for separation between first and second column

    fmt::print("{}\n", info_text);
    fmt::print("\n");

    fmt::print("    {:<{}}   Description\n", "Command", indent - 3, "Description");
    fmt::print("    {:-<{}}{:<{}}   -----------\n", "", 7, "", indent - 10);
    for(auto& [cmd, desc]: cmd_help)
    {
        // use a width of at least 40 characters for descriptions.
        int desc_len = std::max(40ul, slang::utils::get_terminal_width() - indent);
        std::list<std::string> desc_lines = slang::utils::wrap_text(desc, desc_len);

        fmt::print("    {:<{}}", cmd, indent);
        if(desc_lines.size() != 0)
        {
            fmt::print("{}\n", desc_lines.front());
            for(auto it = std::next(desc_lines.begin()); it != desc_lines.end(); ++it)
            {
                fmt::print("    {:<{}}{}\n", "", indent, *it);
            }
        }
        else
        {
            fmt::print("\n");
        }
    }
    fmt::print("\n");
}

constexpr std::size_t usage_help_indent = 4;

void print_usage_help(const std::string& usage_text, const std::string& help_text)
{
    fmt::print("Usage: {}\n", usage_text);

    std::list<std::string> lines = slang::utils::wrap_text(help_text, slang::utils::get_terminal_width() - usage_help_indent);
    if(lines.size() > 0)
    {
        fmt::print("\n");
        for(const auto& line: lines)
        {
            fmt::print("{:<{}}{}\n", "", usage_help_indent, line);
        }
        fmt::print("\n");
    }
}

}    // namespace slang::utils
