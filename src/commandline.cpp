/**
 * slang - a simple scripting language.
 *
 * commands to be executed from the command line.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <array>

#include <fmt/core.h>

#include "commandline.h"
#include "compiler.h"
#include "utils.h"

namespace slang::commandline
{

/**
 * Protected package names. Can only be removed
 * when --protected is specified on the command line.
 */
const std::array<std::string, 1> protected_names = {
  "std"};

/*
 * command implementation.
 */

void command::validate_name() const
{
    for(const auto& c: name)
    {
        if(c != '_' && !std::isalpha(c))
        {
            throw std::runtime_error(fmt::format("Invalid command name '{}'.", name));
        }
    }
}

/*
 * pkg implementation (package management).
 */

pkg::pkg(slang::package_manager& in_manager)
: command{"pkg"}
, manager{in_manager}
{
}

void pkg::invoke(const std::vector<std::string>& args)
{
    if(args.size() == 0)
    {
        const std::vector<std::pair<std::string, std::string>> cmd_help =
          {
            {"create pkg_name", "Create a new package."},
            {"info", "Output information about the current environment"},
            {"list [--all]", "Print a list of installed packages. If --all is specified, also lists sub-packages."},
            {"remove [--protected] pkg_name", "Remove a package."}};

        slang::utils::print_command_help("Usage: slang pkg command [args...]", cmd_help);
        return;
    }
    else
    {
        if(args[0] == "create")
        {
            if(args.size() != 2)
            {
                const std::string help_text =
                  "pkg_name consists of package identifiers, separated by ::. For example, std::utils "
                  "is a valid package name.";
                slang::utils::print_usage_help("slang pkg create pkg_name", help_text);
                return;
            }

            const std::string package_name = args[1];
            fmt::print("Creating package '{}'...\n", package_name);

            if(!package::is_valid_name(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot create package '{}': The name is not a valid package name.", package_name));
            }

            if(manager.exists(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot create package '{}', since it already exists.", package_name));
            }

            slang::package pkg = manager.open(package_name, true);
            if(!pkg.is_persistent())
            {
                pkg.make_persistent();
            }

            fmt::print("Package '{}' successfully created.\n", package_name);
        }
        else if(args[0] == "info")
        {
            fmt::print("Package environment:\n\n");
            fmt::print("    Package root:            {}\n", manager.get_root_path().string());
            fmt::print("    Persistent package root: {}\n", manager.is_persistent());
            fmt::print("\n");
        }
        else if(args[0] == "list")
        {
            bool include_sub_packages = false;

            if(args.size() >= 2)
            {
                if(args.size() > 2 || args[1] != "--all")
                {
                    slang::utils::print_usage_help(
                      "slang pkg list [--all]",
                      "Print a list of installed packages. If --all is specified, also lists sub-packages.");
                    return;
                }
                include_sub_packages = true;
            }

            std::vector<std::string> package_names = manager.get_package_names(include_sub_packages);

            if(package_names.size() > 0)
            {
                fmt::print("Packages:\n\n");
                for(const auto& name: package_names)
                {
                    fmt::print("    {}\n", name);
                }
                fmt::print("\n");
            }
            else
            {
                fmt::print("No packages found.\n");
            }
        }
        else if(args[0] == "remove")
        {
            auto print_help = []()
            {
                const std::string help_text =
                  "pkg_name consists of package identifiers, separated by ::. For example, std::utils "
                  "is a valid package name.";
                slang::utils::print_usage_help("slang pkg remove [--protected] pkg_name", help_text);
            };

            if(args.size() != 2 && args.size() != 3)
            {
                print_help();
                return;
            }

            std::string package_name;
            bool remove_protected = false;
            if(args.size() == 3)
            {
                if(args[1] != "--protected")
                {
                    print_help();
                    return;
                }
                remove_protected = true;
                package_name = args[2];
            }
            else
            {
                package_name = args[1];
            }

            // check if we're about to remove a protected package
            if(std::find(protected_names.begin(), protected_names.end(), package_name) != protected_names.end() && !remove_protected)
            {
                fmt::print("Cannot remove protected package '{}'. If you really want to remove it, specify --protected on the command line.\n", package_name);
                return;
            }

            fmt::print("Removing package '{}'...\n", package_name);

            if(!package::is_valid_name(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot remove package '{}': The name is not a valid package name.", package_name));
            }

            if(!manager.exists(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot remove package '{}', since it does not exist.", package_name));
            }

            manager.remove(package_name);

            if(!manager.exists(package_name))
            {
                fmt::print("Package '{}' successfully removed.\n", package_name);
            }
            else
            {
                throw std::runtime_error(fmt::format("Could not remove package '{}'.", package_name));
            }
        }
        else
        {
            throw std::runtime_error(fmt::format("Unknown argument '{}'.", args[0]));
        }
    }
}

std::string pkg::get_description() const
{
    return "Manage packages.";
}

/*
 * Package builds.
 */

build::build(slang::package_manager& in_manager)
: command{"build"}
, manager{in_manager}
{
}

void build::invoke(const std::vector<std::string>& args)
{
    if(args.size() != 1)
    {
        const std::string help_text =
          "pkg_name consists of package identifiers, separated by ::. For example, std::utils "
          "is a valid package name.\n\nWhen a package is built, all of its sub-packages are also built.";
        slang::utils::print_usage_help("slang build pkg_name", help_text);
        return;
    }

    const std::string package_name = args[0];
    if(!package::is_valid_name(package_name))
    {
        throw std::runtime_error(fmt::format("Cannot build package '{}': The name is not a valid package name.", package_name));
    }

    if(!manager.exists(package_name))
    {
        throw std::runtime_error(fmt::format("Cannot build package '{}', since it does not exist.", package_name));
    }

    fmt::print("Building package '{}'...\n", package_name);

    // get all module sources.
    std::vector<package> pkgs{manager.open(package_name, false)};
    std::vector<std::string> sub_package_names = manager.get_package_names(true, package_name);

    fmt::print("Collected:\n");
    fmt::print("    * {}\n", package_name);
    for(auto& n: sub_package_names)
    {
        fmt::print("    * {}\n", n);
        pkgs.push_back(manager.open(n, false));
    }

    slang::compiler compiler{manager};

    // first pass.
    compiler.setup(pkgs);
    compiler.invoke_lexer();
    compiler.collect_definitions();
    pkgs = compiler.resolve_dependencies();

    // second pass.
    try
    {
        for(auto& p: pkgs)
        {
            compiler.compile_package(p);
        }
    }
    catch(const slang::compiler_error& e)
    {
        fmt::print("{}\n", e.what());
        fmt::print("Compilation failed.\n");
        return;
    }
}

std::string build::get_description() const
{
    return "Build a package.";
}

/*
 * Package execution.
 */

exec::exec(slang::package_manager& in_manager)
: command{"exec"}
, manager{in_manager}
{
}

void exec::invoke(const std::vector<std::string>& args)
{
    // TODO implement.
    throw std::runtime_error("exec command not implemented.");
}

std::string exec::get_description() const
{
    return "Execute a package.";
}

}    // namespace slang::commandline