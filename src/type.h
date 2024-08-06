/**
 * slang - a simple scripting language.
 *
 * type representation in the typing system.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <vector>

#include "token.h"

namespace slang::typing
{

/** Type class. */
enum class type_class : std::uint8_t
{
    tc_plain = 0,   /** Plain (non-array, non-struct, non-function) type. */
    tc_array = 1,   /** Array type. */
    tc_struct = 2,  /** Struct type. */
    tc_function = 3 /** Function type. */
};

/** Type representation. */
class type
{
    /** Source location of the type (if available). */
    token_location location;

    /** Optional name for this type. Only set when `array` is `false`. */
    std::optional<std::string> name;

    /** The type class. */
    type_class cls;

    /**
     * Components of the type for arrays, structs and functions.
     * - For arrays, this has length 1 and contains the array's base type.
     * - For structs, this is a list of all contained types.
     * - For functions, the first entry is the return type and the following types are the argument types.
     */
    std::vector<std::shared_ptr<type>> components;

    /** Type id or std::nullopt for unresolved types. */
    std::optional<std::uint64_t> type_id;

public:
    /** Default constructors. */
    type() = default;
    type(const type&) = default;
    type(type&&) = default;

    /** Default assignment operators. */
    type& operator=(const type&) = default;
    type& operator=(type&&) = default;

    /**
     * Create a new type.
     *
     * TODO Remove `base` and add `name`, `location` and `components` as optional parameters.
     *
     * @param base The base type name.
     * @param cls The type class.
     * @param type_id The type's id, or std::nullopt for an unresolved type.
     */
    type(const token& base, type_class cls, std::optional<std::uint64_t> type_id);

    /**
     * Return if two types are equal.
     *
     * @throws Throws a `type_error` if the types are not both resolved.
     */
    bool operator==(const type& other) const;

    /**
     * Return if two types are not equal.
     *
     * @throws Throws a `type_error` if the types are not both resolved.
     */
    bool operator!=(const type& other) const;

    /** Get the token location. */
    const token_location& get_location() const
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
    type* get_element_type();

    /**
     * Return the element type.
     *
     * @throws Throws a `type_error` if the type is not an array type.
     */
    const type* get_element_type() const;

    /**
     * Return the function signature for function types.
     *
     * @throws Throws a `type_error` if the type is not a function type.
     */
    const std::vector<std::shared_ptr<type>>& get_signature() const;

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

    /**
     * Get the string representation of this type.
     *
     * @return The string representation of this type.
     */
    std::string to_string() const
    {
        if(is_array())
        {
            return fmt::format("[{}]", get_element_type()->to_string());
        }
        return *name;
    }

    /**
     * Helper to create an unresolved type.
     *
     * @param base The base type name.
     * @param cls The type class.
     */
    static type make_unresolved(token base, type_class cls)
    {
        return {std::move(base), cls, std::nullopt};
    }
};

/**
 * Check whether a string represents a built-in type, that is,
 * void, i32, f32 or str.
 */
inline bool is_builtin_type(const std::string& s)
{
    return s == "void" || s == "i32" || s == "f32" || s == "str";
}

/**
 * Convert a type to a string.
 *
 * @param t The type to convert.
 */
std::string to_string(const type& t);

/**
 * Convert a type to a string.
 *
 * @param t The type to convert, given as a pair of `(base_type, optional_array_length)`.
 */
std::string to_string(const std::pair<token, bool>& t);

/**
 * Convert a type to a string.
 *
 * @param t The type to convert, given as a pair of `(base_type, optional_array_length)`.
 */
std::string to_string(const std::pair<std::string, bool>& t);

}    // namespace slang::typing
