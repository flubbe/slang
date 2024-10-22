/**
 * slang - a simple scripting language.
 *
 * runtime debugging support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

void assert_([[maybe_unused]] si::context& ctx, si::operand_stack& stack)
{
    if(!stack.pop_i32())
    {
        throw interpreter::interpreter_error("Assertion failed.");
    }
}

}    // namespace slang::runtime
