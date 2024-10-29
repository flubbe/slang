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
#include <vector>

#include "package.h"

namespace slang::commandline
{

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

/** Package builds. */
class build : public command
{
    /** The package manager bound to this command. */
    slang::package_manager& manager;

public:
    /** Constructor. */
    explicit build(slang::package_manager& manager);
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