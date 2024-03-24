/**
 * slang - a simple scripting language.
 *
 * compiled binary file (=module) support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <exception>
#include <string>
#include <vector>

namespace slang
{

/** A module error. */
class module_error : public std::runtime_error
{
public:
    /**
     * Construct a `module_error`.
     *
     * @param message The error message.
     */
    module_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** Symbol types for imports and exports. */
enum class symbol_type
{
    package = 0,
    variable = 1,
    function = 2,
    type = 3,
};

/** Function signature. */
struct function_signature
{
    /** Return type. */
    std::string return_type;

    /** Argument type list. */
    std::vector<std::string> arg_types;

    /** Default constructors. */
    function_signature() = default;
    function_signature(const function_signature&) = default;
    function_signature(function_signature&&) = default;

    /** Default assignments. */
    function_signature& operator=(const function_signature&) = default;
    function_signature& operator=(function_signature&&) = default;

    /**
     * Construct a function signature from a return type and argument types.
     *
     * @param return_type The function's return type.
     * @param arg_types The function's argument types.
     */
    function_signature(std::string return_type, std::vector<std::string> arg_types)
    : return_type{std::move(return_type)}
    , arg_types{std::move(arg_types)}
    {
    }
};

/** Additional details for native functions. */
struct native_function_details
{
    /** The library name. */
    std::string library_name;

    /** Default constructors. */
    native_function_details() = default;
    native_function_details(const native_function_details&) = default;
    native_function_details(native_function_details&&) = default;

    /** Default assignments. */
    native_function_details& operator=(const native_function_details&) = default;
    native_function_details& operator=(native_function_details&&) = default;

    /**
     * Construct the function details.
     *
     * @param library_name The name of the library this function is implemented in.
     */
    native_function_details(std::string library_name)
    : library_name{library_name}
    {
    }
};

/** Additional details for functions. */
struct function_details
{
    /** Offset into the module file (not counting the header). */
    std::size_t offset;

    /** Size of the function in the module file, in bytes. */
    std::size_t size;
};

/** Function descriptor. */
struct function_descriptor
{
    /** The function's signature. */
    function_signature signature;

    /** Whether this is a native function. */
    bool native{false};

    /** Details. */
    std::variant<function_details, native_function_details> details;

    /** Default constructors. */
    function_descriptor() = default;
    function_descriptor(const function_descriptor&) = default;
    function_descriptor(function_descriptor&&) = default;

    /** Default assignments. */
    function_descriptor& operator=(const function_descriptor&) = default;
    function_descriptor& operator=(function_descriptor&&) = default;

    /**
     * Construct a function descriptor.
     *
     * @param signature The function's signature.
     * @param native Whether the function is native.
     * @param details The function's details.
     */
    function_descriptor(function_signature signature, bool native, std::variant<function_details, native_function_details> details)
    : signature{std::move(signature)}
    , native{native}
    , details{std::move(details)}
    {
    }
};

/** Type descriptor. */
struct type_descriptor
{
    /** Member types. */
    std::vector<std::string> member_types;
};

/** An entry in the import table. */
struct imported_symbol
{
    /** Symbol type. */
    symbol_type type;

    /** Symbol name. */
    std::string name;

    /** Index into the package import table. Unused for package imports (set to `(uint32_t)(-1)`). */
    std::uint32_t package_index;
};

/** An entry in the export table. */
struct exported_symbol
{
    /** Symbol type. */
    symbol_type type;

    /** Symbol name. */
    std::string name;

    /** Type, function signature or type descriptor. */
    std::variant<std::string, function_descriptor, type_descriptor> desc;

    /** Default constructors. */
    exported_symbol() = default;
    exported_symbol(const exported_symbol&) = default;
    exported_symbol(exported_symbol&&) = default;

    /** Default assignments. */
    exported_symbol& operator=(const exported_symbol&) = default;
    exported_symbol& operator=(exported_symbol&&) = default;

    /**
     * Construct an exported symbol.
     *
     * @param type The symbol type.
     * @param name The symbol name.
     * @param desc The symbol's descriptor.
     */
    exported_symbol(symbol_type type, std::string name, std::variant<std::string, function_descriptor, type_descriptor> desc)
    : type{type}
    , name{std::move(name)}
    , desc{std::move(desc)}
    {
    }
};

/** Header of a module. */
struct module_header
{
    /** Import table. */
    std::vector<imported_symbol> imports;

    /** Export table. */
    std::vector<exported_symbol> exports;

    /** String table. */
    std::vector<std::string> strings;
};

/** A compiled binary file. */
class language_module
{
    /** The header. */
    module_header header;

public:
    /** Default constructors. */
    language_module() = default;
    language_module(const language_module&) = default;
    language_module(language_module&&) = default;

    /** Default assignments. */
    language_module& operator=(const language_module&) = default;
    language_module& operator=(language_module&&) = default;

    /**
     * Add a function to the module.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The function's argument types.
     * @param entry_point The entry point of the function.
     */
    void add_function(std::string name, std::string return_type, std::vector<std::string> arg_types, std::size_t entry_point);

    /**
     * Add a native function to the module.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The function's argument types.
     * @param lib_name Name of the library to import the function from.
     */
    void add_native_function(std::string name, std::string return_type, std::vector<std::string> arg_types, std::string lib_name);
};

}    // namespace slang