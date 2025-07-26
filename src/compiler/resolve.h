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

#include "sema.h"

/*
 * Forward declarations.
 */
namespace slang::loader
{
class context;
}    // namespace slang::loader

namespace slang::resolve
{

namespace ld = slang::loader;

/** Name resolution context. */
class context
{
    /** Semantic environment. */
    sema::env& env;

public:
    /** Defaulted and deleted constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = default;

    /**
     * Set up a resolution context.
     *
     * @param env The semantic environment.
     */
    context(sema::env& env)
    : env{env}
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
     * @param env The semantic environment.
     */
    void resolve_imports(ld::context& loader);
};

}    // namespace slang::resolve
