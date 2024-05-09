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
#include "interpreter.h"

namespace
{

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
    slang::interpreter::context ctx{file_mgr};
    ASSERT_NO_THROW(ctx.load_module("test_output", mod));

    slang::interpreter::value res;
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

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg", {1}));
    EXPECT_EQ(*res.get<int>(), 2);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg", {15}));
    EXPECT_EQ(*res.get<int>(), 16);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg", {-100}));
    EXPECT_EQ(*res.get<int>(), -99);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg2", {1.0f}));
    EXPECT_NEAR(*res.get<float>(), 3.0, 1e-6);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg2", {-1.0f}));
    EXPECT_NEAR(*res.get<float>(), -1.0, 1e-6);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "arg2", {0.f}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "sid", {"Test"}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "call", {0}));
    EXPECT_EQ(*res.get<int>(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "local", {0}));
    EXPECT_EQ(*res.get<int>(), -1);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "local2", {0}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "local3", {}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    ASSERT_NO_THROW(res = ctx.invoke("test_output", "cast_i2f", {23}));
    EXPECT_EQ(*res.get<float>(), 23.0);
    ASSERT_NO_THROW(res = ctx.invoke("test_output", "cast_f2i", {92.3f}));
    EXPECT_EQ(*res.get<int>(), 92);
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
    slang::interpreter::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                 });

    ASSERT_NO_THROW(ctx.load_module("hello_world", mod));
    EXPECT_NO_THROW(ctx.invoke("hello_world", "main", {"Test"}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Hello, World!\n");

    // re-defining functions should fail.
    EXPECT_THROW(ctx.register_native_function("slang", "println",    // collides with a native function name
                                              [](slang::interpreter::operand_stack& stack) {}),
                 slang::interpreter::interpreter_error);
    EXPECT_THROW(ctx.register_native_function("hello_world", "main",    // collides with a scripted function name
                                              [](slang::interpreter::operand_stack& stack) {}),
                 slang::interpreter::interpreter_error);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("operators", mod));
    ASSERT_NO_THROW(ctx.invoke("operators", "main", {}));

    slang::interpreter::value res;
    ASSERT_NO_THROW(res = ctx.invoke("operators", "and", {27, 3}));
    EXPECT_EQ(*res.get<int>(), 3);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "or", {27, 4}));
    EXPECT_EQ(*res.get<int>(), 31);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "xor", {27, 3}));
    EXPECT_EQ(*res.get<int>(), 24);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "shl", {27, 3}));
    EXPECT_EQ(*res.get<int>(), 216);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "shr", {27, 3}));
    EXPECT_EQ(*res.get<int>(), 3);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "mod", {127, 23}));
    EXPECT_EQ(*res.get<int>(), 12);
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
    slang::interpreter::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                 });

    ASSERT_NO_THROW(ctx.load_module("control_flow", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("control_flow", "test_if_else", {2}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("control_flow", "test_if_else", {-1}));
    EXPECT_EQ(*res.get<int>(), 0);

    ASSERT_NO_THROW(ctx.invoke("control_flow", "conditional_hello_world", {3.f}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    ASSERT_NO_THROW(ctx.invoke("control_flow", "conditional_hello_world", {2.2f}));
    ASSERT_EQ(print_buf.size(), 2);
    EXPECT_EQ(print_buf.back(), "World, hello!\n");

    print_buf.clear();
    ASSERT_NO_THROW(ctx.invoke("control_flow", "no_else", {1}));
    ASSERT_EQ(print_buf.size(), 2);
    EXPECT_EQ(print_buf[0], "a>0\n");
    EXPECT_EQ(print_buf[1], "Test\n");

    print_buf.clear();
    ASSERT_NO_THROW(ctx.invoke("control_flow", "no_else", {-2}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Test\n");
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
    slang::interpreter::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                 });

    ASSERT_NO_THROW(ctx.load_module("loops", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("loops", "main", {}));
    EXPECT_EQ(print_buf.size(), 10);
    for(auto& it: print_buf)
    {
        EXPECT_EQ(it, "Hello, World!\n");
    }
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
    slang::interpreter::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                 });

    ASSERT_NO_THROW(ctx.load_module("loops_bc", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("loops_bc", "main_b", {}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    print_buf.clear();
    ASSERT_NO_THROW(res = ctx.invoke("loops_bc", "main_c", {}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("inf_recursion", mod));

    slang::interpreter::value res;

    ASSERT_THROW(res = ctx.invoke("inf_recursion", "inf", {}), slang::interpreter::interpreter_error);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("arrays", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("arrays", "f", {}));
    EXPECT_EQ(*res.get<int>(), 2);

    ASSERT_NO_THROW(res = ctx.invoke("arrays", "g", {}));
    EXPECT_EQ(*res.get<int>(), 3);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "return_array", {}));
    ASSERT_EQ(res.get<std::vector<int>>()->size(), 2);
    EXPECT_EQ((*res.get<std::vector<int>>())[0], 1);
    EXPECT_EQ((*res.get<std::vector<int>>())[1], 2);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "pass_array", {}));
    EXPECT_EQ(*res.get<int>(), 3);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    ASSERT_THROW(ctx.invoke("return_array", "invalid_index", {}), slang::interpreter::interpreter_error);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("return_array", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "str_array", {}));
    ASSERT_EQ(res.get<std::vector<std::string>>()->size(), 3);
    EXPECT_EQ((*res.get<std::vector<std::string>>())[0], "a");
    EXPECT_EQ((*res.get<std::vector<std::string>>())[1], "test");
    EXPECT_EQ((*res.get<std::vector<std::string>>())[2], "123");

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "ret_str", {}));
    EXPECT_EQ(*res.get<std::string>(), "123");

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "call_return", {}));
    EXPECT_EQ(*res.get<int>(), 1);
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
    slang::interpreter::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.load_module("prefix_postfix", mod));

    slang::interpreter::value res;

    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_add_i32", {1}));
    ASSERT_EQ(*res.get<int>(), 2);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_add_i32", {1}));
    ASSERT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_sub_i32", {1}));
    ASSERT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_sub_i32", {1}));
    ASSERT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_add_f32", {1.f}));
    ASSERT_EQ(*res.get<float>(), 2.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_add_f32", {1.f}));
    ASSERT_EQ(*res.get<float>(), 1.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_sub_f32", {1.f}));
    ASSERT_EQ(*res.get<float>(), 0.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_sub_f32", {1.f}));
    ASSERT_EQ(*res.get<float>(), 1.f);
}

}    // namespace