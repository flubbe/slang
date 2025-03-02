/**
 * slang - a simple scripting language.
 *
 * built-in macros.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <memory>
#include <vector>

#include "compiler/ast.h"
#include "compiler/codegen.h"
#include "shared/module.h"

namespace slang::codegen::macros
{

/**
 * Expand the built-in 'format!' macro.
 *
 * TODO Parse expressions.
 *
 * @param desc Macro descriptor.
 * @param loc Location of the macro invokation.
 * @param tokens Tokens the macro operates on.
 * @returns The expanded macro.
 */
std::unique_ptr<ast::expression> expand_builtin_format(
  const module_::macro_descriptor& desc,
  token_location loc,
  const std::vector<token>& tokens);

}    // namespace slang::codegen::macros
