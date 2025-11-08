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
#include "macro.h"
#include "typing.h"
#include "package.h"
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

bool context::resolve_macros(
  co::context& co_ctx,
  macro::env& env)
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
          [&co_ctx, &needs_import_resolution](ast::expression& e) -> void
          {
              if(e.get_id() != ast::node_identifier::namespace_access_expression)
              {
                  return;
              }

              if(e.is_macro_invocation()
                 || e.is_call_expression())
              {
                  e.update_namespace();

                  co_ctx.push_scope(co::context::global_scope_id);

                  if(co_ctx.declare_external(
                       e.get_namespace_path().value(),
                       sema::symbol_type::module_,
                       e.get_location()))
                  {
                      needs_import_resolution = true;
                  }

                  co_ctx.pop_scope();
              }
          },
          false,
          false);
    }

    return needs_import_resolution;
}

}    // namespace slang::loader
