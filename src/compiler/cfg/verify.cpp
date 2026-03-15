/**
 * slang - a simple scripting language.
 *
 * Control flow graph verifier.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/codegen/codegen.h"
#include "verify.h"

namespace slang::cfg
{

void verify(const cg::function& func)
{
    for(const auto& block: func.get_basic_blocks())
    {
        if(!block->is_terminated())
        {
            throw cg::codegen_error(
              std::format(
                "CFG verification error: Block '{}' in function '{}' must end with a terminator.",
                block->get_label(),
                func.get_name()));
        }
    }
}

void verify(
  const std::vector<
    std::unique_ptr<
      cg::function>>&
    funcs)
{
    for(const auto& func: funcs)
    {
        verify(*func.get());
    }
}

}    // namespace slang::cfg
