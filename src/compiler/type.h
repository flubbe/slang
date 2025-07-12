/**
 * slang - a simple scripting language.
 *
 * type representation in the typing system.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "token.h"

namespace slang::typing
{

/** Type class. */
enum class type_class : std::uint8_t
{
    tc_plain = 0,    /** Plain (non-array, non-struct, non-function) type. */
    tc_array = 1,    /** Array type. */
    tc_struct = 2,   /** Struct type. */
    tc_function = 3, /** Function type. */

    tc_declaration = 4, /** Any declaration. */

    last = tc_declaration /** Last element. */
};

/**
 * `type_class` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param cls The type class to serialize.
 * @returns The input archive.
 */
archive& operator&(archive& ar, type_class& cls);

/** Type representation. */
class type_info
{
    /** Source location of the type (if available). */
    source_location location;

    /** Optional name for this type. Only set when `cls` is not `type_class::tc_array`. */
    std::optional<std::string> name;

    /** The type class. */
    type_class cls;

    /**
     * Components of the type for arrays, structs and functions.
     * - For arrays, this has length 1 and contains the array's base type.
     * - For structs, this is a list of all contained types.
     * - For functions, the first entry is the return type and the following types are the argument types.
     */
    std::vector<std::shared_ptr<type_info>> components;

    /** Type id or std::nullopt for unresolved types. */
    std::optional<std::uint64_t> type_id;

    /** Optional import path. Only set when `cls` is not `type_class::tc_array`. */
    std::optional<std::string> import_path;

public:
    /** Default constructors. */
    type_info() = default;
    type_info(const type_info&) = default;
    type_info(type_info&&) = default;

    /** Default assignment operators. */
    type_info& operator=(const type_info&) = default;
    type_info& operator=(type_info&&) = default;

    /**
     * Create a new type info.
     *
     * TODO Remove `base` and add `name`, `location` and `components` as optional parameters.
     *
     * @param base The base type name.
     * @param cls The type class.
     * @param type_id The type's id, or `std::nullopt` for an unresolved type.
     * @param import_path Optional import path.
     */
    type_info(
      const token& base,
      type_class cls,
      std::optional<std::uint64_t> type_id,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Return if two types are equal.
     *
     * @throws Throws a `type_error` if the types are not both resolved.
     */
    bool operator==(const type_info& other) const;

    /**
     * Return if two types are not equal.
     *
     * @throws Throws a `type_error` if the types are not both resolved.
     */
    bool operator!=(const type_info& other) const;

    /**
     * Serialize `type_info`.
     *
     * @param ar The archive to use for serialization.
     */
    void serialize(archive& ar);

    /** Get the token location. */
    const source_location& get_location() const
    {
        return location;
    }

    /** Get the type class. */
    type_class get_type_class() const
    {
        return cls;
    }

    /**
     * Return the element type for arrays.
     *
     * @throws Throws a `type_error` if the type is not an array type.
     */
    type_info* get_element_type();

    /**
     * Return the element type.
     *
     * @throws Throws a `type_error` if the type is not an array type.
     */
    const type_info* get_element_type() const;

    /**
     * Return the function signature for function types.
     *
     * @throws Throws a `type_error` if the type is not a function type.
     */
    const std::vector<std::shared_ptr<type_info>>& get_signature() const;

    /** Return whether this type is an array type. */
    bool is_array() const
    {
        return cls == type_class::tc_array;
    }

    /** Return whether this is a function type. */
    bool is_function_type() const
    {
        return cls == type_class::tc_function;
    }

    /** Set the type id. */
    void set_type_id(std::uint64_t new_id)
    {
        type_id = new_id;
    }

    /**
     * Return the type id of this type.
     *
     * @note The type needs to be resolved first.
     * @throws Throws a `type_error` if the type id is not resolved.
     */
    std::uint64_t get_type_id() const;

    /** Return whether the type is resolved. */
    bool is_resolved() const
    {
        return type_id.has_value();
    }

    /** Get the import path. */
    std::optional<std::string> get_import_path() const
    {
        return import_path;
    }

    /**
     * Get the string representation of this type.
     *
     * @return The string representation of this type.
     */
    std::string to_string() const;

    /**
     * Helper to create an unresolved type.
     *
     * @param base The base type name.
     * @param cls The type class.
     * @param import_path The import path, or `std::nullopt` for the current module.
     */
    static type_info make_unresolved(
      token base,
      type_class cls,
      std::optional<std::string> import_path)
    {
        return {std::move(base), cls, std::nullopt, std::move(import_path)};
    }
};

/**
 * Serialize `type_info`.
 *
 * @param ar The archive to use for serialization.
 * @param info The type info to serialize.
 * @returns The input archive.
 */
inline archive& operator&(archive& ar, type_info& info)
{
    info.serialize(ar);
    return ar;
}

/**
 * Convert a type to a string.
 *
 * @param t The type to convert.
 */
std::string to_string(const type_info& t);

/**
 * Convert a type to a string.
 *
 * @param t The type to convert, given as a pair of `(base_type, is_array)`.
 */
std::string to_string(const std::pair<token, bool>& t);

}    // namespace slang::typing
