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

#include "collect.h"
#include "loader.h"
#include "package.h"
#include "resolve.h"
#include "utils.h"

#include <print>

namespace slang::resolve
{

namespace co = slang::collect;

/*
 * context.
 */

std::optional<sema::symbol_id> context::resolve(
  const std::string& name,
  sema::symbol_type type,
  sema::scope_id scope_id)
{
    return env.get_symbol_id(
      name,
      type,
      scope_id);
}

/**
 * Map module symbols to semantic symbol types.
 *
 * @param s Symbol type from the module header.
 * @returns Returns the symbol mapped to `sema::symbol_type`.
 * @throws Throws a `std::runtime_error` if the symbol type could not be mapped.
 */
sema::symbol_type to_sema_symbol_type(module_::symbol_type s)
{
    switch(s)
    {
    case module_::symbol_type::constant: return sema::symbol_type::constant_declaration;
    case module_::symbol_type::function: return sema::symbol_type::function_definition;
    case module_::symbol_type::macro: return sema::symbol_type::macro_definition;
    case module_::symbol_type::type: return sema::symbol_type::struct_definition;
    default:
        throw std::runtime_error(
          std::format(
            "Unable to get semantic symbol type from module symbol type '{}'.",
            static_cast<int>(s)));
    }
}

void context::resolve_imports(
  ld::context& loader)
{
    // import environment, to be merged with the context's environment.
    sema::env import_env;
    co::context import_collector{import_env};

    // set up global scope.
    if(import_collector.push_scope("<global>", {0, 0}) != co::context::global_scope_id)
    {
        throw std::runtime_error("Got unexpected scope id for global scope.");
    }

    // set up next ids.
    import_env.next_scope_id = env.next_scope_id;
    import_env.next_symbol_id = env.next_symbol_id;

    for(auto& [id, info]: env.symbol_table)
    {
        if(info.type != sema::symbol_type::module_import)
        {
            continue;
        }

        bool transitive_import = env.transitive_imports.contains(id);

        module_::module_resolver& resolver = loader.resolve_module(
          info.qualified_name, transitive_import);

        const module_::module_header& header = resolver.get_module().get_header();

        for(const auto& it: header.exports)
        {
            auto qualified_name =
              std::format(
                "{}::{}",
                info.qualified_name,
                it.name);

            import_collector.declare(
              it.name,
              qualified_name,
              to_sema_symbol_type(it.type),
              info.loc,
              id,
              transitive_import,
              &it);
        }
    }

    // Move symbols into semantic environment.
    for(auto& [id, info]: import_env.symbol_table)
    {
        auto symbol_it = std::ranges::find_if(
          env.symbol_table,
          [&info](const std::pair<sema::symbol_id, sema::symbol_info>& p) -> bool
          {
              return p.second.name == info.name;
          });
        if(symbol_it != env.symbol_table.end())
        {
            if(symbol_it->second.qualified_name != info.qualified_name)
            {
                throw std::runtime_error(
                  std::format(
                    "{}: '{}': A symbol with the same name already exists in symbol table. Declaration at: {}",
                    to_string(info.loc),
                    info.qualified_name,
                    to_string(symbol_it->second.loc)));
            }

            if(env.transitive_imports.contains(symbol_it->first)
               && !import_env.transitive_imports.contains(id))
            {
                env.transitive_imports.erase(symbol_it->first);
            }

            continue;
        }

        env.symbol_table.insert({id, info});

        auto scope_it = env.scope_map.find(env.global_scope_id);
        if(scope_it == env.scope_map.end())
        {
            throw std::runtime_error(
              std::format(
                "No global scope (id '{}') in semantic environment.",
                static_cast<int>(env.global_scope_id)));
        }
        auto binding_it = scope_it->second.bindings.find(info.name);
        if(binding_it != scope_it->second.bindings.end())
        {
            if(binding_it->second.contains(info.type))
            {
                throw std::runtime_error(
                  std::format(
                    "Symbol '{}' of type '{}' already bound.",
                    info.name,
                    sema::to_string(info.type)));
            }

            binding_it->second.insert({info.type, id});
        }
        else
        {
            scope_it->second.bindings.insert(
              {info.name,
               {{info.type, id}}});
        }

        if(import_env.transitive_imports.contains(id))
        {
            env.transitive_imports.insert(id);
        }
    }

    env.next_scope_id = import_env.next_scope_id;
    env.next_symbol_id = import_env.next_symbol_id;
}

}    // namespace slang::resolve
