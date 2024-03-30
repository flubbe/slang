/**
 * slang - a simple scripting language.
 *
 * File manager tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "filemanager.h"

#include <fmt/core.h>

#include <gtest/gtest.h>

namespace
{

TEST(filemanager, exists)
{
    slang::file_manager mgr;
    mgr.add_search_path(fs::current_path());

    EXPECT_TRUE(mgr.exists("README.md"));
    EXPECT_TRUE(mgr.is_file("README.md"));
    EXPECT_FALSE(mgr.is_directory("README.md"));
    EXPECT_NO_THROW(mgr.resolve("README.md"));

    std::unique_ptr<slang::file_archive> ar;
    EXPECT_NO_THROW(ar = mgr.open("README.md", slang::file_manager::open_mode::read));
    EXPECT_EQ(ar->get_target_byte_order(), slang::endian::little);
    EXPECT_EQ(ar->is_reading(), true);
    EXPECT_EQ(ar->is_writing(), false);
    EXPECT_EQ(ar->is_persistent(), true);
}

}    // namespace