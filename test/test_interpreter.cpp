/**
 * slang - a simple scripting language.
 *
 * Interpreter tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "module.h"

#include "archives/file.h"
#include "runtime/runtime.h"
#include "interpreter.h"

namespace
{

namespace si = slang::interpreter;
namespace rt = slang::runtime;

TEST(interpreter, module_and_functions)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("test_output.bin");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'test_output.bin'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};
    ASSERT_NO_THROW(ctx.load_module("test_output", mod));

    si::value res;
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "itest", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "iadd", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "isub", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "imul", {}));
    EXPECT_EQ(*res.get<int>(), 6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "idiv", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "ftest", {}));
    EXPECT_NEAR(*res.get<float>(), 1.1, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "fadd", {}));
    EXPECT_NEAR(*res.get<float>(), 3.2, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "fsub", {}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "fmul", {}));
    EXPECT_NEAR(*res.get<float>(), 6.51, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "fdiv", {}));
    EXPECT_NEAR(*res.get<float>(), 3.2, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "stest", {}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg", {si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 2);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg", {si::value{15}}));
    EXPECT_EQ(*res.get<int>(), 16);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg", {si::value{-100}}));
    EXPECT_EQ(*res.get<int>(), -99);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg2", {si::value{1.0f}}));
    EXPECT_NEAR(*res.get<float>(), 3.0, 1e-6);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg2", {si::value{-1.0f}}));
    EXPECT_NEAR(*res.get<float>(), -1.0, 1e-6);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg2", {si::value{0.f}}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "sid", {si::value{"Test"}}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "call", {si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "local", {si::value{0}}));
    EXPECT_EQ(*res.get<int>(), -1);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "local2", {si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "local3", {}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "cast_i2f", {si::value{23}}));
    EXPECT_EQ(*res.get<float>(), 23.0);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "cast_f2i", {si::value{92.3f}}));
    EXPECT_EQ(*res.get<int>(), 92);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, hello_world)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("hello_world.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'hello_world.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path("src/lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                     ctx.get_gc().remove_temporary(s);
                                 });

    ASSERT_NO_THROW(ctx.load_module("hello_world", mod));
    EXPECT_NO_THROW(ctx.invoke("hello_world", "main", {si::value{"Test"}}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Hello, World!\n");

    // re-defining functions should fail.
    EXPECT_THROW(ctx.register_native_function("slang", "println",    // collides with a native function name
                                              []([[maybe_unused]] si::operand_stack& stack) {}),
                 si::interpreter_error);
    EXPECT_THROW(ctx.register_native_function("hello_world", "main",    // collides with a scripted function name
                                              []([[maybe_unused]] si::operand_stack& stack) {}),
                 si::interpreter_error);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, operators)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("operators.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'operators.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("operators", mod));
    ASSERT_NO_THROW(ctx.invoke("operators", "main", {}));

    si::value res;
    ASSERT_NO_THROW(res = ctx.invoke("operators", "and", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 3);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{0}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{1}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{0}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{1}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "or", {si::value{27}, si::value{4}}));
    EXPECT_EQ(*res.get<int>(), 31);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{0}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{1}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{0}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{1}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "xor", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 24);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "shl", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 216);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "shr", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 3);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "mod", {si::value{127}, si::value{23}}));
    EXPECT_EQ(*res.get<int>(), 12);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, control_flow)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("control_flow.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'control_flow.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path("src/lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                     ctx.get_gc().remove_temporary(s);
                                 });

    ASSERT_NO_THROW(ctx.load_module("control_flow", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("control_flow", "test_if_else", {si::value{2}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("control_flow", "test_if_else", {si::value{-1}}));
    EXPECT_EQ(*res.get<int>(), 0);

    ASSERT_NO_THROW(ctx.invoke("control_flow", "conditional_hello_world", {si::value{3.f}}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(ctx.invoke("control_flow", "conditional_hello_world", {si::value{2.2f}}));
    ASSERT_EQ(print_buf.size(), 2);
    EXPECT_EQ(print_buf.back(), "World, hello!\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    print_buf.clear();
    ASSERT_NO_THROW(ctx.invoke("control_flow", "no_else", {si::value{1}}));
    ASSERT_EQ(print_buf.size(), 2);
    EXPECT_EQ(print_buf[0], "a>0\n");
    EXPECT_EQ(print_buf[1], "Test\n");

    print_buf.clear();
    ASSERT_NO_THROW(ctx.invoke("control_flow", "no_else", {si::value{-2}}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Test\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, loops)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("loops.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'loops.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path("src/lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                     ctx.get_gc().remove_temporary(s);
                                 });

    ASSERT_NO_THROW(ctx.load_module("loops", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("loops", "main", {}));
    EXPECT_EQ(print_buf.size(), 10);
    for(auto& it: print_buf)
    {
        EXPECT_EQ(it, "Hello, World!\n");
    }

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, loop_break_continue)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("loops_bc.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'loops.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path("src/lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                     ctx.get_gc().remove_temporary(s);
                                 });

    ASSERT_NO_THROW(ctx.load_module("loops_bc", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("loops_bc", "main_b", {}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    print_buf.clear();
    ASSERT_NO_THROW(res = ctx.invoke("loops_bc", "main_c", {}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, infinite_recursion)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("inf_recursion.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'inf_recursion.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("inf_recursion", mod));

    si::value res;

    ASSERT_THROW(res = ctx.invoke("inf_recursion", "inf", {}), si::interpreter_error);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, arrays)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("arrays.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'arrays.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("arrays", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("arrays", "f", {}));
    EXPECT_EQ(*res.get<int>(), 2);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("arrays", "g", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_arrays)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_array.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_array.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "return_array", {}));

    auto v = *reinterpret_cast<si::fixed_vector<int>* const*>(res.get<void*>());
    ASSERT_EQ(v->size(), 2);
    EXPECT_EQ((*v)[0], 1);
    EXPECT_EQ((*v)[1], 2);

    ASSERT_NO_THROW(ctx.get_gc().remove_temporary(v));
    ASSERT_NO_THROW(ctx.get_gc().run());

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, pass_array)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_array.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_array.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "pass_array", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "f", {si::value{std::vector<int>{1, 2}}}));
    EXPECT_EQ(*res.get<int>(), 2);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, invalid_index)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_array.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_array.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    ASSERT_THROW(ctx.invoke("return_array", "invalid_index", {}), si::interpreter_error);
}

TEST(interpreter, return_str_array)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_array.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_array.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "str_array", {}));

    {
        auto array = *reinterpret_cast<si::fixed_vector<std::string*>* const*>(res.get<void*>());
        ASSERT_EQ(array->size(), 3);
        EXPECT_EQ(*(*array)[0], "a");
        EXPECT_EQ(*(*array)[1], "test");
        EXPECT_EQ(*(*array)[2], "123");

        ASSERT_NO_THROW(ctx.get_gc().remove_temporary(array));
        ASSERT_NO_THROW(ctx.get_gc().run());

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "ret_str", {}));
    EXPECT_EQ(*res.get<std::string>(), "123");

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "call_return", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, array_length)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("array_length.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'array_length.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("array_length", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("array_length", "len", {}));
    EXPECT_EQ(*res.get<int>(), 2);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_THROW(ctx.invoke("array_length", "len2", {}), si::interpreter_error);
}

TEST(interpreter, array_copy)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("array_copy.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'array_copy.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.register_native_function("slang", "array_copy",
                                                 [&ctx](si::operand_stack& stack)
                                                 {
                                                     rt::array_copy(ctx, stack);
                                                 }));
    ASSERT_NO_THROW(ctx.register_native_function("slang", "string_equals",
                                                 [&ctx](si::operand_stack& stack)
                                                 {
                                                     rt::string_equals(ctx, stack);
                                                 }));

    ASSERT_NO_THROW(ctx.load_module("array_copy", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("array_copy", "test_copy", {}));
    ASSERT_EQ(*res.get<int>(), 1);

    ASSERT_NO_THROW(res = ctx.invoke("array_copy", "test_copy_str", {}));
    ASSERT_EQ(*res.get<int>(), 1);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_THROW(ctx.invoke("array_copy", "test_copy_fail_none", {}), si::interpreter_error);
    ASSERT_THROW(ctx.invoke("array_copy", "test_copy_fail_type", {}), si::interpreter_error);
}

TEST(interpreter, string_operations)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("string_operations.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'string_operations.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.register_native_function("slang", "string_equals",
                                                 [&ctx](si::operand_stack& stack)
                                                 {
                                                     rt::string_equals(ctx, stack);
                                                 }));
    ASSERT_NO_THROW(ctx.register_native_function("slang", "string_concat",
                                                 [&ctx](si::operand_stack& stack)
                                                 {
                                                     rt::string_concat(ctx, stack);
                                                 }));
    ASSERT_NO_THROW(ctx.load_module("string_operations", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("string_operations", "main", {}));
    ASSERT_EQ(*res.get<int>(), 10);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, operator_new)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_array.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_array.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "new_array", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_THROW(ctx.invoke("return_array", "new_array_invalid_size", {}), si::interpreter_error);
}

TEST(interpreter, prefix_postfix)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("prefix_postfix.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'prefix_postfix.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("prefix_postfix", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_add_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 2);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_add_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_sub_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_sub_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_add_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 2.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_add_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 1.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_sub_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 0.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_sub_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 1.f);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_discard)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_discard.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_discard.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_discard", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_discard", "f", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_discard_array)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_discard_array.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_discard_array.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_discard_array", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_discard_array", "f", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_discard_strings)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("return_discard_strings.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'return_discard_strings.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_discard_strings", mod));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_discard_strings", "f", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, structs)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("structs.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'structs.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    auto& header = mod.get_header();
    ASSERT_EQ(header.exports.size(), 2);
    EXPECT_EQ(header.exports[0].type, slang::symbol_type::type);
    EXPECT_EQ(header.exports[0].name, "S");
    EXPECT_EQ(header.exports[1].type, slang::symbol_type::type);
    EXPECT_EQ(header.exports[1].name, "T");

    {
        auto& desc = std::get<slang::type_descriptor>(header.exports[0].desc);
        ASSERT_EQ(desc.member_types.size(), 2);
        EXPECT_EQ(desc.member_types[0].first, "i");
        EXPECT_EQ(desc.member_types[0].second, "i32");
        EXPECT_EQ(desc.member_types[1].first, "j");
        EXPECT_EQ(desc.member_types[1].second, "f32");
    }

    {
        auto& desc = std::get<slang::type_descriptor>(header.exports[1].desc);
        ASSERT_EQ(desc.member_types.size(), 2);
        EXPECT_EQ(desc.member_types[0].first, "s");
        EXPECT_EQ(desc.member_types[0].second, "S");
        EXPECT_EQ(desc.member_types[1].first, "t");
        EXPECT_EQ(desc.member_types[1].second, "str");
    }

    slang::file_manager file_mgr;
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("structs", mod));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

}    // namespace