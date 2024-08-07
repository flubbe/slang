/**
 * slang - a simple scripting language.
 *
 * runtime array support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter.h"
#include "runtime/runtime.h"

namespace gc = slang::gc;

namespace slang::runtime
{

void array_copy(si::context& ctx, si::operand_stack& stack)
{
    void* to = stack.pop_addr<void*>();
    void* from = stack.pop_addr<void*>();

    if(to == nullptr)
    {
        throw slang::interpreter::interpreter_error("array_copy: 'to' is null.");
    }
    if(from == nullptr)
    {
        throw slang::interpreter::interpreter_error("array_copy: 'from' is null.");
    }

    auto& gc = ctx.get_gc();

    auto to_type = gc.get_object_type(to);
    auto from_type = gc.get_object_type(from);

    if(to_type != from_type)
    {
        throw slang::interpreter::interpreter_error("array_copy: type mismatch.");
    }

    // copy array with bounds check.
    if(to_type == slang::gc::gc_object_type::array_i32 || to_type == slang::gc::gc_object_type::array_f32)
    {
        auto from_array = reinterpret_cast<si::fixed_vector<std::int32_t>*>(from);
        auto to_array = reinterpret_cast<si::fixed_vector<std::int32_t>*>(to);
        for(std::size_t i = 0; i < from_array->size(); ++i)
        {
            to_array->at(i) = from_array->at(i);
        }
    }
    else if(to_type == slang::gc::gc_object_type::array_str
            || to_type == slang::gc::gc_object_type::array_aref)
    {
        auto from_array = reinterpret_cast<si::fixed_vector<void*>*>(from);
        auto to_array = reinterpret_cast<si::fixed_vector<void*>*>(to);
        for(std::size_t i = 0; i < from_array->size(); ++i)
        {
            to_array->at(i) = from_array->at(i);
        }
    }
    else
    {
        throw slang::interpreter::interpreter_error("array_copy: unsupported array type.");
    }

    gc.remove_temporary(from);
    gc.remove_temporary(to);
}

}    // namespace slang::runtime