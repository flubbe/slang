/**
 * slang - a simple scripting language.
 *
 * defines a directive, as used by the AST and codegen.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <vector>
#include <utility>

#include "token.h"

namespace slang::codegen
{

/** A directive that modifies an expression. */
struct directive
{
    /** The directive's name. */
    token name;

    /** The directive's arguments. */
    std::vector<std::pair<token, token>> args;

    /** Default constructors. */
    directive() = default;
    directive(const directive&) = default;
    directive(directive&&) = default;

    /** Default assignments. */
    directive& operator=(const directive&) = default;
    directive& operator=(directive&&) = default;

    /** Constructor. */
    directive(token name, std::vector<std::pair<token, token>> args)
    : name{std::move(name)}
    , args{std::move(args)}
    {
    }
};

}    // namespace slang::codegen
