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

namespace slang::utils
{

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
