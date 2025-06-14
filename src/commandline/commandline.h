/**
 * slang - a simple scripting language.
 *
 * commands to be executed from the command line.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include <cxxopts.hpp>

#include "package.h"

/*
 * Forward declarations.
 */

namespace slang::interpreter
{
class context;
}    // namespace slang::interpreter

namespace slang::commandline
{

/**
 * Set the command line.
 *
 * @param cmdline The command line.
 */
void set_command_line(const std::vector<std::string>& cmdline);

/**
 * Get the command line.
 *
 * @returns Returns a reference to the command line vector.
 */
const std::vector<std::string>& get_command_line();

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

    /**
     * Create a `cxxopts::Options` object using command line information,
     * command name and command description.
     *
     * @returns A `cxxopts::Options` object.
     */
    cxxopts::Options make_cxxopts_options() const
    {
        const std::string program_name = std::format("{} {}", get_command_line()[0], get_name());
        return cxxopts::Options(program_name, get_description());
    }

    /**
     * Parse the command line.
     *
     * @param options A `cxxopts::Options` object.
     * @return The parse result.
     */
    static cxxopts::ParseResult parse_args(cxxopts::Options& options, const std::vector<std::string>& args)
    {
        std::vector<const char*> argv =
          args
          | std::views::transform(
            [](const auto& arg)
            { return arg.data(); })
          | std::ranges::to<std::vector>();

        argv.insert(argv.begin(), "<dummy-argument>");

        return options.parse(argv.size(), argv.data());
    };

public:
    /**
     * Construct a command.
     *
     * @param name The command's name, used for invocation. A valid name
     *             consists only of letters and underscores.
     *
     * @throws Throws a std::runtime_error if the name is invalid.
     */
    explicit command(const std::string& name)
    : name{name}
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

/** Module execution. */
class run : public command
{
    /** The package manager bound to this command. */
    slang::package_manager& manager;

public:
    /** Constructor. */
    explicit run(slang::package_manager& manager);
    void invoke(const std::vector<std::string>& args) override;
    std::string get_description() const override;
};

/** Bytecode disassembly. */
class disasm : public command
{
    /** The package manager bound to this command. */
    slang::package_manager& manager;

public:
    /** Constructor. */
    explicit disasm(slang::package_manager& manager);
    void invoke(const std::vector<std::string>& args) override;
    std::string get_description() const override;
};

/**
 * Set up the default runtime environment for a context.
 *
 * @param ctx The context to set up.
 * @param verbose Whether to enable verbose logging. Defaults to `false`.
 */
void runtime_setup(slang::interpreter::context& ctx, bool verbose = false);

}    // namespace slang::commandline
