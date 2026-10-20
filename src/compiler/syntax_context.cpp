/**
 * slang - a simple scripting language.
 *
 * syntax context table implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/syntax_context.h"

namespace slang
{

syntax_context_id syntax_context_table::allocate(
  syntax_context_id parent)
{
    syntax_context_id id = next_id++;
    parent_map[id] = parent;
    return id;
}

syntax_context_id syntax_context_table::parent(
  syntax_context_id ctx) const
{
    auto it = parent_map.find(ctx);
    if(it != parent_map.end())
    {
        return it->second;
    }
    return root_syntax_context;
}

void syntax_context_table::clear()
{
    next_id = root_syntax_context + 1;
    parent_map.clear();
}

}    // namespace slang
