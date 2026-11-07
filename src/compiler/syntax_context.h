/**
 * slang - a simple scripting language.
 *
 * syntax context tracking for hygiene.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <unordered_map>

#include "shared/token.h"

namespace slang
{

/**
 * Syntax context tracking for macro hygiene.
 *
 * Each macro expansion allocates a fresh syntax_context_id.
 * This class records the parent relationship so that identifier lookups
 * can traverse the syntax context hierarchy.
 */
class syntax_context_table
{
    /** Next syntax context id to allocate. */
    syntax_context_id next_id{root_syntax_context + 1};

    /** Map from context id to parent context id. */
    std::unordered_map<
      syntax_context_id,
      syntax_context_id>
      parent_map;

public:
    /**
     * Create a new syntax context (hygiene tag).
     *
     * @param parent The parent context. Defaults to `root_syntax_context`.
     * @return A newly allocated context identifier.
     */
    syntax_context_id allocate(
      syntax_context_id parent = root_syntax_context);

    /**
     * Retrieve the parent of a context.
     *
     * @param ctx The syntax context id.
     * @return Returns `root_syntax_context` if the context has no explicit parent or is root.
     */
    [[nodiscard]]
    syntax_context_id parent(
      syntax_context_id ctx) const;

    /** Clear all allocated contexts. */
    void clear();

    /**
     * Remap an external context ID into this environment.
     *
     * Preserves the parent-child ancestry of the external context while
     * allocating fresh IDs in this environment. If the same external ID
     * is remapped twice, the first mapping is returned (idempotent).
     *
     * @param external_ctx The context ID from another module's environment.
     * @returns A local context ID with equivalent ancestry in this environment.
     */
    [[nodiscard]]
    syntax_context_id remap(
      syntax_context_id external_ctx);
};

}    // namespace slang
