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

#include "compiler/ast/ast.h"
#include "compiler/codegen.h"
#include "shared/module.h"

namespace slang::codegen::macros
{

/**
 * Expand the built-in 'format!' macro.
 *
 * @param desc Macro descriptor.
 * @param loc Location of the macro invokation.
 * @param exprs Expressions the macro operates on.
 * @returns The expanded macro.
 */
std::unique_ptr<ast::expression> expand_builtin_format(
  const module_::macro_descriptor& desc,
  token_location loc,
  const std::vector<std::unique_ptr<ast::expression>>& exprs);

}    // namespace slang::codegen::macros
