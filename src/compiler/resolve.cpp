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
#include <queue>
#include <ranges>

#include "shared/type_utils.h"
#include "collect.h"
#include "loader.h"
#include "name_utils.h"
#include "package.h"
#include "resolve.h"
#include "utils.h"

namespace slang::resolve
{

namespace co = slang::collect;

/*
 * context.
 */

void context::import_constant(
  sema::symbol_id symbol_id,
  const module_::exported_symbol& symbol,
  const module_::module_header& header)
{
    auto symbol_info_it = sema_env.symbol_table.find(symbol_id);
    if(symbol_info_it == sema_env.symbol_table.end())
    {
        throw std::runtime_error(
          std::format(
            "No symbol info registered in symbol table for id '{}'.",
            symbol_id.value));
    }

    const auto& const_idx = std::get<std::size_t>(symbol.desc);
    if(const_idx >= header.constants.size())
    {
        throw std::runtime_error(
          std::format(
            "External symbol '{}': Constant table index out of range.",
            symbol_info_it->second.qualified_name));
    }

    const module_::constant_table_entry& const_entry = header.constants[const_idx];

    // store constant info and add constant to type map.
    const_env.register_constant(symbol_id);

    ty::type_id new_type_id = [this, &symbol_info_it, &symbol_id, &const_entry]() -> ty::type_id
    {
        if(const_entry.type == module_::constant_type::i32)
        {
            const_env.set_const_info(
              symbol_id,
              const_::const_info{
                .origin_module_id = symbol_info_it->second.declaring_module,
                .type = const_::constant_type::i32,
                .value = std::get<int>(const_entry.data)});

            return type_ctx.get_i32_type();
        }

        if(const_entry.type == module_::constant_type::f32)
        {
            const_env.set_const_info(
              symbol_id,
              const_::const_info{
                .origin_module_id = symbol_info_it->second.declaring_module,
                .type = const_::constant_type::f32,
                .value = std::get<float>(const_entry.data)});

            return type_ctx.get_f32_type();
        }

        if(const_entry.type == module_::constant_type::str)
        {
            const_env.set_const_info(
              symbol_id,
              const_::const_info{
                .origin_module_id = symbol_info_it->second.declaring_module,
                .type = const_::constant_type::str,
                .value = std::get<std::string>(const_entry.data)});

            return type_ctx.get_str_type();
        }

        throw std::runtime_error(
          std::format(
            "External symbol '{}': Unknown constant type {}.",
            symbol_info_it->second.qualified_name,
            static_cast<int>(const_entry.type)));
    }();

    auto [entry, success] = sema_env.type_map.insert(
      {symbol_id,
       new_type_id});
    if(!success)
    {
        throw std::runtime_error(
          std::format(
            "Could not insert symbol '{}' into type map.",
            symbol_info_it->second.qualified_name));
    }
}

void context::import_function(
  sema::symbol_id symbol_id,
  const module_::exported_symbol& symbol,
  const std::string& module_name,
  const module_::module_header& header)
{
    const auto& desc = std::get<module_::function_descriptor>(symbol.desc);

    auto symbol_info_it = sema_env.symbol_table.find(symbol_id);
    if(symbol_info_it == sema_env.symbol_table.end())
    {
        throw std::runtime_error(
          std::format(
            "No symbol info registered in symbol table for id '{}'.",
            symbol_id.value));
    }

    // TODO Only verify and load types?

    resolve_imported_type(
      desc.signature.return_type,
      symbol_info_it->second.loc,
      module_name,
      header);

    for(const auto& arg_type: desc.signature.arg_types)
    {
        resolve_imported_type(
          arg_type,
          symbol_info_it->second.loc,
          module_name,
          header);
    }
}

ty::type_id context::resolve_imported_type(
  const module_::variable_type& t,
  const source_location& loc,
  const std::string& module_name,
  const module_::module_header& header)
{
    std::string base_type = t.base_type();
    if(!ty::is_builtin_type(base_type))
    {
        base_type = name::qualified_name(
          module_name,
          base_type);

        resolve(
          base_type,
          sema::symbol_type::type,
          sema::scope::invalid_id    // FIXME ignored.
        );
    }

    return type_ctx.get_type(base_type);
}

void context::import_type(
  sema::symbol_id symbol_id,
  const source_location& loc,
  const module_::exported_symbol& symbol,
  const std::string& module_name,
  const module_::module_header& header)
{
    const auto& desc = std::get<module_::struct_descriptor>(symbol.desc);

    // resolve member types.
    for(const auto& m: desc.member_types)
    {
        if(m.second.base_type.get_import_index().has_value())
        {
            resolve_imported_type(
              m.second.base_type,
              loc,
              module_name,
              header);
        }
    }

    // register struct.
    const std::string canonical_struct_name = name::qualified_name(
      module_name,
      symbol.name);

    auto struct_type_id = type_ctx.declare_struct(
      symbol.name,
      canonical_struct_name);

    type_ctx.set_type_flags(
      struct_type_id,
      (desc.flags & static_cast<std::uint8_t>(module_::struct_flags::native)) != 0,
      (desc.flags & static_cast<std::uint8_t>(module_::struct_flags::allow_cast)) != 0);

    for(const auto& m: desc.member_types)
    {
        const std::string qualified_type_name = name::qualified_name(
          module_name,
          m.second.base_type.base_type());

        bool has_type = type_ctx.has_type(m.second.base_type.base_type());
        bool is_builtin = has_type
                          && type_ctx.is_builtin(
                            type_ctx.get_type(
                              m.second.base_type.base_type()));

        ty::type_id member_type_id =
          is_builtin
            ? type_ctx.get_type(m.second.base_type.base_type())
            : resolve_imported_type(
                m.second.base_type,
                loc,
                module_name,
                header);

        type_ctx.add_field(
          struct_type_id,
          m.first,
          member_type_id);
    }

    type_ctx.seal_struct(struct_type_id);
}

std::optional<sema::symbol_id> context::resolve_external(
  const std::string& module_name,
  module_id module_id,
  const sema::symbol_info& info)
{
    // check if the symbol is already resolved.
    auto sym_id = sema_env.get_symbol_id(
      info.qualified_name,
      info.type,
      sema_env.global_scope_id);
    if(sym_id.has_value())
    {
        return sym_id.value();
    }

    // import the symbol.
    auto new_symbol_id = ++sema_env.next_symbol_id.value;

    auto global_scope_it = sema_env.scope_map.find(sema_env.global_scope_id);
    if(global_scope_it == sema_env.scope_map.end())
    {
        throw std::runtime_error(
          std::format(
            "Cannot insert symbol '{}' into global scope: Scope not found.",
            info.qualified_name));
    }

    global_scope_it->second.bindings.insert(
      {info.qualified_name,
       std::unordered_map<sema::symbol_type, sema::symbol_id>{
         {info.type, new_symbol_id}}});

    auto [entry, success] = sema_env.symbol_table.insert(
      {new_symbol_id,
       sema::symbol_info{
         .name = info.qualified_name,
         .qualified_name = info.qualified_name,
         .type = info.type,
         .loc = info.loc,
         .scope = sema_env.global_scope_id,
         .declaring_module = info.declaring_module,    // FIXME This might refer to its parent within the external sema env.
         .reference = info.reference}});
    if(!success)
    {
        throw std::runtime_error(
          std::format(
            "Could not insert symbol '{}' into global namespace.",
            info.qualified_name));
    }

    if(!info.reference.has_value())
    {
        throw std::runtime_error(
          std::format(
            "External symbol '{}' has no import reference.",
            info.qualified_name));
    }
    const auto* external_reference = std::get<const module_::exported_symbol*>(
      info.reference.value());

    // FIXME The resolver should be cached.

    const loader::context* module_loader = resolved_modules[module_id];
    const module_::module_resolver& module_resolver = module_loader->get_resolver(module_name);
    const module_::module_header& module_header = module_resolver.get_module().get_header();

    if(external_reference->type == module_::symbol_type::constant)
    {
        import_constant(
          new_symbol_id,
          *external_reference,
          module_header);
    }
    else if(external_reference->type == module_::symbol_type::function)
    {
        import_function(
          new_symbol_id,
          *external_reference,
          module_name,
          module_header);
    }
    else if(external_reference->type == module_::symbol_type::macro)
    {
        // TODO
        throw std::runtime_error("resolve: macro");
    }
    else if(external_reference->type == module_::symbol_type::package)
    {
        // TODO
        throw std::runtime_error("resolve: package");
    }
    else if(external_reference->type == module_::symbol_type::type)
    {
        import_type(
          new_symbol_id,
          info.loc,
          *external_reference,
          module_name,
          module_header);
    }
    else
    {
        throw std::runtime_error(
          std::format(
            "External symbol '{}' has unhandled type: '{}'.",
            info.qualified_name,
            sema::to_string(info.type)));
    }

    return entry->first;
}

std::optional<sema::symbol_id> context::resolve(
  const std::string& name,
  sema::symbol_type type,
  sema::scope_id scope_id)
{
    if(name.rfind("::") != std::string_view::npos)
    {
        // FIXME triple work
        std::string mod_name{name::module_path(name)};
        std::string member_name{name::unqualified_name(name)};

        // Resolve module.
        auto mod_symbol_id = sema_env.get_symbol_id(
          mod_name,
          sema::symbol_type::module_,
          sema_env.global_scope_id);
        if(mod_symbol_id == std::nullopt)
        {
            return std::nullopt;
        }

        // FIXME check the module exists?
        auto mod_symbol_info = sema_env.symbol_table.find(
          mod_symbol_id.value());
        if(mod_symbol_info == sema_env.symbol_table.end())
        {
            return std::nullopt;
        }

        // Resolve module name.
        auto mod_id = module_map.find(mod_name);
        if(mod_id == module_map.end())
        {
            return std::nullopt;
        }

        auto mod_env = module_envs.find(mod_id->second);
        if(mod_env == module_envs.end())
        {
            return std::nullopt;
        }

        auto external_symbol = std::ranges::find_if(
          mod_env->second.symbol_table,
          [&member_name, type](const auto& p) -> bool
          {
              const auto& [external_symbol_id, info] = p;
              return info.type == type
                     && info.name == member_name;
          });
        if(external_symbol == mod_env->second.symbol_table.end())
        {
            return std::nullopt;
        }

        if(external_symbol->second.qualified_name != name)
        {
            throw std::runtime_error(
              std::format(
                "Qualified name mismatch during name resolution: Expected '{}', external symbol resolved to '{}'.",
                name,
                external_symbol->second.qualified_name));
        }

        // insert new declaration.
        auto s_it = sema_env.scope_map.find(sema_env.global_scope_id);
        if(s_it == sema_env.scope_map.end())
        {
            throw std::runtime_error(
              std::format(
                "Cannot insert symbol '{}' into global scope: Scope not found.",
                external_symbol->second.qualified_name));
        }

        auto it = std::ranges::find_if(
          s_it->second.bindings,
          [&name, &type](const auto& b) -> bool
          {
              if(b.first != name)
              {
                  return false;
              }

              return b.second.contains(type);
          });
        if(it != s_it->second.bindings.end())
        {
            // already bound.
            return it->second[type];
        }

        return resolve_external(
          mod_name,
          mod_id->second,
          external_symbol->second);
    }

    return sema_env.get_symbol_id(
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
    case module_::symbol_type::type: return sema::symbol_type::type;
    default:
        throw std::runtime_error(
          std::format(
            "Unable to get semantic symbol type from module symbol type '{}'.",
            static_cast<int>(s)));
    }
}

static std::vector<std::string> topological_order(
  const std::unordered_map<
    std::string,
    std::set<std::string>>& module_graph)
{
    std::unordered_map<
      std::string,
      std::vector<std::string>>
      rev;
    for(const auto& [m, deps]: module_graph)
    {
        for(const auto& d: deps)
        {
            rev[d].push_back(m);
        }
    }
    for(auto& [k, v]: rev)
    {
        std::ranges::sort(v);
    }

    std::unordered_map<std::string, std::size_t> indeg;
    for(auto& [m, deps]: module_graph)
    {
        indeg[m] = deps.size();
    }

    std::priority_queue<
      std::string,
      std::vector<std::string>,
      std::greater<>>
      q;
    for(const auto& [m, d]: indeg)
    {
        if(d == 0)
        {
            q.push(m);
        }
    }

    std::vector<std::string> order;
    order.reserve(module_graph.size());
    while(!q.empty())
    {
        auto u = q.top();
        q.pop();
        order.push_back(u);
        for(const auto& v: rev[u])
        {
            if(--indeg[v] == 0)
            {
                q.push(v);
            }
        }
    }

    if(order.size() != module_graph.size())
    {
        throw std::runtime_error("Cycle detected in module graph");
    }

    return order;
}

void context::resolve_imports(
  ld::context& loader)
{
    sema::env module_import_env;
    co::context module_import_collector{module_import_env};

    module_import_env.next_symbol_id = sema_env.next_symbol_id;

    if(module_import_collector.push_scope("<global>", {0, 0})
       != co::context::global_scope_id)
    {
        throw std::runtime_error("Got unexpected scope id for global scope.");
    }

    std::unordered_map<
      sema::symbol_id,
      module_::constant_table_entry,
      sema::symbol_id::hash>
      imported_constants;    // symbol id -> constant table entry.

    // build list of modules.
    std::unordered_map<
      std::string,
      sema::symbol_id>
      unresolved_modules;    // fully-qualified module names -> declaring_module_id
    std::unordered_map<
      std::string,
      source_location>
      module_locations;
    std::unordered_map<
      std::string,
      sema::symbol_id>
      module_ids;

    for(auto& [module_id, info]: sema_env.symbol_table
                                   | std::views::filter([](const auto& p) -> bool
                                                        { return p.second.type == sema::symbol_type::module_; }))
    {
        if(module_map.contains(info.qualified_name))
        {
            continue;
        }

        auto [it, success] = unresolved_modules.insert(
          {info.qualified_name,
           info.declaring_module});
        if(!success && info.declaring_module == sema::symbol_id::invalid)
        {
            it->second = sema::symbol_info::current_module_id;
        }

        module_locations.insert(
          {info.qualified_name,
           info.loc});
    }

    // modules awaiting load.
    std::unordered_map<
      std::string,
      std::set<std::string>>
      module_graph;
    std::unordered_map<
      std::string,
      std::pair<module_::module_resolver*, sema::symbol_id>>
      pending_modules;
    std::unordered_map<std::string, sema::symbol_id> importing_modules;

    while(!unresolved_modules.empty())
    {
        auto nh = unresolved_modules.extract(unresolved_modules.begin());
        const std::string& qualified_module_name = nh.key();
        sema::symbol_id importing_module = nh.mapped();
        bool transitive = importing_module.value != sema::symbol_info::current_module_id;

        module_::module_resolver* resolver = &loader.resolve_module(
          qualified_module_name,
          transitive);

        auto loc_it = module_locations.find(qualified_module_name);
        if(loc_it == module_locations.end())
        {
            throw std::runtime_error(
              std::format(
                "No source location for imported module {}.",
                qualified_module_name));
        }
        const source_location& loc = loc_it->second;

        if(module_ids.contains(qualified_module_name))
        {
            throw std::runtime_error(
              std::format(
                "Module '{}' already loaded.",
                qualified_module_name));
        }

        auto unqualified_name = name::unqualified_name(qualified_module_name);

        sema::symbol_id module_id = sema::symbol_id::invalid;
        if(!transitive)
        {
            // we should already have a module id
            auto opt_id = sema_env.get_symbol_id(
              qualified_module_name,
              sema::symbol_type::module_,
              sema_env.global_scope_id);

            if(!opt_id.has_value())
            {
                throw std::runtime_error(
                  std::format(
                    "No symbol id for non-transitive module '{}'.",
                    qualified_module_name));
            }

            module_id = opt_id.value();
        }
        else
        {
            module_id = module_import_collector.declare(
              unqualified_name,
              qualified_module_name,
              sema::symbol_type::module_,
              loc,
              importing_module,
              transitive);
        }

        module_ids[qualified_module_name] = module_id;
        module_graph[qualified_module_name] = {};
        pending_modules[qualified_module_name] = {
          resolver,
          module_id};

        auto& module_graph_entry = module_graph[qualified_module_name];

        for(const auto& it: resolver->get_module().get_header().imports)
        {
            if(it.type != module_::symbol_type::package)
            {
                continue;
            }

            std::string qualified_name = loader.resolve_name(it.name);
            module_graph_entry.insert(qualified_name);

            if(module_graph.contains(qualified_name))
            {
                continue;
            }

            if(unresolved_modules.contains(qualified_name))
            {
                continue;
            }

            unresolved_modules.insert(
              std::make_pair(
                qualified_name,
                module_id));

            module_locations.insert(
              {qualified_name,
               loc});
        }
    }

    sema_env.next_symbol_id = module_import_env.next_symbol_id;
    if(module_import_env.next_scope_id != co::context::global_scope_id + 1)
    {
        throw std::runtime_error(
          "Inconsistent semantic environment after import: Non-global scopes were created.");
    }

    std::vector<std::string> module_load_order = topological_order(module_graph);

    for(const auto& qualified_module_name: module_load_order)
    {
        const auto& [resolver, module_id] = pending_modules.at(qualified_module_name);

        sema::env import_env;
        co::context import_collector{import_env};

        // set up global scope.
        if(import_collector.push_scope("<global>", {0, 0}) != co::context::global_scope_id)
        {
            throw std::runtime_error("Got unexpected scope id for global scope.");
        }

        const auto& header = resolver->get_module().get_header();
        bool transitive = module_id != sema::symbol_info::current_module_id;

        std::vector<
          std::pair<
            ty::type_id,
            const module_::struct_descriptor*>>
          type_descriptors;

        for(const auto& it: header.exports)
        {
            if(it.type == module_::symbol_type::package)
            {
                // already resolved.
                continue;
            }

            auto loc_it = module_locations.find(qualified_module_name);
            if(loc_it == module_locations.end())
            {
                throw std::runtime_error(
                  std::format(
                    "No source location for imported module {}.",
                    qualified_module_name));
            }
            const source_location& loc = loc_it->second;

            // import symbols.
            auto qualified_name = name::qualified_name(
              qualified_module_name,
              it.name);

            auto symbol_id = import_collector.declare(
              it.name,
              qualified_name,
              to_sema_symbol_type(it.type),
              loc,
              module_id,
              transitive,
              &it);
        }

        auto id = generate_module_id();
        module_map.insert({qualified_module_name, id});
        resolved_modules.insert({id, &loader});
        module_envs.insert({id, std::move(import_env)});
    }
}

}    // namespace slang::resolve
