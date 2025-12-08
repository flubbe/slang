/**
 * slang - a simple scripting language.
 *
 * runtime debugging support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

void assert_(si::context& ctx, si::operand_stack& stack)
{
    /*
     * note: the second argument was pushed last onto stack, so it's the first we pop.
     */
    auto [msg_container] = get_args<gc_object<std::string>>(ctx, stack);
    std::int32_t condition = stack.pop_cat1<std::int32_t>();

    if(condition == 0)
    {
        std::string* msg = msg_container.get();
        if(msg != nullptr && ctx.get_gc().get_object_type(msg) == gc::gc_object_type::str)
        {
            throw interpreter::interpreter_error(std::format("Assertion failed: {}", *msg));
        }

        throw interpreter::interpreter_error("Assertion failed (invalid message).");
    }
}

}    // namespace slang::runtime
