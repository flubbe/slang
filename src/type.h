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

#include "token.h"

namespace slang::typing
{

/** Type representation. */
class type
{
    /** Source location of the type (if available). */
    token_location location;

    /** Optional name for this type. Only set when `array` is `false`. */
    std::optional<std::string> name;

    /** Whether the type is an array. */
    bool array{false};

    /**
     * The element type for arrays.
     *
     * TODO Make this into a vector / type components. For functions, the
     *      return type is at index 0, and the arguments at indices >= 1.
     */
    std::shared_ptr<type> element_type;

    /** Type id or std::nullopt for unresolved types. */
    std::optional<std::uint64_t> type_id;

    /** Whether this is a function type. */
    bool function_type = false;

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
     * FIXME Complicated and non-flexible member initialization.
     *
     * @param base The base type name.
     * @param array Whether the type is an array.
     * @param type_id The type's id, or std::nullopt for an unresolved type.
     * @param function_type Whether this type comes from a function.
     */
    type(const token& base, bool array, std::optional<std::uint64_t> type_id, bool function_type)
    : location{base.location}
    , name{array ? std::nullopt : std::make_optional<std::string>(base.s)}
    , array{array}
    , element_type{array ? std::make_shared<type>(base, false, std::nullopt, false) : nullptr}
    , type_id{type_id}
    , function_type{function_type}
    {
    }

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

    /** Return the element type. Returns `nullptr` if the type is not an array. */
    const type* get_element_type() const
    {
        if(!is_array())
        {
            return nullptr;
        }
        return element_type.get();
    }

    /** Return whether this type is an array type. */
    bool is_array() const
    {
        return array;
    }

    /** Return whether this is a function type. */
    bool is_function_type() const
    {
        return function_type;
    }

    /**
     * Set the type id.
     */
    void set_type_id(std::optional<std::uint64_t> new_id)
    {
        type_id = new_id;
    }

    /**
     * Return the type id of this type.
     *
     * @note The type needs to be resolved first.
     */
    std::uint64_t get_type_id() const
    {
        return *type_id;
    }

    /** Return whether the type is resolved. */
    bool is_resolved() const
    {
        return type_id.has_value();
    }

    /**
     * Helper to create an unresolved type.
     *
     * @param base The base type name.
     * @param array Whether the type is an array type.
     * @param function_type Whether this type comes from a function.
     */
    static type make_unresolved(token base, bool array, bool function_type)
    {
        return {std::move(base), array, std::nullopt, function_type};
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
