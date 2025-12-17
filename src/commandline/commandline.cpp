/**
 * slang - a simple scripting language.
 *
 * commands to be executed from the command line.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "commandline.h"
#include "strings.h"

namespace slang::commandline
{

/*
 * global command line.
 */

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<std::string> global_command_line;

void set_command_line(const std::vector<std::string>& cmdline)
{
    global_command_line = cmdline;
}

const std::vector<std::string>& get_command_line()
{
    return global_command_line;
}

/*
 * command implementation.
 */

void command::validate_name() const
{
    for(const auto& c: name)
    {
        if(c != '_' && !is_alpha(c))
        {
            throw std::runtime_error(std::format("Invalid command name '{}'.", name));
        }
    }
}

}    // namespace slang::commandline
