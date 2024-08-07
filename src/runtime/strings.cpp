/**
 * slang - a simple scripting language.
 *
 * runtime string support.
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

void string_equals(si::context& ctx, si::operand_stack& stack)
{
    void* s1 = stack.pop_addr<void*>();
    void* s2 = stack.pop_addr<void*>();

    if(s1 == nullptr || s2 == nullptr)
    {
        throw si::interpreter_error("string_equals: arguments cannot be null.");
    }

    auto& gc = ctx.get_gc();

    auto s1_type = gc.get_object_type(s1);
    auto s2_type = gc.get_object_type(s2);

    if(s1_type != gc::gc_object_type::str || s2_type != gc::gc_object_type::str)
    {
        throw si::interpreter_error("string_equals: arguments are not strings.");
    }

    stack.push_i32(*reinterpret_cast<std::string*>(s1) == *reinterpret_cast<std::string*>(s2));

    gc.remove_temporary(s1);
    gc.remove_temporary(s2);
}

void string_concat(si::context& ctx, si::operand_stack& stack)
{
    std::string* s2 = stack.pop_addr<std::string>();
    std::string* s1 = stack.pop_addr<std::string>();

    if(s1 == nullptr)
    {
        throw slang::interpreter::interpreter_error("string_concat: arguments cannot be null.");
    }

    auto& gc = ctx.get_gc();

    auto s1_type = gc.get_object_type(s1);
    auto s2_type = gc.get_object_type(s2);

    if(s1_type != slang::gc::gc_object_type::str || s2_type != slang::gc::gc_object_type::str)
    {
        throw slang::interpreter::interpreter_error("string_concat: arguments are not strings.");
    }

    std::string* str = gc.gc_new<std::string>(slang::gc::gc_object::of_temporary);
    *str = *s1 + *s2;

    gc.remove_temporary(s1);
    gc.remove_temporary(s2);

    stack.push_addr<std::string>(str);
}

}    // namespace slang::runtime
