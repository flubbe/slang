/**
 * slang - a simple scripting language.
 *
 * runtime utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

/*
 * Garbage collected temporary.
 */

gc_object_base::~gc_object_base()
{
    if(obj)
    {
        ctx.get_gc().remove_temporary(obj);
        obj = nullptr;
    }
}

gc_object_base gc_pop(si::context& ctx, si::operand_stack& stack)
{
    return {ctx, stack.pop_addr<void*>()};
}

}    // namespace slang::runtime
