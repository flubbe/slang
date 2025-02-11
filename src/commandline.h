/**
 * slang - a simple scripting language.
 *
 * commands to be executed from the command line.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "package.h"

namespace slang::commandline
{

/** Command-line argument parser. */
class argument_parser
{
    /** A mapping of command line options. */
    std::unordered_map<std::string, std::optional<std::string>> options;

    /** Arguments that are neither options nor flags, stored by position. */
    std::vector<std::pair<std::size_t, std::string>> positional;

    /**
     * Parse the arguments.
     *
     * @param args Argument vector.
     */
    void parse(const std::vector<std::string>& args)
    {
        for(std::size_t i = 0; i < args.size(); ++i)
        {
            const auto& arg = args[i];
            if(arg.compare(0, 2, "--") == 0)
            {
                if(arg.length() == 2)
                {
                    // argument separator. Ignore all following arguments.
                    return;
                }

                if(i != args.size() - 1 && args[i + 1][0] != '-')
                {
                    // NOTE overwrites previous arguments.
                    options.insert({arg.substr(2), args[i + 1]});
                }
                else
                {
                    options.insert({arg.substr(2), std::nullopt});
                }
            }
            else
            {
                positional.emplace_back(i, arg);
            }
        }
    }

public:
    /**
     * Construct an argument parser.
     *
     * @param args Argument vector.
     */
    argument_parser(const std::vector<std::string>& args)
    {
        parse(args);
    }

    /** Check if a flag is set. */
    bool has_flag(const std::string& flag) const
    {
        auto it = options.find(flag);
        return it != options.end() && (*it).second == std::nullopt;
    }

    /**
     * Get a command line option.
     *
     * @param option The option to get.
     * @param default_value A default value if the option was not found.
     * @returns The option's value or `default_value`.
     */
    std::optional<std::string> get_option(
      const std::string& option,
      std::optional<std::string> default_value = std::nullopt)
    {
        auto it = options.find(option);
        if(it == options.end())
        {
            return default_value;
        }
        return it->second;
    }

    /** Get the positional arguments. */
    const std::vector<std::pair<std::size_t, std::string>> get_positional() const
    {
        return positional;
    }
};

/** A generic commandline command. */
class command
{
protected:
    /** Command name, as specified on the command line. */
    std::string name;

    /**
     * Check the command's name for validity. A valid name
     * consists only of letters and underscores.
     *
     * @throws Throws a std::runtime_error if the name is invalid.
     */
    void validate_name() const;

public:
    /**
     * Construct a command.
     *
     * @param name The command's name, used for invokation. A valid name
     *             consists only of letters and underscores.
     *
     * @throws Throws a std::runtime_error if the name is invalid.
     */
    explicit command(const std::string& in_name)
    : name{in_name}
    {
        validate_name();
    }

    /** Destructor. */
    virtual ~command() = default;

    /** Disallow default construction. */
    command() = delete;

    /** Default copy constructor. */
    command(const command&) = default;

    /** Move constructor. */
    command(command&& other)
    : name{std::move(other.name)}
    {
    }

    /** Copy assignment. */
    command& operator=(const command&) = default;

    /** Move assignment. */
    command& operator=(command&& other)
    {
        name = std::move(other.name);
        return *this;
    }

    /**
     * Invoke the command.
     *
     * @param args Arguments for the command.
     * @throw Throws std::runtime_error.
     */
    virtual void invoke(const std::vector<std::string>& args) = 0;

    /** Get the command's description. */
    virtual std::string get_description() const = 0;

    /** Return the command's name. */
    const std::string& get_name() const
    {
        return name;
    }
};

/** Package management. */
class pkg : public command
{
    /** The package manager bound to this command. */
    slang::package_manager& manager;

public:
    /** Constructor. */
    explicit pkg(slang::package_manager& manager);
    void invoke(const std::vector<std::string>& args) override;
    std::string get_description() const override;
};

/** Compile single module. */
class compile : public command
{
    /** The package manager bound to this command. */
    slang::package_manager& manager;

public:
    /** Constructor. */
    explicit compile(slang::package_manager& manager);
    void invoke(const std::vector<std::string>& args) override;
    std::string get_description() const override;
};

/** Package execution. */
class exec : public command
{
    /** The package manager bound to this command. */
    slang::package_manager& manager;

public:
    /** Constructor. */
    explicit exec(slang::package_manager& manager);
    void invoke(const std::vector<std::string>& args) override;
    std::string get_description() const override;
};

}    // namespace slang::commandline