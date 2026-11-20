/**
 * slang - a simple scripting language.
 *
 * Constant diagnostics integration tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "compiler/constant.h"
#include "compiler/diagnostics.h"

namespace
{

namespace const_ = slang::const_;
namespace diag = slang::compiler::diagnostics;

TEST(constant_diagnostics, duplicate_constant_info)
{
    diag::context diag_ctx;
    const_::env const_env{diag_ctx};

    const_::const_info info1{
      .origin_module_id = slang::sema::symbol_id::invalid,
      .type = const_::constant_type::i32,
      .value = static_cast<std::int64_t>(42)};

    const_::const_info info2{
      .origin_module_id = slang::sema::symbol_id::invalid,
      .type = const_::constant_type::i32,
      .value = static_cast<std::int64_t>(99)};    // different value

    slang::sema::symbol_id symbol_id{123};

    // First set should succeed
    const_env.set_const_info(symbol_id, info1);
    EXPECT_FALSE(diag_ctx.has_errors());

    // Second set with different value should report error
    const_env.set_const_info(symbol_id, info2);
    EXPECT_TRUE(diag_ctx.has_errors());
    EXPECT_EQ(diag_ctx.get_error_count(), 1);

    std::string output = diag_ctx.format_all();
    EXPECT_TRUE(output.find("already exists") != std::string::npos);
    EXPECT_TRUE(output.find("123") != std::string::npos);
}

TEST(constant_diagnostics, duplicate_constant_registration)
{
    diag::context diag_ctx;
    const_::env const_env{diag_ctx};

    slang::sema::symbol_id symbol_id{456};

    // First registration should succeed
    const_env.register_constant(symbol_id);
    EXPECT_FALSE(diag_ctx.has_errors());

    // Second registration should report error
    const_env.register_constant(symbol_id);
    EXPECT_TRUE(diag_ctx.has_errors());
    EXPECT_EQ(diag_ctx.get_error_count(), 1);

    std::string output = diag_ctx.format_all();
    EXPECT_TRUE(output.find("already registered") != std::string::npos);
    EXPECT_TRUE(output.find("456") != std::string::npos);
}

}    // namespace
