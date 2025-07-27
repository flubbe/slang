/**
 * slang - a simple scripting language.
 *
 * name collection.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <string>

#include "location.h"
#include "sema.h"

namespace slang::collect
{

/** A general collection error. */
class collection_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** Name redefinition error. */
class redefinition_error : public collection_error
{
    /** The redefined symbol's name. */
    std::string symbol_name;

    /** The symbol type. */
    sema::symbol_type type;

    /** The symbol's source location. */
    source_location loc;

    /** Location of the original definition. */
    source_location original_loc;

public:
    /**
     * Construct a `redefinition_error`.
     *
     * @param symbol_name Name of the re-defined symbol.
     * @param type Type of the symbol.
     * @param loc Location of the symbol.
     * @param original_loc Location of the original definition.
     */
    explicit redefinition_error(
      const std::string& symbol_name,
      sema::symbol_type type,
      source_location loc,
      source_location original_loc);
};

/** Name collection context. */
class context
{
    /** Semantic environment. */
    sema::env& env;

    /** A reference context, or `nullptr`. */
    context* reference = nullptr;

    /** Current scope. */
    sema::scope_id current_scope;

    /** Anonymous scope id counter. */
    std::size_t anonymous_scope_counter{0};

    /** Generate a name for an anonymous scope. */
    std::string generate_scope_name();

    /**
     * Create a scope. If `scope_id` is `scope::invalid_id`, the global
     * scope is created.
     *
     * @param parent Parent scope. Can be `scope::invalid_id` when creating the
     *               global scope.
     * @param name An optional scope name. If `std::nullopt` is specified, the scope will be set to "scope@loc".
     * @param loc Source location of the scope.
     * @returns Returns the scope id.
     * @throws Throws `std::runtime_error` if the scope table is not empty when creating
     *         the global scope. Throws `redefinition_error` if the scope already exists.
     */
    sema::scope_id create_scope(
      sema::scope_id parent,
      const std::optional<std::string>& name,
      source_location loc);

    /** Return a new scope id. */
    sema::scope_id generate_scope_id()
    {
        return env.next_scope_id++;
    }

    /** Return a new symbol id. */
    sema::symbol_id generate_symbol_id()
    {
        return env.next_symbol_id++;
    }

public:
    /**
     * Global scope id. This is also the smallest valid scope id.
     *
     * The first scope in the AST will be assigned the global scope id.
     * This works, since the AST always has a `block` (creating a scope)
     * at its root.
     */
    static constexpr sema::scope_id global_scope_id{0};

    /** Default constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = default;

    /**
     * Construct the name collection context.
     *
     * @param env The semantic environment to use.
     * @param reference An optional reference context, used e.g. for id generation.
     */
    context(
      sema::env& env,
      context* reference = nullptr)
    : env{env}
    , reference{reference}
    {
        current_scope = sema::scope::invalid_id;
        env.global_scope_id = global_scope_id;
    }

    /** Default destructor. */
    ~context() = default;

    /** Default assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Declare a symbol.
     *
     * @param name The symbol name.
     * @param qualified_name Fully qualified symbol name.
     * @param type The symbol type.
     * @param loc The symbol's location in the source.
     * @param declaring_module Symbol id of the module declaring the name, or `symbol_id::invalid` for the current module.
     * @param transitive Whether the symbol is transitively imported.
     * @param reference Optional reference (AST node or export header entry) to attach.
     * @returns The symbol id.
     */
    sema::symbol_id declare(
      std::string name,
      std::string qualified_name,
      sema::symbol_type type,
      source_location loc,
      sema::symbol_id declaring_module,
      bool transitive,
      std::optional<
        std::variant<
          const ast::expression*,
          const module_::exported_symbol*>>
        reference = std::nullopt);

    /**
     * Attach a child to a symbol.
     *
     * @param parent The parent symbol to attach the child to.
     * @param child The child symbol to attach.
     */
    void attach(
      sema::symbol_id parent,
      sema::symbol_id child);

    /**
     * Enter a scope by pushing it onto the scope stack.
     *
     * @param name An optional name. If not supplied, a name will be generated based on source location.
     * @param loc Source location of the scope.
     * @returns Returns the scope id.
     */
    sema::scope_id push_scope(
      std::optional<std::string> name,
      source_location loc);

    /**
     * Enter an existing scope by id.
     *
     * @param id The scope id.
     * @throws Throws a `collection_error` if the `id` is invalid.
     */
    void push_scope(sema::scope_id id);

    /** Exit a scope by popping it from the scope stack. */
    void pop_scope();

    /**
     * Get the scope corresponding to an id.
     *
     * @note The returned pointer is invalidated when the scope table is rehashed.
     * @param scope_id The scope id.
     * @returns The scope corresponding to the given id.
     * @throws Throws `std::runtime_error` if the scope id is `scope::invalid_id`
     *         or if the scope id was not found.
     */
    [[nodiscard]]
    sema::scope* get_scope(sema::scope_id id);

    [[nodiscard]]
    const sema::scope* get_scope(sema::scope_id id) const;

    /** Get the current scope. */
    [[nodiscard]]
    sema::scope_id get_current_scope() const
    {
        return current_scope;
    }

    /** Get the canonical/qualified scope name. */
    [[nodiscard]]
    std::string get_canonical_scope_name(sema::scope_id id) const;
};

}    // namespace slang::collect
