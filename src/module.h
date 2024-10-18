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
#include <variant>
#include <vector>

#include "archives/archive.h"
#include "archives/memory.h"
#include "type.h" /* for slang::typing::is_reference_type */

/* Forward declarations. */
namespace slang::interpreter
{
class operand_stack;
class context;
};    // namespace slang::interpreter

namespace slang::module_
{

namespace si = slang::interpreter;
namespace ty = slang::typing;

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
    /** The symbol's size. If the symbol is an array, this is the size of a single element. */
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

/** Array type. */
enum class array_type : std::uint8_t
{
    i32 = 0,
    f32 = 1,
    str = 2,
    ref = 3
};

/** Convert `array_type` to a readable string. */
inline std::string to_string(array_type type)
{
    switch(type)
    {
    case array_type::i32: return "i32";
    case array_type::f32: return "f32";
    case array_type::str: return "str";
    case array_type::ref: return "ref";
    }

    return "unknown";
}

/**
 * Array type serializer.
 *
 * @param ar The archive to use for serialization.
 * @param t The array type.
 */
inline archive& operator&(archive& ar, array_type& t)
{
    std::uint8_t v = static_cast<std::uint8_t>(t);
    ar & v;
    if(v != static_cast<std::uint8_t>(array_type::i32)
       && v != static_cast<std::uint8_t>(array_type::f32)
       && v != static_cast<std::uint8_t>(array_type::str)
       && v != static_cast<std::uint8_t>(array_type::ref))
    {
        throw serialization_error("Invalid array type.");
    }
    t = static_cast<array_type>(v);
    return ar;
}

/**
 * Encode a type given by a string.
 *
 * @param t The type name.
 * @returns The encoded type string.
 */
std::string encode_type(const std::string& t);

/**
 * Decode a type given by a string.
 *
 * @param t The encoded type string.
 * @returns The decoded type name.
 * @throws Throws a `module_error` if the type is not known.
 */
std::string decode_type(const std::string& t);

/** The type stored in the module. */
class type
{
    /** The string. */
    std::string s;

public:
    /** Default constructors. */
    type() = default;
    type(const type&) = default;
    type(type&&) = default;

    /** Default assignments. */
    type& operator=(const type&) = default;
    type& operator=(type&&) = default;

    /** Initialize from a `std::string`. */
    type(std::string s)
    : s{std::move(s)}
    {
    }

    /** String assignment. */
    type& operator=(const std::string& s)
    {
        this->s = s;
        return *this;
    }

    /** String assignment. */
    type& operator=(std::string&& s)
    {
        this->s = std::move(s);
        return *this;
    }

    /** Comparison. */
    bool operator==(const type& ts) const
    {
        return this->s == ts.s;
    }

    /** Comparison. */
    bool operator!=(const type& ts) const
    {
        return !(*this == ts);
    }

    /** Comparison. */
    bool operator==(const std::string& s) const
    {
        return this->s == s;
    }

    /** Comparison. */
    bool operator!=(const std::string& s) const
    {
        return !(*this == s);
    }

    /** Conversion to `std::string` */
    operator std::string()
    {
        return s;
    }

    /** Conversion to `std::string` */
    operator std::string() const
    {
        return s;
    }

    /*
     * Encode the type string.
     *
     * @returns The encoded type string.
     */
    std::string encode() const
    {
        return encode_type(s);
    }

    /**
     * Set type string from an encoded string.
     *
     * @param s The encoded string.
     */
    void decode(const std::string& s)
    {
        this->s = decode_type(s);
    }
};

/**
 * Type string serializer.
 *
 * @param ar The archive to use for serialization.
 * @param ts The type string.
 */
archive& operator&(archive& ar, type& ts);

/** A variable. */
struct variable : public symbol
{
    /** The variable's type. */
    module_::type type;

    /** Whether this is an array type. */
    bool array;

    /** Whether this is a reference type. */
    bool reference;

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
     * @param array Whether this is an array type.
     */
    variable(std::string type, bool array = false)
    : type{std::move(type)}
    , array{array}
    {
        reference = ty::is_reference_type(type);
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
    ar & v.array;
    v.reference = ty::is_reference_type(v.type);
    return ar;
}

/** Function signature. */
struct function_signature
{
    /** Return type. */
    std::pair<type, bool> return_type;

    /** Argument type list. */
    std::vector<std::pair<type, bool>> arg_types;

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
    function_signature(std::pair<std::string, bool> return_type,
                       std::vector<std::pair<std::string, bool>> arg_types)
    : return_type{std::move(return_type)}
    {
        std::transform(arg_types.cbegin(), arg_types.cend(), std::back_inserter(this->arg_types),
                       [](const auto& arg) -> std::pair<type, bool>
                       {
                           return {arg.first, arg.second};
                       });
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
    std::function<void(si::operand_stack&)> func;

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
    std::size_t args_size{0};

    /** Decoded size of locals. Not serialized. */
    std::size_t locals_size{0};

    /** Decoded return type size. Not serialized. */
    std::size_t return_size{0};

    /** Operand stack size needed for this function. Not serialized. */
    std::size_t stack_size{0};

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

/** Type information of a variable. */
struct type_info
{
    /** The field's base type: i32, f32, str, or a struct name. */
    type base_type;

    /** Whether this is an array. */
    bool array{false};

    /** Package in the import table for imported types. */
    std::optional<std::size_t> package_index;

    /** Type size (not serialized). */
    std::size_t size{0};

    /** Type alignment (not serialized). */
    std::size_t alignment{0};

    /** Offset (not serialized). */
    std::size_t offset{0};

    /** Default constructors. */
    type_info() = default;
    type_info(const type_info&) = default;
    type_info(type_info&&) = default;

    /** Default assignments. */
    type_info& operator=(const type_info&) = default;
    type_info& operator=(type_info&&) = default;

    /**
     * Create a new `type_info`.
     *
     * @param base_type The type's base type.
     * @param array Whether the type is an array type.
     * @param import_index Optional index into the import table. Only for imported types.
     */
    type_info(std::string base_type, bool array, std::optional<std::size_t> import_index = std::nullopt)
    : base_type{std::move(base_type)}
    , array{array}
    , package_index{import_index}
    {
    }

    /** Type comparison. */
    bool operator==(const type_info& other) const
    {
        return base_type == other.base_type && array == other.array;
    }

    /** Type comparison. */
    bool operator!=(const type_info& other) const
    {
        return !(*this == other);
    }
};

/**
 * Serializer for type info.
 *
 * @param ar The archive to use for serialization.
 * @param info The type descriptor.
 */
inline archive& operator&(archive& ar, type_info& info)
{
    ar & info.base_type;
    ar & info.array;
    ar & info.package_index;
    return ar;
}

/** Type descriptor. */
struct type_descriptor
{
    /** Members as (name, type). */
    std::vector<std::pair<std::string, type_info>> member_types;

    /** Type size (not serialized). */
    std::size_t size{0};

    /** Type alignment (not serialized). */
    std::size_t alignment{0};

    /** Type layout id (not serialized). */
    std::size_t layout_id{0};
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
    /** Tag. */
    std::uint32_t tag;

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

    /** The binary part. */
    std::vector<std::byte> binary;

    /** Whether this is a decoded module. */
    bool decoded = false;

    /** Jump targets as `(label_id, offset)`. Only valid during instruction decoding. */
    std::unordered_map<std::int64_t, std::size_t> jump_targets;

    /** Jump origins as `(offset, target_label_id)`. Only valid during instruction decoding. */
    std::unordered_map<std::size_t, std::int64_t> jump_origins;

public:
    /** Module tag. */
    static constexpr std::uint32_t tag = 0x63326c73;

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
    void add_function(std::string name,
                      std::pair<std::string, bool> return_type,
                      std::vector<std::pair<std::string, bool>> arg_types,
                      std::size_t size, std::size_t entry_point, std::vector<variable> locals);

    /**
     * Add a native function to the module.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The function's argument types.
     * @param lib_name Name of the library to import the function from.
     */
    void add_native_function(std::string name,
                             std::pair<std::string, bool> return_type,
                             std::vector<std::pair<std::string, bool>> arg_types,
                             std::string lib_name);

    /**
     * Add a type to the module.
     *
     * @param name The type's name.
     * @param members The type's members as `(name, type)`.
     */
    void add_type(std::string name, std::vector<std::pair<std::string, type_info>> members);

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
    friend class si::context;
};

/**
 * Serializer for the module header.
 *
 * @param ar The archive to use for serialization.
 * @param header The module header.
 */
inline archive& operator&(archive& ar, module_header& header)
{
    if(ar.is_writing())
    {
        header.tag = language_module::tag;
    }
    ar & header.tag;
    if(ar.is_reading() && header.tag != language_module::tag)
    {
        throw module_error("Not a module.");
    }
    ar & header.imports;
    ar & header.exports;
    ar & header.strings;
    return ar;
}

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

}    // namespace slang::module_