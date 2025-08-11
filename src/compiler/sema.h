/**
 * slang - a simple scripting language.
 *
 * shared semantic environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <variant>

#include "location.h"
#include "type.h"

namespace slang::ast
{
class expression; /* ast.h */
}    // namespace slang::ast

namespace slang::module_
{
struct exported_symbol; /* module.h */
}    // namespace slang::module_

namespace slang::sema
{

/** A semantic error. */
class semantic_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** Symbol type. */
enum class symbol_type : std::uint8_t
{
    module_import,
    constant_declaration,
    variable_declaration,
    function_definition,
    macro_definition,
    struct_definition
};

/**
 * Convert a `symbol_type` to a readable string.
 *
 * @param t The symbol type.
 * @returns Returns a readable symbol type string.
 */
std::string to_string(symbol_type t);

/** Symbol id type. */
struct symbol_id
{
    using value_type = std::uint64_t;

    /** Symbol id. */
    value_type value{symbol_id::invalid};

    /** Invalid symbol id. */
    constexpr static value_type invalid = static_cast<value_type>(-1);

    /** Default constructors. */
    symbol_id() = default;
    symbol_id(const symbol_id&) = default;
    symbol_id(symbol_id&&) = default;

    /**
     * Create a symbol id from a value.
     *
     * @param symbol_id The symbol id.
     */
    symbol_id(value_type symbol_id)
    : value{symbol_id}
    {
    }

    /** Default assignments. */
    symbol_id& operator=(const symbol_id&) = default;
    symbol_id& operator=(symbol_id&&) = default;

    /** Comparisons. */
    bool operator==(const symbol_id& other) const noexcept
    {
        return value == other.value;
    }
    bool operator!=(const symbol_id& other) const noexcept
    {
        return value != other.value;
    }

    bool operator<(const symbol_id& other) const noexcept
    {
        return value < other.value;
    }

    symbol_id operator++(int)
    {
        return {value++};
    }

    /** std::hash support. */
    struct hash
    {
        std::size_t operator()(const symbol_id& id) const noexcept
        {
            return std::hash<std::uint64_t>{}(id.value);
        }
    };
};

/**
 * `symbol_id` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param id The symbol id to serialize.
 * @returns The input archive.
 */
archive& operator&(archive& ar, symbol_id& id);

/** Scope id. */
using scope_id = std::uint64_t;

/** Scope. */
struct scope
{
    /** Invalid scope id. */
    constexpr static scope_id invalid_id{static_cast<scope_id>(-1)};

    /** Parent scope. */
    scope_id parent;

    /** Scope name. */
    std::string name;

    /** Source location of the scope. */
    source_location loc;

    /** Symbol bindings. */
    std::unordered_map<
      std::string,
      std::unordered_map<
        symbol_type,
        symbol_id>>
      bindings;
};

/** Collected symbol info. */
struct symbol_info
{
    /** Local symbol name. */
    std::string name;

    /** Fully qualified symbol name. */
    std::string qualified_name;

    /** Symbol type. */
    symbol_type type;

    /** Source location, either of the symbol or of the corresponding import statement. */
    source_location loc;

    /** Scope id. */
    scope_id scope;

    /** Module declaring the symbol, or `symbol_info::current_module_id` for the compiled module. */
    symbol_id declaring_module;

    /** Symbol id for the currently compiled module. */
    constexpr static auto current_module_id = symbol_id::invalid;

    /** Declaration info (AST node or import reference). */
    std::optional<
      std::variant<
        const ast::expression*,
        const module_::exported_symbol*>>
      reference{std::nullopt};
};

/** Semantic environment. */
struct env
{
    /** Global scope id. Needs to be set manually, e.g. by a collection context. */
    scope_id global_scope_id{scope::invalid_id};

    /** Next symbol id. */
    symbol_id next_symbol_id{0};

    /** Next scope id. */
    scope_id next_scope_id{0};

    /** Scope table. */
    std::unordered_map<scope_id, scope> scope_map;

    /** Symbol table. */
    std::unordered_map<symbol_id, symbol_info, symbol_id::hash> symbol_table;

    /** Transitive import tracking. */
    std::set<symbol_id> transitive_imports;

    /** Symbol-type bindings. */
    std::unordered_map<symbol_id, typing::type_id, symbol_id::hash> type_map;

    /** Current function return type. */
    std::optional<typing::type_id> current_function_return_type{std::nullopt};

    /** Current function name. */
    std::optional<std::string> current_function_name{std::nullopt};

    /** Default constructors. */
    env() = default;
    env(const env&) = default;
    env(env&&) = default;

    /** Default destructor. */
    ~env() = default;

    /** Default assignments. */
    env& operator=(const env&) = default;
    env& operator=(env&&) = default;

    /**
     * Get a symbol id.
     *
     * @param name The symbol's name (qualified or unqualified).
     * @param type The symbol's type.
     * @param scope_id The symbol's scope.
     * @returns Returns the symbol id or `std::nullopt` if the symbol was not found.
     * @throws Throws a `std::runtime_error` if the scope id is not found.
     */
    [[nodiscard]]
    std::optional<symbol_id> get_symbol_id(
      const std::string& name,
      symbol_type type,
      scope_id id) const;
};

/**
 * Convert the semantic environment into a readable string.
 *
 * @param env The semantic environment.
 */
std::string to_string(const env& env);

}    // namespace slang::sema
