/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "typing.h"
#include "resolve.h"

namespace ty = slang::typing;

namespace slang::resolve
{

/*
 * Exceptions.
 */

resolve_error::resolve_error(const token_location& loc, const std::string& message)
: std::runtime_error{fmt::format("{}: {}", to_string(loc), message)}
{
}

/*
 * resolver context.
 */

void context::resolve_imports(ty::context& ctx)
{
    const std::vector<std::vector<token>>& imports = ctx.get_imports();

    for(auto& import: imports)
    {
        // TODO
        throw resolve_error("context::resolve_import not implemented.");
    }
}

}    // namespace slang::resolve