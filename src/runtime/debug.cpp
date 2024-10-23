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
    gc_object<std::string> msg_container = gc_pop(ctx, stack);
    std::int32_t condition = stack.pop_i32();

    if(condition == 0)
    {
        std::string* msg = msg_container.get();
        if(msg != nullptr && ctx.get_gc().get_object_type(msg) == gc::gc_object_type::str)
        {
            throw interpreter::interpreter_error(fmt::format("Assertion failed: {}", *msg));
        }

        throw interpreter::interpreter_error("Assertion failed (invalid message).");
    }
}

}    // namespace slang::runtime
