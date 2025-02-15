/**
 * slang - a simple scripting language.
 *
 * package management.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "utils.h"

namespace slang
{

namespace fs = std::filesystem;

/**
 * A package is a folder structure, and might contain packages or modules (i.e., compiled library files).
 *
 * The package path is always relative (e.g. to a root directory known to a package manager).
 */
class package
{
    /** The path of the package. */
    fs::path path;

public:
    /** Package name component delimiter. */
    inline static const std::string delimiter = "::";

    /** File extension for modules. */
    inline static const std::string module_ext = "cmod";

    /** File extension for source files. */
    inline static const std::string source_ext = "sl";

    /**
     * Open a package.
     *
     * @param path Path to the package. Does not need to exist.
     */
    explicit package(const fs::path& path);

    /**
     * Check if a given module is contained in this package.
     *
     * @param name The module name.
     * @return Returns true if the module name is valid and contained in this package, else false.
     */
    bool contains_module(const std::string& module_name) const;

    /**
     * Check if a source file for a given module is contained in this package.
     *
     * @param name The module name.
     * @return Returns true if the module name is valid and contained in this package, else false.
     */
    bool contains_source(const std::string& module_name) const;

    /**
     * Return whether the package exists in the fileystem.
     */
    bool is_persistent() const;

    /**
     * Create the path for the package in the filesystem if it does not exist.
     */
    void make_persistent();

    /**
     * Return "::"-separated components of a string.
     *
     * @return The components of the input string.
     */
    static std::list<std::string> split(const std::string& s);

    /**
     * Check whether the supplied package name is valid.
     *
     * A package name consists of "::"-separated components. A component starts with an underscore
     * or a letter, and then can only contain letters, numbers, and underscores.
     *
     * @param name The name to check.
     * @return Whether the name was valid.
     */
    static bool is_valid_name(const std::string& name);

    /**
     * Check if a given name is a valid name for a package name component.
     * A component starts with an underscore or a letter, and then can only contain
     * letters, numbers, and underscores.
     *
     * @param name The component to check.
     * @return Whether the provided name was a valid package name component.
     */
    static bool is_valid_name_component(const std::string& name);
};

/**
 * The package manager. Opens/closes/creates/deletes packages.
 */
class package_manager
{
    /** The root path for this package manager. The path is lazily created if not explicitly asked for. */
    fs::path package_root;

    /**
     * Return the canonical form of the path. Creates the path if it does not exist.
     *
     * @param path The path to create.
     *
     * @throws std::bad_alloc on allocation failure and fs::filesystem_error
     *         when path creation fails or the canonical path cannot be obtained.
     */
    fs::path get_path(const fs::path& path);

public:
    /**
     * Construct a new package manager.
     *
     * @param package_root The root path for this package manager.
     * @param create Whether to create the path if it does not exist.
     */
    package_manager(const fs::path& package_root, bool create = false);

    /** Move constructor. */
    package_manager(package_manager&& other)
    : package_root{std::move(other.package_root)}
    {
    }

    /** Move assignment. */
    package_manager& operator=(const package_manager&& other)
    {
        package_root = std::move(other.package_root);
        return *this;
    }

    // disallow default construction and copying.
    package_manager() = delete;
    package_manager(const package_manager&) = delete;

    // disallow copy-assignments.
    package_manager& operator=(const package_manager&) = delete;

    /**
     * Open a package. The package name is relative to
     * the package manager's root.
     *
     * @param name The package name.
     * @throw Throws std::runtime_error if the name was invalid.
     *      If create is false and the package does not exist, also throws
     *      std::runtime_error.
     */
    package open(const std::string& name, bool create = false);

    /**
     * Remove a package. This also removes sub-packages.
     *
     * @param name Name of the package to remove.
     * @throws std::runtime_error if the package name was invalid or if
     *         the package does not exist.
     */
    void remove(const std::string& name);

    /**
     * Check if a package exists.
     *
     * @param name The package to check for existence.
     * @return Returns whether the package exists.
     * @throw std::runtime_error if the package name was invalid.
     */
    bool exists(const std::string& name) const;

    /**
     * Get a sorted vector of all package names in the package root or relative to a parent package.
     *
     * @param include_sub_packages Whether to include all subpackages. Defaults to false.
     * @param parent If specified, get all packages relative to the parent.
     * @return A vector of all package names.
     */
    std::vector<std::string> get_package_names(bool include_sub_packages = false, std::optional<std::string> parent = std::nullopt);

    /**
     * Get the root path for this package manager.
     *
     * @return The package root path.
     */
    const fs::path& get_root_path() const
    {
        return package_root;
    }

    /**
     * Return whether the root path exists in the fileystem.
     */
    bool is_persistent() const;

    /**
     * Create the root path in the filesystem if it does not exist.
     */
    void make_persistent();
};

}    // namespace slang