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

/** Name resolution context. */
class context
{
    /** Semantic environment. */
    sema::env& sema_env;

    /** Constant environment. */
    const_::env& const_env;

    /** Type context. */
    ty::context& type_ctx;

    /** Resolved modules with their loaders (map `qualified_name -> loader`). */
    std::unordered_map<std::string, ld::context*> resolved_modules;

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
     * @param type_ctx The type context.
     */
    context(
      sema::env& sema_env,
      const_::env& const_env,
      ty::context& type_ctx)
    : sema_env{sema_env}
    , const_env{const_env}
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
