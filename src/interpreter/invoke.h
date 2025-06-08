/**
 * slang - a simple scripting language.
 *
 * function invocation helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <functional>
#include <vector>

#include "value.h"

namespace slang::interpreter
{

/**
 * Invoke a function.
 *
 * @param ctx Interpreter context.
 * @param module_name Name of the module that contains the function.
 * @param function_name Name of the function.
 * @param args Arguments that are passed to the function.
 * @returns The function's return value.
 */
template<typename... Args>
value invoke(
  context& ctx,
  const std::string& module_name,
  const std::string& function_name,
  Args&&... args)
{
    return ctx.invoke(
      module_name,
      function_name,
      move_into_value_vector(
        std::make_tuple(args...)));
}

/**
 * Invoke a function.
 *
 * @param fn The function.
 * @param args Arguments that are passed to the function.
 * @returns The function's return value.
 */
template<typename... Args>
value invoke(
  function& fn,
  Args&&... args)
{
    return fn(std::forward<Args>(args)...);
}

}    // namespace slang::interpreter
