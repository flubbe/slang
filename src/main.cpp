/**
 * slang - a simple scripting language.
 *
 * Program entry point.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstdlib>

#include <cxxopts.hpp>
#include <fmt/core.h>

#include "commandline/commandline.h"

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
        slang::commandline::set_command_line({argv, argv + argc});
        std::vector<std::string> args{std::next(argv), argv + argc};

        // create root package manager.
        slang::package_manager root_pm{"lang"};

        // create command list.
        command_vector cmd_list;
        add_unique_command(cmd_list, std::make_unique<slang::commandline::compile>(root_pm));
        add_unique_command(cmd_list, std::make_unique<slang::commandline::exec>(root_pm));

        cxxopts::Options options(argv[0], "slang command line interface.");    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        auto command_help = [&cmd_list]() -> std::string
        {
            std::string text = "  command   The command to execute. One of {";
            for(std::size_t i = 0; i < cmd_list.size() - 1; ++i)
            {
                text += fmt::format("{}|", cmd_list[i]->get_name());
            }
            return fmt::format("{}{}}}.", text, cmd_list.back()->get_name());
        };

        options.add_options()("command", "", cxxopts::value<std::string>());

        options.parse_positional({"command"});
        options.positional_help("command");
        auto result = options.parse(std::min(argc, 2), argv);    // only consider the first argument.

        if(result.count("command") < 1)
        {
            fmt::print("{}", options.help());
            fmt::print("Positional arguments:\n\n");
            fmt::print("{}\n\n", command_help());
            return EXIT_SUCCESS;
        }

        auto command = result["command"].as<std::string>();
        for(auto& cmd: cmd_list)
        {
            if(command == cmd->get_name())
            {
                cmd->invoke({std::next(args.begin()), args.end()});
                return EXIT_SUCCESS;
            }
        }

        fmt::print("{}", options.help());
        fmt::print("Positional arguments:\n\n");
        fmt::print("{}\n\n", command_help());

        fmt::print("Error: Command '{}' not found.\n\n", args[0]);

        return EXIT_FAILURE;
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("An error occured: {}\n", e.what());
        return EXIT_FAILURE;
    }
    catch(const cxxopts::exceptions::exception& e)
    {
        fmt::print("{}\n", e.what());
        return EXIT_FAILURE;
    }
    catch(...)
    {
        fmt::print("The program unexpectedly crashed.\n");
        return EXIT_FAILURE;
    }
}
