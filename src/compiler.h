/**
 * slang - a simple scripting language.
 *
 * utility functions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <stdexcept>

#include "package.h"

namespace slang
{

/**
 * A compiler error.
 */
class compiler_error : public std::runtime_error
{
public:
    /**
     * Create a compiler_error instance.
     *
     * @param message The error message.
     */
    explicit compiler_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * An error during dependency resolution.
 */
class dependency_error : public compiler_error
{
public:
    /**
     * Create a dependency_error instance.
     *
     * @param message The error message.
     */
    explicit dependency_error(const std::string& message)
    : compiler_error{message}
    {
    }
};

/**
 * The package compiler.
 */
class compiler
{
protected:
    /** The package manager. */
    package_manager& manager;

    /*+ The packages we are compiling. */
    std::vector<package> packages;

public:
    /**
     * Construct a compiler instance.
     *
     * @param in_manager The package manager to use.
     */
    explicit compiler(package_manager& in_manager) noexcept
    : manager{in_manager}
    {
    }

    /**
     * Clear the internal state.
     */
    void clear();

    /**
     * Set the compiler up for compiling packages.
     *
     * @param pkgs The packages to compile.
     */
    void setup(const std::vector<package>& pkgs);

    /**
     * Invoke the lexer. This should be done after calling setup.
     *
     * @throws lexer_error
     */
    void invoke_lexer();

    /**
     * Collect definitions from the packages and store them internally.
     *
     * This needs to be done after invoking the lexer and before calling resolve_dependencies.
     *
     * @param pkgs The packages to collect definitions from.
     */
    void collect_definitions();

    /**
     * Resolve the dependencies for the given packages. Creates a list
     * where each package only depends on the previous ones. Stores
     * external dependencies internally.
     *
     * @param pkgs The packages to resolve.
     * @return A list of packages, where each package only depends on the previous ones.
     * @throws Throws a dependency_error, if the dependencies could not be resolved.
     */
    std::vector<package> resolve_dependencies();

    /**
     * Invoke the compiler on a package. This does the
     * compilation and writes the corresponding bytecode file.
     * The package needs to be created with the package manager
     * the compiler was contructed with.
     *
     * @param pkg The package to compile.
     * @throws Throws a compiler_error on failure.
     */
    void compile_package(package& pkg);
};

}    // namespace slang