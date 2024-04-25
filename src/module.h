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

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "archives/archive.h"
#include "archives/memory.h"

/* Forward declarations. */
namespace slang::interpreter
{
class operand_stack;
class context;
};    // namespace slang::interpreter

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
enum class symbol_type : std::uint8_t
{
    package = 0,
    variable = 1,
    function = 2,
    type = 3,
};

/**
 * `symbol_type` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param s The symbol type to serialize.
 */
inline archive& operator&(archive& ar, symbol_type& s)
{
    std::uint8_t i = static_cast<std::uint8_t>(s);
    ar & i;

    if(i != static_cast<std::uint8_t>(symbol_type::package)
       && i != static_cast<std::uint8_t>(symbol_type::variable)
       && i != static_cast<std::uint8_t>(symbol_type::function)
       && i != static_cast<std::uint8_t>(symbol_type::type))
    {
        throw serialization_error("Invalid symbol type.");
    }

    s = static_cast<symbol_type>(i);
    return ar;
}

/** Return a readable string for a symbol type. */
inline std::string to_string(symbol_type s)
{
    switch(s)
    {
    case symbol_type::package: return "package";
    case symbol_type::variable: return "variable";
    case symbol_type::function: return "function";
    case symbol_type::type: return "type";
    }
    return "<unknown>";
}

/** A symbol. */
struct symbol
{
    /** The symbol's size. */
    std::size_t size;

    /** The offset. */
    std::size_t offset;
};

/**
 * Serializer for symbols.
 *
 * @param ar The archive to use for serialization.
 * @param s The symbol.
 */
inline archive& operator&(archive& ar, symbol& s)
{
    ar & s.offset;
    ar & s.size;
    return ar;
}

/** A variable. */
struct variable : public symbol
{
    /** The variable's type. */
    std::string type;

    /** Default constructors. */
    variable() = default;
    variable(const variable&) = default;
    variable(variable&&) = default;

    /** Default assignments. */
    variable& operator=(const variable&) = default;
    variable& operator=(variable&&) = default;

    /**
     * Construct a variable.
     *
     * @param type The variable's type.
     */
    variable(std::string type)
    : type{std::move(type)}
    {
    }
};

/**
 * Serializer for variables.
 *
 * @param ar The archive to use for serialization.
 * @param s The variable.
 */
inline archive& operator&(archive& ar, variable& v)
{
    ar & v.type;
    return ar;
}

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

/**
 * Function signature serializer.
 *
 * @param ar The archive to use for serialization.
 * @param s The symbol type to serialize.
 */
inline archive& operator&(archive& ar, function_signature& s)
{
    ar & s.return_type;
    ar & s.arg_types;
    return ar;
}

/** Additional details for native functions. */
struct native_function_details
{
    /** The library name. */
    std::string library_name;

    /** The resolved function. Not serialized. */
    std::function<void(slang::interpreter::operand_stack&)> func;

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

/**
 * Detail serializer for native functions.
 *
 * @param ar The archive to use for serialization.
 * @param details The native function details.
 */
inline archive& operator&(archive& ar, native_function_details& details)
{
    ar & details.library_name;
    return ar;
}

/** Additional details for functions. */
struct function_details : public symbol
{
    /** Locals (including arguments). */
    std::vector<variable> locals;

    /** Decoded arguments size. Not serialized. */
    std::size_t args_size;

    /** Decoded size of locals. Not serialized. */
    std::size_t locals_size;

    /** Decoded return type size. Not serialized. */
    std::size_t return_size;

    /** Operand stack size needed for this function. Not serialized. */
    std::size_t stack_size;

    /** Default constructors. */
    function_details() = default;
    function_details(const function_details&) = default;
    function_details(function_details&&) = default;

    /** Default assignments. */
    function_details& operator=(const function_details&) = default;
    function_details& operator=(function_details&&) = default;

    /**
     * Construct function details.
     *
     * @param size The size of the function's bytecode.
     * @param offset The offset of the function
     * @param locals The function's arguments and locals.
     */
    function_details(std::size_t size, std::size_t offset, std::vector<variable> locals)
    : symbol{size, offset}
    , locals{std::move(locals)}
    {
    }
};

/**
 * Serializer for function details.
 *
 * @param ar The archive to use for serialization.
 * @param details The function details.
 */
inline archive& operator&(archive& ar, function_details& details)
{
    ar& static_cast<symbol&>(details);
    ar & details.locals;
    return ar;
}

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

/**
 * Serializer for function descriptors.
 *
 * @param ar The archive to use for serialization.
 * @param details The function descriptor.
 */
inline archive& operator&(archive& ar, function_descriptor& desc)
{
    if(!ar.is_reading() && !ar.is_writing())
    {
        throw serialization_error("Archive has to be reading or writing.");
    }

    ar & desc.signature;
    ar & desc.native;
    if(desc.native)
    {
        if(ar.is_reading())
        {
            native_function_details details;
            ar & details;
            desc.details = std::move(details);
        }
        else if(ar.is_writing())
        {
            auto& details = std::get<native_function_details>(desc.details);
            ar & details;
        }
    }
    else
    {
        if(ar.is_reading())
        {
            function_details details;
            ar & details;
            desc.details = std::move(details);
        }
        else if(ar.is_writing())
        {
            auto& details = std::get<function_details>(desc.details);
            ar & details;
        }
    }
    return ar;
}

/** Type descriptor. */
struct type_descriptor
{
    /** Member types. */
    std::vector<std::string> member_types;
};

/**
 * Serializer for type descriptors.
 *
 * @param ar The archive to use for serialization.
 * @param details The type descriptor.
 */
inline archive& operator&(archive& ar, type_descriptor& desc)
{
    ar & desc.member_types;
    return ar;
}

/** An entry in the import table. */
struct imported_symbol
{
    /** Symbol type. */
    symbol_type type;

    /** Symbol name. */
    std::string name;

    /** Index into the package import table. Unused for package imports (set to `(uint32_t)(-1)`). */
    std::uint32_t package_index;

    /** If the import is resolved, this points to the corresponding module or into the export table. Not serialized. */
    std::variant<const class language_module*, struct exported_symbol*> export_reference;

    /** Default constructors. */
    imported_symbol() = default;
    imported_symbol(const imported_symbol&) = default;
    imported_symbol(imported_symbol&&) = default;

    /** Default assignments. */
    imported_symbol& operator=(const imported_symbol&) = default;
    imported_symbol& operator=(imported_symbol&&) = default;

    /**
     * Construct an imported symbol
     *
     * @param type The symbol's type.
     * @param name The symbol's name.
     * @param package_index The symbol's package index as an index of the import table. Unused for package imports.
     */
    imported_symbol(symbol_type type, std::string name, std::uint32_t package_index = static_cast<std::uint32_t>(-1))
    : type{type}
    , name{std::move(name)}
    , package_index{package_index}
    {
    }
};

/**
 * Serializer for imported symbols.
 *
 * @param ar The archive to use for serialization.
 * @param details The imported symbol.
 */
inline archive& operator&(archive& ar, imported_symbol& s)
{
    ar & s.type;
    ar & s.name;
    ar & s.package_index;
    return ar;
}

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

/**
 * Serializer for exported symbols.
 *
 * @param ar The archive to use for serialization.
 * @param details The exported symbol.
 */
inline archive& operator&(archive& ar, exported_symbol& s)
{
    if(!ar.is_reading() && !ar.is_writing())
    {
        throw serialization_error("Archive has to be reading or writing.");
    }

    ar & s.type;
    ar & s.name;
    if(s.type == symbol_type::variable)
    {
        if(ar.is_reading())
        {
            std::string desc;
            ar & desc;
            s.desc = std::move(desc);
        }
        else if(ar.is_writing())
        {
            auto& desc = std::get<std::string>(s.desc);
            ar & desc;
        }
    }
    else if(s.type == symbol_type::function)
    {
        if(ar.is_reading())
        {
            function_descriptor desc;
            ar & desc;
            s.desc = std::move(desc);
        }
        else if(ar.is_writing())
        {
            auto& desc = std::get<function_descriptor>(s.desc);
            ar & desc;
        }
    }
    else if(s.type == symbol_type::type)
    {
        if(ar.is_reading())
        {
            type_descriptor desc;
            ar & desc;
            s.desc = std::move(desc);
        }
        else if(ar.is_writing())
        {
            auto& desc = std::get<type_descriptor>(s.desc);
            ar & desc;
        }
    }
    else if(s.type != symbol_type::package)
    {
        throw serialization_error("Unknown symbol type.");
    }

    return ar;
}

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

/**
 * Serializer for the module header.
 *
 * @param ar The archive to use for serialization.
 * @param header The module header.
 */
inline archive& operator&(archive& ar, module_header& header)
{
    ar & header.imports;
    ar & header.exports;
    ar & header.strings;
    return ar;
}

/** A compiled binary file. */
class language_module
{
    /** The header. */
    module_header header;

    /** The binary part. */
    std::vector<std::byte> binary;

    /** Whether this is a decoded module. */
    bool decoded = false;

    /** Jump targets as (label_id, offset). Only valid during instruction decoding. */
    std::unordered_map<std::int64_t, std::size_t> jump_targets;

    /** Jump origins as (offset, target_label_id). Only valid during instruction decoding. */
    std::unordered_map<std::size_t, std::int64_t> jump_origins;

public:
    /** Default constructors. */
    language_module() = default;
    language_module(const language_module&) = default;
    language_module(language_module&&) = default;

    /** Default assignments. */
    language_module& operator=(const language_module&) = default;
    language_module& operator=(language_module&&) = default;

    /**
     * Construct a module from header.
     *
     * @param header THe module's header.
     */
    language_module(module_header header)
    : header{std::move(header)}
    {
    }

    /**
     * Add an import to the module.
     *
     * @param type The symbol type.
     * @param name The symbol's name.
     * @param package_index The symbol's package index as an index into the import table. Ignored for `symbol_type::package`.
     * @returns The import's index inside the import table.
     */
    std::size_t add_import(symbol_type type, std::string name, std::uint32_t package_index = static_cast<std::uint32_t>(-1));

    /**
     * Add a function to the module.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The function's argument types.
     * @param size The function's bytecode size.
     * @param entry_point The entry point of the function.
     * @param locals The function's arguments and locals.
     */
    void add_function(std::string name, std::string return_type, std::vector<std::string> arg_types, std::size_t size, std::size_t entry_point, std::vector<variable> locals);

    /**
     * Add a native function to the module.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The function's argument types.
     * @param lib_name Name of the library to import the function from.
     */
    void add_native_function(std::string name, std::string return_type, std::vector<std::string> arg_types, std::string lib_name);

    /**
     * Set the string table.
     *
     * @param strings The new string table.
     */
    void set_string_table(const std::vector<std::string>& strings)
    {
        header.strings = strings;
    }

    /**
     * Set the binary module part.
     *
     * @param binary The binary part.
     */
    void set_binary(std::vector<std::byte> binary)
    {
        this->binary = std::move(binary);
    }

    /** Get the module header. */
    const module_header& get_header() const
    {
        return header;
    }

    /** Get the binary. */
    const std::vector<std::byte>& get_binary() const
    {
        return binary;
    }

    /** Get whether the module is decoded. */
    bool is_decoded() const
    {
        return decoded;
    }

    friend archive& operator&(archive& ar, language_module& mod);
    friend class slang::interpreter::context;
};

/**
 * Module serializer.
 *
 * @param ar The archive.
 * @returns The archive.
 */
inline archive& operator&(archive& ar, language_module& mod)
{
    ar & mod.header;
    ar & mod.binary;
    return ar;
}

}    // namespace slang