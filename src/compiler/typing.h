/**
 * slang - a simple scripting language.
 *
 * type system context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "sema.h"
#include "token.h"
#include "type.h"

/*
 * Forward declartions.
 */
namespace slang::ast
{
class expression; /* ast.h */
}    // namespace slang::ast

namespace slang::typing
{

/** Type errors. */
class type_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;

public:
    /**
     * Construct a type_error.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    type_error(const slang::source_location& loc, const std::string& message);
};

/** Built-in types. */
enum class builtins
{
    null,
    void_,
    i32,
    f32,
    str
};

/** Type kind. */
enum class type_kind
{
    builtin,
    array,
    struct_
};

/**
 * Convert a `type_kind` to a string.
 *
 * @param kind The type kind.
 * @returns Returns a readable string for the type kind.
 */
std::string to_string(type_kind kind);

/** Builtin type info. */
struct builtin_info
{
    builtins id;
};

/** Info for array types. */
struct array_info
{
    /** Element type id. */
    type_id element_type_id;

    /** Array rank. */
    std::size_t rank;
};

/** Field info. */
struct field_info
{
    /** Field name. */
    std::string name;

    /** Field type id. */
    type_id type;
};

/** Info for struct types. */
struct struct_info
{
    /** The struct's name. */
    std::string name;

    /** The struct's fully qualified name. */
    std::optional<std::string> qualified_name;

    /** Fields, in declaration order. */
    std::vector<field_info> fields;

    /** Fields by name. */
    std::unordered_map<std::string, std::size_t> fields_by_name;

    /** Sealed after definition. */
    bool is_sealed{false};

    /** Whether to allow casts from any non-primitive type. */
    bool allow_cast{false};
};

/** Type info. */
struct type_info
{
    /** Type kind. */
    type_kind kind;

    /** Additional data. */
    std::variant<
      builtin_info,
      array_info,
      struct_info>
      data;
};

/** Type system context. */
class context
{
    /** Next type id. */
    std::size_t next_type_id = 0;

    /** Type info map. */
    std::unordered_map<
      type_id,
      type_info>
      type_info_map;

    /** Expression types. */
    std::unordered_map<
      const ast::expression*,
      std::optional<type_id>>
      expression_types;

    /**
     * Generate a new type id.
     *
     * @returns Returns a new type id.
     */
    std::size_t generate_type_id();

    /** Initialize the context with the built-in types. */
    void initialize_builtins();

    /** Add imported types. */
    void add_imports();

    /** Builtin type ids. */
    type_id null_type;
    type_id void_type;
    type_id i32_type;
    type_id f32_type;
    type_id str_type;

public:
    /** Create a typing context. */
    context()
    {
        initialize_builtins();
    }

    /** Deleted constructors. */
    context(const context&) = delete;
    context(context&&) = delete;

    /** Deleted assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Add a builtin type.
     *
     * @param builtin The builtin to add.
     * @returns Returns the type id.
     */
    type_id add_builtin(builtins builtin);

    /**
     * Declare a struct. After declaration, fields can be added.
     *
     * @param name The struct's name.
     * @param qualified_name The struct's qualified name, or `std::nullopt` for structs defined in the current module.
     * @returns Returns the type id of the struct.
     */
    type_id declare_struct(
      std::string name,
      std::optional<std::string> qualified_name);

    /**
     * Add a field to a struct.
     *
     * @param struct_type_id Type id of the struct.
     * @param field_name Name of the field to be added.
     * @param field_type_id Type id of the field to be added.
     * @returns Returns the field index.
     */
    std::size_t add_field(
      type_id struct_type_id,
      std::string field_name,
      type_id field_type_id);

    /**
     * Seal a struct to prevent further mutation.
     *
     * @param struct_type_id Type id of the struct.
     */
    void seal_struct(type_id struct_type_id);

    /**
     * Get the struct info.
     *
     * @param id The struct type id.
     * @returns Returns the associated `struct_info`.
     * @throws Throws a `type_error` if the id was not a struct.
     */
    struct_info& get_struct_info(type_id id);

    /**
     * Get the struct info.
     *
     * @param id The struct type id.
     * @returns Returns the associated `struct_info`.
     * @throws Throws a `type_error` if the id was not a struct.
     */
    const struct_info& get_struct_info(type_id id) const;

    /**
     * Get a struct's field type id by index (including array-related properties).
     *
     * @param struct_type_id The struct type id.
     * @param field_index Field index.
     * @returns Returns a type id for a field.
     */
    type_id get_field_type(
      type_id struct_type_id,
      std::size_t field_index) const;

    /**
     * Get a struct's field index by name (including array-related properties
     * like `length`).
     *
     * @param struct_type_id The struct type id.
     * @param field_name The field's name.
     * @returns Returns the index of the field.
     */
    std::size_t get_field_index(
      type_id struct_type_id,
      const std::string& field_name) const;

    /**
     * Get the id of an existing type.
     *
     * @param name The type's name.
     * @returns Returns the type id.
     * @throws Throws a `type_error` if the type is not known.
     */
    type_id get_type(const std::string& name) const;

    /**
     * Get the full type info for an existing type.
     *
     * @param id The type id.
     * @returns Returns the type info.
     * @throws Throws a `type_error` if the type is not known.
     */
    type_info get_type_info(type_id id) const;

    /**
     * Get the base type for a type.
     *
     * @param id The type id.
     * @returns Returns the type id of the base type.
     * @throws Throws a `type_error` if the type id could not be found.
     */
    type_id get_base_type(type_id id) const;

    /**
     * Get the array rank of a type.
     *
     * @param id The type id.
     * @returns Returns the rank of the array. Return 0 for non-array types.
     * @throws Throws a `type_error` if the type id could not be found.
     */
    std::size_t get_array_rank(type_id id) const;

    /**
     * Resolve a type.
     *
     * @param name The name to be resolved.
     * @returns Returns the type id.
     * @throws Throws a `type_error` if the type could not be resolved.
     */
    type_id resolve_type(const std::string& name);

    /**
     * Check whether an expression type is known, or it is known to have no type.
     *
     * @param expr The expression.
     * @returns Returns `true` if the expression's type is known, or if it is known to have no type.
     */
    bool has_expression_type(const ast::expression& expr) const;

    /**
     * Set an expression's type id.
     *
     * @param expr The expression.
     * @param id Type identifier or `std::nullopt` for the expression.
     * @throws Throws a `type_error` if the expression type is already set.
     */
    void set_expression_type(
      const ast::expression& expr,
      std::optional<type_id> id);

    /**
     * Get an expression's type id.
     *
     * @param expr The expression.
     * @throws Throws a `type_error` if the expression type is not known.
     */
    std::optional<type_id> get_expression_type(const ast::expression& expr) const;

    /**
     * Check if a type is built-in.
     *
     * @param id The type id to check.
     * @returns Returns whether a type is built-in.
     */
    bool is_builtin(type_id id) const;

    /**
     * Check if a type is nullable.
     *
     * @param id The type id to check.
     * @returns Returns whether a type is nullable.
     */
    bool is_nullable(type_id id) const;

    /**
     * Check if a type is a reference.
     *
     * @param id The type id to check.
     * @returns Returns whether a type is a reference.
     */
    bool is_reference(type_id id) const;

    /**
     * Check if the type is an array type.
     *
     * @param id The type id to check.
     * @returns Returns whether a type is an array type.
     */
    bool is_array(type_id id) const;

    /**
     * Check if the type is a struct type.
     *
     * @param id THe type id to check.
     * @returns Returns whether a type is a struct type.
     */
    bool is_struct(type_id id) const;

    /**
     * Get the element type of an array type.
     *
     * @param id The array type id.
     * @returns Returns the element type id.
     * @throws Throws a `type_error` if the given type was not an array type.
     */
    type_id array_element_type(type_id id) const;

    /**
     * Check if types are compatible.
     *
     * @param expected The expected type id.
     * @param actual The actual type.
     * @returns Returns `true` if the types are compatible, and `false` otherwise.
     */
    bool are_types_compatible(type_id expected, type_id actual) const;

    /** Get null type. */
    type_id get_null_type() const
    {
        return null_type;
    }

    /** Get void type. */
    type_id get_void_type() const
    {
        return void_type;
    }

    /** Get i32 type. */
    type_id get_i32_type() const
    {
        return i32_type;
    }

    /** Get f32 type. */
    type_id get_f32_type() const
    {
        return f32_type;
    }

    /** Get str type. */
    type_id get_str_type() const
    {
        return str_type;
    }

    /**
     * Get an array type.
     *
     * @param id The type to get an array for.
     * @param rank Rank of the array.
     * @returns Returns an array type.
     */
    type_id get_array(type_id id, std::size_t rank);

    /**
     * Convert a type id to a string.
     *
     * @param id Type id to convert.
     * @returns Returns a string representation of the type.
     */
    std::string to_string(type_id id) const;

    /** Get a string representation of the context. */
    std::string to_string() const;
};

}    // namespace slang::typing
