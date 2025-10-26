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

#include "compiler/ast/ast.h"
#include "compiler/ast/node_registry.h"
#include "compiler/codegen/codegen.h"
#include "compiler/macro.h"
#include "compiler/typing.h"
#include "shared/type_utils.h"
#include "package.h"
#include "loader.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
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

bool context::resolve_macros(macro::env& env, ty::context& type_ctx)
{
    bool needs_import_resolution = false;

    for(auto& m: env.macros)
    {
        const auto& desc = m->get_desc();
        if(!desc.serialized_ast.has_value())
        {
            throw resolve_error(
              std::format(
                "Macro '{}' has empty AST.",
                m->get_name()));
        }

        memory_read_archive ar{
          desc.serialized_ast.value(),
          true,
          std::endian::little};

        std::unique_ptr<ast::expression> macro_ast;
        ar& ast::expression_serializer{macro_ast};

        // update namespace information.
        macro_ast->visit_nodes(
          [](ast::expression& e) -> void
          {
              if(e.get_id() != ast::node_identifier::namespace_access_expression)
              {
                  return;
              }

              if(!e.is_macro_invocation())
              {
                  return;
              }

              // this updates the namespace information.
              (void)e.as_macro_invocation();

              // TODO Function calls need to be resolved.
          },
          false,
          false);

        // Add import from macro AST to type context.
        macro_ast->visit_nodes(
          [&type_ctx, &needs_import_resolution](const ast::expression& e) -> void
          {
              if(e.is_macro_invocation()
                 && e.get_namespace_path().has_value())
              {
                  // TODO type context
                  throw std::runtime_error("resolve_macros");
              }

              // TODO Function calls need to be resolved.
          },
          true,
          false);
    }

    return needs_import_resolution;
}

}    // namespace slang::loader
