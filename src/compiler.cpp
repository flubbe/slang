/**
 * slang - a simple scripting language.
 *
 * compiler implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler.h"

namespace slang
{

void compiler::clear()
{
    packages.clear();
}

void compiler::setup(const std::vector<package>& pkgs)
{
    packages.insert(packages.end(), pkgs.cbegin(), pkgs.cend());

    // TODO
}

void compiler::invoke_lexer()
{
    throw dependency_error("compiler::invoke_lexer not implemented.");

    // TODO
}

void compiler::collect_definitions()
{
    throw dependency_error("compiler::collect_definitions not implemented.");

    // TODO
}

std::vector<package> compiler::resolve_dependencies()
{
    throw dependency_error("compiler::resolve_dependencies not implemented.");

    // TODO
    return {};
}

void compiler::compile_package(package& pkg)
{
    throw compiler_error("compiler::compile_package not implemented.");

    // TODO
}

}    // namespace slang