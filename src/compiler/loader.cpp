/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <ranges>

#include "ast/ast.h"
#include "ast/node_registry.h"
#include "codegen/codegen.h"
#include "shared/type_utils.h"
#include "loader.h"
#include "package.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace si = slang::interpreter;

namespace slang::loader
{

/*
 * Exceptions.
 */

resolve_error::resolve_error(const source_location& loc, const std::string& message)
: std::runtime_error{std::format("{}: {}", to_string(loc), message)}
{
}

/*
 * resolver context.
 */

module_::module_resolver& context::resolve_module(
  const std::string& import_name,
  bool transitive)
{
    // module is already resolved.
    auto it = resolvers.find(import_name);
    if(it != resolvers.end())
    {
        if(it->second->is_transitive() && !transitive)
        {
            it->second->make_explicit();
        }

        return *it->second;
    }

    // load the module.
    std::string import_path = import_name;
    slang::utils::replace_all(import_path, package::delimiter, "/");

    fs::path fs_path = fs::path{import_path};
    if(!fs_path.has_extension())
    {
        fs_path = fs_path.replace_extension(package::module_ext);
    }
    fs::path resolved_path = file_mgr.resolve(fs_path);

    resolvers.insert(
      {import_name,
       std::make_unique<module_::module_resolver>(
         file_mgr,
         resolved_path,
         transitive)});
    return *resolvers[import_name].get();
}

const module_::module_resolver& context::get_resolver(
  const std::string& import_name) const
{
    // module is already resolved.
    auto it = resolvers.find(import_name);
    if(it == resolvers.end())
    {
        throw resolve_error(
          std::format(
            "Cannot resolve module: '{}' not loaded.",
            import_name));
    }

    return *it->second;
}

std::string context::resolve_name(const std::string& name) const
{
    std::string import_path = name;
    slang::utils::replace_all(import_path, package::delimiter, "/");

    fs::path fs_path = fs::path{import_path};
    if(!fs_path.has_extension())
    {
        fs_path = fs_path.replace_extension(package::module_ext);
    }

    file_mgr.resolve(fs_path);

    return name;
}

}    // namespace slang::loader
