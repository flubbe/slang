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

TEST(interpreter, loading)
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

    slang::interpreter::context ctx;
    EXPECT_NO_THROW(ctx.load_module("test_output", mod));

    // slang::interpreter::value res;
    // EXPECT_NO_THROW(res = ctx.invoke("test_output", "test"));
    // EXPECT_EQ(std::get<int>(res.result), 1);
}

}    // namespace