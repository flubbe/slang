/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "constant.h"
#include "macro.h"
#include "sema.h"
#include "typing.h"

/*
 * Forward declarations.
 */
namespace slang::loader
{
class context;
}    // namespace slang::loader

namespace slang::resolve
{

namespace const_ = slang::const_;
namespace ld = slang::loader;
namespace ty = slang::typing;

/** Module id. */
using module_id = std::uint64_t;

/** Name resolution context. */
class context
{
    /** Semantic environment. */
    sema::env& sema_env;

    /** Constant environment. */
    const_::env& const_env;

    /** Macro environment. */
    macro::env& macro_env;

    /** Type context. */
    ty::context& type_ctx;

    /** Module map (map `qualified_name -> module_id`). */
    std::unordered_map<std::string, module_id> module_map;

    /** Resolved modules with their loaders (map `module_id -> loader`). */
    std::unordered_map<module_id, ld::context*> resolved_modules;

    /** Module semantic environments. */
    std::unordered_map<module_id, sema::env> module_envs;

    /** Next module id. */
    module_id next_module_id{0};

    /** Generate a new module id. */
    module_id generate_module_id()
    {
        return next_module_id++;
    }

    /** Return a new symbol id. */
    sema::symbol_id generate_symbol_id()
    {
        return sema_env.next_symbol_id++;
    }

    /**
     * Resolve an external symbol.
     *
     * @param module_name The module name.
     * @param module_id The module the symbol is in.
     * @param module_env Semantic environment of the module.
     * @param info Symbol information.
     * @returns Returns the symbol info or `std::nullopt` on failure.
     */
    std::optional<sema::symbol_id> resolve_external(
      const std::string& module_name,
      module_id module_id,
      const sema::env& module_env,
      const sema::symbol_info& info);

    /**
     * Resolve an imported type.
     *
     * @param t The type to resolve.
     * @param loc Location of the import.
     * @param module_name The module name of the type reference.
     * @param header Module header of the type reference.
     * @returns Returns the type id of the resolved type.
     */
    ty::type_id resolve_imported_type(
      const module_::variable_type& t,
      const source_location& loc,
      const std::string& module_name,
      const module_::module_header& header);

    /**
     * Import a constant into the semantic and constant environments
     * and register its type.
     *
     * @param symbol_id The generated symbol id.
     * @param symbol The exported symbol.
     * @param header Symbol origin module header.
     */
    void import_constant(
      sema::symbol_id symbol_id,
      const module_::exported_symbol& symbol,
      const module_::module_header& header);

    /**
     * Import a function into the semantic and type environments.
     *
     * @param symbol_id The generated symbol id.
     * @param symbol The exported symbol.
     * @param module_name The origin module name.
     * @param header Symbol origin module header.
     */
    void import_function(
      sema::symbol_id symbol_id,
      const module_::exported_symbol& symbol,
      const std::string& module_name,
      const module_::module_header& header);

    /**
     * Import a macro into the semantic and macro environments.
     *
     * @param symbol_id The generated symbol id.
     * @param symbol The exported symbol.
     * @param module_name The origin module name.
     */
    void import_macro(
      sema::symbol_id symbol_id,
      const module_::exported_symbol& symbol,
      const std::string& module_name);

    /**
     * Import a type into the semantic and type environments.
     *
     * @param symbol_id The generated symbol id.
     * @param loc Location of the import.
     * @param symbol The exported symbol.
     * @param module_name The origin module name.
     * @param header Symbol origin module header.
     */
    void import_type(
      sema::symbol_id symbol_id,
      const source_location& loc,
      const module_::exported_symbol& symbol,
      const std::string& module_name,
      const module_::module_header& header);

public:
    /** Defaulted and deleted constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = default;

    /**
     * Set up a resolution context.
     *
     * @param sema_env The semantic environment.
     * @param const_env The constant environment.
     * @param macro_env The macro environment.
     * @param type_ctx The type context.
     */
    context(
      sema::env& sema_env,
      const_::env& const_env,
      macro::env& macro_env,
      ty::context& type_ctx)
    : sema_env{sema_env}
    , const_env{const_env}
    , macro_env{macro_env}
    , type_ctx{type_ctx}
    {
    }

    /** Default destructor. */
    ~context() = default;

    /** Deleted assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Resolve a symbol.
     *
     * @param name The symbol's name (qualified or unqualified).
     * @param type Symbol type.
     * @param scope_id The scope id.
     * @returns Returns the symbol id or `std::nullopt`.
     */
    std::optional<sema::symbol_id> resolve(
      const std::string& name,
      sema::symbol_type type,
      sema::scope_id scope_id);

    /**
     * Resolve imports.
     *
     * @param loader The loader to use for resolution.
     */
    void resolve_imports(ld::context& loader);
};

}    // namespace slang::resolve
