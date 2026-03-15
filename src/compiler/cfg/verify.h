/**
 * slang - a simple scripting language.
 *
 * Control flow graph verifier.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <memory>

/*
 * Forward declarations.
 */
namespace slang::codegen
{
class function;
}    // namespace slang::codegen

namespace slang::cfg
{

namespace cg = slang::codegen;

/**
 * Verify the control flow graph of a function.
 *
 * @param func The function to verify.
 * @throws Throws a `cg::codegen_error` if the verification failed.
 */
void verify(const cg::function& func);

/**
 * Verify the control flow graph of a vector of functions.
 *
 * @param funcs The functions to verify.
 * @throws Throws a `cg::codegen_error` if the verification failed.
 */
void verify(
  const std::vector<
    std::unique_ptr<
      cg::function>>&
    funcs);

}    // namespace slang::cfg
