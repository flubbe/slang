/**
 * slang - a simple scripting language.
 *
 * Package tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "package.h"

namespace fs = std::filesystem;

namespace
{

TEST(package, name_component_validation)
{
    const std::array<std::pair<std::string, bool>, 15> names = {
      std::make_pair("in valid", false),
      {"_valid", true},
      {"1nvalid", false},
      {"v4l1d", true},
      {"Valid", true},
      {" nvalid", false},
      {":nvalid", false},
      {"inv:lid", false},
      {"1234", false},
      {"", false},
      {".", false},
      {"1", false},
      {"::", false},
      {"1::a", false},
      {"a::b::v4l1d_", false}};

    for(const auto& [n, e]: names)
    {
        EXPECT_EQ(slang::package::is_valid_name_component(n), e);
    }
}

TEST(package, name_validation)
{
    const std::array<std::pair<std::string, bool>, 15> names = {
      std::make_pair("in valid", false),
      {"_valid", true},
      {"1nvalid", false},
      {"v4l1d", true},
      {"Valid", true},
      {" nvalid", false},
      {":nvalid", false},
      {"inv:lid", false},
      {"1234", false},
      {"", false},
      {".", false},
      {"1", false},
      {"::", false},
      {"1::a", false},
      {"a::b::v4l1d_", true}};

    for(const auto& [n, e]: names)
    {
        EXPECT_EQ(slang::package::is_valid_name(n), e);
    }
}

TEST(package_manager, construction)
{
    auto pm_no_create = slang::package_manager("pm_test");
    EXPECT_FALSE(fs::exists("pm_test"));

    // check that the directory does not exist.
    bool dir_exists = fs::exists("pm_create_test");
    EXPECT_FALSE(dir_exists);

    if(dir_exists)
    {
        fs::remove_all("pm_create_test");
        dir_exists = fs::exists("pm_create_test");
        EXPECT_FALSE(dir_exists);
    }

    if(!dir_exists)
    {
        auto pm_create = slang::package_manager("pm_create_test", true);
        EXPECT_TRUE(fs::exists("pm_create_test"));
        EXPECT_TRUE(pm_create.is_persistent());

        auto try_open = [&pm_create](bool create) -> bool
        {
            try
            {
                auto pkg = pm_create.open("std", create);
                EXPECT_EQ(pkg.is_persistent(), create);

                EXPECT_FALSE(pkg.contains_module("test"));
                EXPECT_FALSE(pkg.contains_source("test"));
            }
            catch(const std::runtime_error& e)
            {
                return false;
            }
            return true;
        };
        EXPECT_FALSE(try_open(false));
        EXPECT_TRUE(try_open(true));

        // delete the created directory.
        if(fs::exists("pm_create_test"))
        {
            fs::remove_all("pm_create_test");
        }
    }
}

}    // namespace
