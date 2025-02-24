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
    if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0         // NOLINT(cppcoreguidelines-pro-type-vararg)
       && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0     // NOLINT(cppcoreguidelines-pro-type-vararg)
       && ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != 0)    // NOLINT(cppcoreguidelines-pro-type-vararg)
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
    if(s.empty())
    {
        return {};
    }

    // split s at <delimiter> occurences.
    std::list<std::string> components;
    std::size_t current = 0;
    std::size_t last = 0;
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
    if(v.empty())
    {
        return {};
    }

    if(v.size() == 1)
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

}    // namespace slang::utils
