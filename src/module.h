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
class context;
class module_loader;
class operand_stack;
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
    explicit module_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** Symbol types for imports and exports. */
enum class symbol_type : std::uint8_t
{
    package = 0,
    function = 1,
    type = 2,
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

/** Type of a variable stored in the module. */
class variable_type
{
    /** The decoded type string. */
    std::string decoded_type_string;

    /** Array dimensions (if any). */
    std::optional<std::size_t> array_dims;

public:
    /** Default constructors. */
    variable_type() = default;
    variable_type(const variable_type&) = default;
    variable_type(variable_type&&) = default;

    /** Default assignments. */
    variable_type& operator=(const variable_type&) = default;
    variable_type& operator=(variable_type&&) = default;

    /** Initialize from a `std::string`. */
    variable_type(std::string decoded_type_string, std::optional<std::size_t> array_dims = std::nullopt)
    : decoded_type_string{std::move(decoded_type_string)}
    , array_dims{array_dims}
    {
    }

    /** Comparison. */
    bool operator==(const variable_type& ts) const
    {
        return decoded_type_string == ts.decoded_type_string;
    }

    /** Comparison. */
    bool operator!=(const variable_type& ts) const
    {
        return !(*this == ts);
    }

    /*
     * Encode the type string.
     *
     * @returns The encoded type string.
     */
    std::string encode() const;

    /**
     * Set type string from an encoded string.
     *
     * @param s The encoded string.
     */
    void set_from_encoded(const std::string& s);

    /** Return the base type. */
    std::string base_type() const
    {
        return decoded_type_string;
    }

    /** Whether the type is an array. */
    bool is_array() const
    {
        return array_dims.has_value();
    }

    /** Return the array dimensions or `std::nullopt`. */
    std::optional<std::size_t> get_array_dims() const
    {
        return array_dims;
    }
};

/**
 * Type string serializer.
 *
 * @param ar The archive to use for serialization.
 * @param ts The type string.
 */
archive& operator&(archive& ar, variable_type& ts);

/**
 * Convert a `variable_type` to a readable string.
 *
 * @param t The type.
 * @returns Returns a readable variable type string.
 */
std::string to_string(const variable_type& t);

/** Variable descriptor. */
struct variable_descriptor : public symbol
{
    /** The variable's type. */
    variable_type type;

    /** Whether the base type is a reference type. This is inferred from `type`. */
    bool reference;

    /** Default constructors. */
    variable_descriptor() = default;
    variable_descriptor(const variable_descriptor&) = default;
    variable_descriptor(variable_descriptor&&) = default;

    /** Default assignments. */
    variable_descriptor& operator=(const variable_descriptor&) = default;
    variable_descriptor& operator=(variable_descriptor&&) = default;

    /**
     * Initialize the variable descriptor with a type.
     *
     * @param type The variable type.
     */
    explicit variable_descriptor(variable_type type)
    : type{std::move(type)}
    {
        reference = ty::is_reference_type(type.base_type());
    }
};

/**
 * Serializer for variable descriptors.
 *
 * @param ar The archive to use for serialization.
 * @param desc The variable descriptor.
 */
inline archive& operator&(archive& ar, variable_descriptor& desc)
{
    ar & desc.type;
    desc.reference = ty::is_reference_type(desc.type.base_type());
    return ar;
}

/** Function signature. */
struct function_signature
{
    /** Return type. */
    std::pair<variable_type, bool> return_type;

    /** Argument type list. */
    std::vector<std::pair<variable_type, bool>> arg_types;

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
    {
        std::transform(arg_types.cbegin(), arg_types.cend(), std::back_inserter(this->arg_types),
                       [](const auto& arg) -> std::pair<variable_type, bool>
                       {
                           return {variable_type{
                                     arg.first,
                                     arg.second
                                       ? std::make_optional<std::size_t>(1)    // FIXME dummy array dimensions
                                       : std::nullopt},
                                   arg.second};
                       });

        this->return_type = {variable_type{
                               {std::move(return_type.first), return_type.second},
                               return_type.second
                                 ? std::make_optional<std::size_t>(1)    // FIXME dummy array dimensions
                                 : std::nullopt},
                             return_type.second};
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
    explicit native_function_details(std::string library_name)
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
    std::vector<variable_descriptor> locals;

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
    function_details(std::size_t size, std::size_t offset, std::vector<variable_descriptor> locals)
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

/** Field descriptor. */
struct field_descriptor
{
    /** The field's base type. */
    variable_type base_type;

    /** Package in the import table for imported types. */
    std::optional<std::size_t> package_index;

    /** Type size (not serialized). */
    std::size_t size{0};

    /** Type alignment (not serialized). */
    std::size_t alignment{0};

    /** Offset (not serialized). */
    std::size_t offset{0};

    /** Default constructors. */
    field_descriptor() = default;
    field_descriptor(const field_descriptor&) = default;
    field_descriptor(field_descriptor&&) = default;

    /** Default assignments. */
    field_descriptor& operator=(const field_descriptor&) = default;
    field_descriptor& operator=(field_descriptor&&) = default;

    /**
     * Create a new `field_descriptor`.
     *
     * @param base_type The field's base type.
     * @param array Whether the field is an array type.
     * @param import_index Optional index into the import table. Only for imported types.
     */
    field_descriptor(std::string base_type, bool array, std::optional<std::size_t> import_index = std::nullopt)
    : base_type{std::move(base_type), array ? std::make_optional(1) : std::nullopt}
    , package_index{import_index}
    {
    }

    /** Comparison. */
    bool operator==(const field_descriptor& other) const
    {
        return base_type == other.base_type && base_type.get_array_dims() == other.base_type.get_array_dims();
    }

    /** Comparison. */
    bool operator!=(const field_descriptor& other) const
    {
        return !(*this == other);
    }
};

/**
 * Serializer for field descriptors.
 *
 * @param ar The archive to use for serialization.
 * @param info The type descriptor.
 */
inline archive& operator&(archive& ar, field_descriptor& info)
{
    ar & info.base_type;
    ar & info.package_index;
    return ar;
}

/** Struct flags. */
enum class struct_flags : std::uint8_t
{
    none = 0,      /** No flags. */
    allow_cast = 1 /** Allow casts to and from arbitrary objects. */
};

/** Struct descriptor. */
struct struct_descriptor
{
    /** Struct flags. */
    std::uint8_t flags{0};

    /** Members as (name, type). */
    std::vector<std::pair<std::string, field_descriptor>> member_types;

    /** Type size (not serialized). */
    std::size_t size{0};

    /** Type alignment (not serialized). */
    std::size_t alignment{0};

    /** Type layout id (not serialized). */
    std::size_t layout_id{0};
};

/**
 * Serializer for struct descriptors.
 *
 * @param ar The archive to use for serialization.
 * @param details The struct descriptor.
 */
inline archive& operator&(archive& ar, struct_descriptor& desc)
{
    ar & desc.flags;
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
    std::variant<const si::module_loader*, struct exported_symbol*> export_reference;

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

    /** Function or struct descriptor. */
    std::variant<function_descriptor, struct_descriptor> desc;

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
    exported_symbol(symbol_type type, std::string name, std::variant<function_descriptor, struct_descriptor> desc)
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
    if(s.type == symbol_type::function)
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
            struct_descriptor desc;
            ar & desc;
            s.desc = std::move(desc);
        }
        else if(ar.is_writing())
        {
            auto& desc = std::get<struct_descriptor>(s.desc);
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
    explicit language_module(module_header header)
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
    std::size_t add_import(
      symbol_type type,
      std::string name,
      std::uint32_t package_index = static_cast<std::uint32_t>(-1));

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
                      std::size_t size, std::size_t entry_point, std::vector<variable_descriptor> locals);

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
     * Add a struct to the module.
     *
     * @param name The struct's name.
     * @param members The struct's members as `(name, type)`.
     * @param flags The struct's flags.
     */
    void add_struct(
      std::string name,
      std::vector<std::pair<std::string, field_descriptor>> members,
      std::uint8_t flags);

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
    friend class si::module_loader;
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