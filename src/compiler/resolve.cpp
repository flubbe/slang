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
    case module_::symbol_type::constant: return sema::symbol_type::constant;
    case module_::symbol_type::function: return sema::symbol_type::function;
    case module_::symbol_type::macro: return sema::symbol_type::macro;
    case module_::symbol_type::type: return sema::symbol_type::struct_;
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
        if(info.type != sema::symbol_type::module_)
        {
            continue;
        }

        if(resolved_modules.contains(info.qualified_name))
        {
            continue;
        }

        bool transitive_import = env.transitive_imports.contains(id);

        module_::module_resolver& resolver = loader.resolve_module(
          info.qualified_name, transitive_import);

        resolved_modules[info.qualified_name] = &loader;

        // TODO load dependencies ?

        const module_::module_header& header = resolver.get_module().get_header();

        std::vector<
          std::pair<
            ty::type_id,
            const module_::struct_descriptor*>>
          type_descriptors;

        for(const auto& it: header.exports)
        {
            auto qualified_name =
              std::format(
                "{}::{}",
                info.qualified_name,
                it.name);

            // import symbols.
            import_collector.declare(
              it.name,
              qualified_name,
              to_sema_symbol_type(it.type),
              info.loc,
              id,
              transitive_import,
              &it);

            // import type declaration.
            if(it.type == module_::symbol_type::type)
            {
                const auto& desc = std::get<module_::struct_descriptor>(it.desc);

                auto struct_type_id = type_ctx.declare_struct(
                  it.name,
                  qualified_name);

                type_descriptors.emplace_back(
                  struct_type_id, &desc);
            }
        }

        for(const auto& [struct_type_id, desc]: type_descriptors)
        {
            for(const auto& [field_name, field_desc]: desc->member_types)
            {
                ty::type_id field_type_id;

                const auto& base_type = field_desc.base_type.base_type();
                if(type_ctx.has_type(base_type)
                   && type_ctx.is_builtin(type_ctx.get_type(base_type)))
                {
                    field_type_id = type_ctx.get_type(base_type);
                }
                else
                {
                    auto qualified_field_type = std::format(
                      "{}::{}",
                      info.qualified_name,
                      base_type);

                    field_type_id = type_ctx.get_type(qualified_field_type);
                }

                std::optional<std::size_t> array_rank = field_desc.base_type.get_array_dims();
                if(array_rank.has_value())
                {
                    field_type_id = type_ctx.get_array(
                      field_type_id,
                      array_rank.value());
                }

                type_ctx.add_field(
                  struct_type_id,
                  field_name,
                  field_type_id);
            }

            type_ctx.seal_struct(struct_type_id);
        }
    }

    // Move symbols into semantic environment.
    for(auto& [id, info]: import_env.symbol_table)
    {
        auto symbol_it = std::ranges::find_if(
          env.symbol_table,
          [&info](const std::pair<sema::symbol_id, sema::symbol_info>& p) -> bool
          {
              return p.second.type == info.type
                     && p.second.qualified_name == info.qualified_name;
          });
        if(symbol_it != env.symbol_table.end())
        {
            std::println(
              "{}: Warning: Import '{}' ('{}'): A symbol with the same name already exists in symbol table. Declaration at: {}",
              to_string(info.loc),
              info.name,
              info.qualified_name,
              to_string(symbol_it->second.loc));

            continue;
        }

        env.symbol_table.insert({id, info});

        // TODO Revisit this.

        // auto scope_it = env.scope_map.find(env.global_scope_id);
        // if(scope_it == env.scope_map.end())
        // {
        //     throw std::runtime_error(
        //       std::format(
        //         "No global scope (id '{}') in semantic environment.",
        //         static_cast<int>(env.global_scope_id)));
        // }
        // auto binding_it = scope_it->second.bindings.find(info.name);
        // if(binding_it != scope_it->second.bindings.end())
        // {
        //     if(binding_it->second.contains(info.type))
        //     {
        //         throw std::runtime_error(
        //           std::format(
        //             "Symbol '{}' of type '{}' already bound.",
        //             info.name,
        //             sema::to_string(info.type)));
        //     }

        //     binding_it->second.insert({info.type, id});
        // }
        // else
        // {
        //     scope_it->second.bindings.insert(
        //       {info.name,
        //        {{info.type, id}}});
        // }

        if(import_env.transitive_imports.contains(id))
        {
            env.transitive_imports.insert(id);
        }
    }

    env.next_scope_id = import_env.next_scope_id;
    env.next_symbol_id = import_env.next_symbol_id;
}

}    // namespace slang::resolve
