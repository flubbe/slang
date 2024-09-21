/**
 * slang - a simple scripting language.
 *
 * Program entry point.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstdlib>

#include <fmt/core.h>

#include "commandline.h"
#include "package.h"
#include "utils.h"

/**
 * Print the help when calling slang.
 */
static void print_help(const std::vector<std::unique_ptr<slang::commandline::command>>& cmd_list)
{
    std::vector<std::pair<std::string, std::string>> cmd_help;
    for(auto& c: cmd_list)
    {
        cmd_help.push_back({c->get_name(), c->get_description()});
    }

    slang::utils::print_command_help("Usage: slang command [args...]", cmd_help);
}

/** The command vector type. */
using command_vector = std::vector<std::unique_ptr<slang::commandline::command>>;

/**
 * Add a unique command to a list. If a command of the same name
 * already exists, throw an error.
 *
 * @param cmds The list the command is added to.
 * @param new_cmd The new command.
 * @throws If the command is already registered, a std::runtime_error is thrown.
 */
void add_unique_command(command_vector& cmds, std::unique_ptr<slang::commandline::command> new_cmd)
{
    for(const auto& c: cmds)
    {
        if(c->get_name() == new_cmd->get_name())
        {
            throw std::runtime_error(fmt::format("add_unique_command: Command '{}' already registered.", new_cmd->get_name()));
        }
    }

    cmds.push_back(std::move(new_cmd));
}

/**
 * Program entry point.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 */
int main(int argc, char* argv[])
{
    try
    {
        std::vector<std::string> args{std::next(argv), argv + argc};

        // create root package manager.
        slang::package_manager root_pm{"lang"};

        // create command list.
        command_vector cmd_list;
        add_unique_command(cmd_list, std::make_unique<slang::commandline::build>(root_pm));
        add_unique_command(cmd_list, std::make_unique<slang::commandline::compile>(root_pm));
        add_unique_command(cmd_list, std::make_unique<slang::commandline::exec>(root_pm));
        add_unique_command(cmd_list, std::make_unique<slang::commandline::pkg>(root_pm));

        if(args.size() == 0)
        {
            print_help(cmd_list);
            return EXIT_SUCCESS;
        }

        for(auto& cmd: cmd_list)
        {
            if(args[0] == cmd->get_name())
            {
                cmd->invoke({std::next(args.begin()), args.end()});
                return EXIT_SUCCESS;
            }
        }

        print_help(cmd_list);
        fmt::print("\nCommand '{}' not found.\n\n", args[0]);
        return EXIT_FAILURE;
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("An error occured: {}\n", e.what());
        return EXIT_FAILURE;
    }
    catch(...)
    {
        fmt::print("The program unexpectedly crashed.\n");
        return EXIT_FAILURE;
    }
}
