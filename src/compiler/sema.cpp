/**
 * slang - a simple scripting language.
 *
 * shared semantic environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <utility>

#include "sema.h"

namespace slang::sema
{

/*
 * symbol_type.
 */

std::string to_string(symbol_type t)
{
    if(t == symbol_type::module_)
    {
        return "module_";
    }

    if(t == symbol_type::constant)
    {
        return "constant";
    }

    if(t == symbol_type::function)
    {
        return "function";
    }

    if(t == symbol_type::type)
    {
        return "struct";
    }

    if(t == symbol_type::variable)
    {
        return "variable";
    }

    if(t == symbol_type::macro)
    {
        return "macro";
    }

    if(t == symbol_type::macro_argument)
    {
        return "macro_argument";
    }

    throw std::runtime_error(
      std::format(
        "Unknown symbol type '{}'.",
        std::to_underlying(t)));
}

/*
 * symbol_id.
 */

archive& operator&(archive& ar, symbol_id& id)
{
    ar & id.value;
    return ar;
}

/*
 * env.
 */

std::optional<symbol_id> env::get_symbol_id(
  const std::string& name,
  symbol_type type,
  scope_id id) const
{
    bool is_qualified_name = name.find("::") != std::string::npos;
    if(is_qualified_name)
    {
        // search the symbol table for qualified names.
        auto it = std::ranges::find_if(
          symbol_table,
          [&name](const std::pair<symbol_id, symbol_info>& p) -> bool
          {
              return p.second.qualified_name == name;
          });
        if(it == symbol_table.end())
        {
            return std::nullopt;
        }
        return it->first;
    }

    // search scope bindings.
    while(id != scope::invalid_id)
    {
        auto scope_it = scope_map.find(id);
        if(scope_it == scope_map.end())
        {
            throw std::runtime_error(
              std::format(
                "Cannot find scope for id '{}'.",
                id));
        }

        auto it = scope_it->second.bindings.find(name);
        if(it != scope_it->second.bindings.end())
        {
            auto symbol_it = it->second.find(type);
            if(symbol_it != it->second.end())
            {
                return symbol_it->second;
            }
        }

        id = scope_it->second.parent;
    }

    return std::nullopt;
}

std::optional<symbol_id> env::get_symbol_id(
  const std::string& name,
  symbol_type type) const
{
    auto it = std::ranges::find_if(
      symbol_table,
      [&name, type](const std::pair<symbol_id, symbol_info>& p) -> bool
      {
          return p.second.type == type
                 && p.second.qualified_name == name;
      });
    if(it == symbol_table.end())
    {
        return std::nullopt;
    }

    return it->first;
}

void env::attach_attribute(
  symbol_id id,
  attribute_info attrib)
{
    attribute_map[id].emplace_back(std::move(attrib));
}

bool env::has_attribute(
  symbol_id id,
  attribute_kind kind) const
{
    auto it = attribute_map.find(id);
    if(it == attribute_map.end())
    {
        return false;
    }

    return std::ranges::find_if(
             it->second,
             [kind](const attribute_info& attrib) -> bool
             {
                 return attrib.kind == kind;
             })
           != it->second.end();
}

std::optional<attribute_info::payload_type> env::get_attribute_payload(
  symbol_id id,
  attribute_kind kind) const
{
    auto it = attribute_map.find(id);
    if(it == attribute_map.end())
    {
        return std::nullopt;
    }

    auto attrib_it = std::ranges::find_if(
      it->second,
      [kind](const attribute_info& attrib) -> bool
      {
          return attrib.kind == kind;
      });
    if(attrib_it == it->second.end())
    {
        return std::nullopt;
    }

    return attrib_it->payload;
}

std::string to_string(const env& env)
{
    std::string res = std::format(
      "--- Semantic Environment ---\n"
      "    Global scope id: {}\n",
      env.global_scope_id);

    if(!env.scope_map.empty())
    {
        res +=
          "\n"
          "    Scope Map\n"
          "    Scope id    Parent id    Name\n"
          "    ----------------------------------\n";

        for(const auto& [id, s]: env.scope_map)
        {
            auto name = s.name;
            if(s.parent == scope::invalid_id)
            {
                name += " [global]";
            }

            res += std::format(
              "    {:>8}    {:>9}    {}\n",
              id,
              static_cast<std::int64_t>(s.parent),
              name);
        }

        for(const auto& [scope_id, s]: env.scope_map)
        {
            res += std::format(
              "\n"
              "    Bindings for scope {}\n"
              "    Symbol id                    Type    Name\n"
              "    ---------------------------------------------\n",
              scope_id);

            for(const auto& [name, type_symbol_map]: s.bindings)
            {
                for(const auto& [symbol_type, symbol_id]: type_symbol_map)
                {
                    res += std::format(
                      "    {:>9}    {:>20}    {}\n",
                      symbol_id.value,
                      to_string(symbol_type),
                      name);
                }
            }
        }
    }

    if(!env.symbol_table.empty())
    {
        res +=
          "\n"
          "    Symbol Table\n"
          "    Symbol id                    Type    Scope id    Decl. Mod.    Location    Name\n"
          "    -----------------------------------------------------------------------------------\n";

        for(const auto& [id, info]: env.symbol_table)
        {
            res += std::format(
              "    {:>9}    {:>20}    {:>8}    {:>10}    {:>8}    {} ({})\n",
              id.value,
              to_string(info.type),
              info.scope,
              static_cast<std::int64_t>(info.declaring_module.value),
              to_string(info.loc),
              info.name,
              info.qualified_name);
        }
    }

    if(!env.transitive_imports.empty())
    {
        res +=
          "\n"
          "    Transitive Imports\n"
          "    Symbol id\n"
          "    -------------\n";

        for(const auto& it: env.transitive_imports)
        {
            res += std::format(
              "    {:>9}\n",
              it.value);
        }
    }

    if(!env.type_map.empty())
    {
        res +=
          "\n"
          "    Type map\n"
          "    Symbol id    Type id\n"
          "    ------------------------\n";

        for(const auto& [symbol_id, type_id]: env.type_map)
        {
            res += std::format(
              "    {:>9}    {:>7}\n",
              symbol_id.value,
              type_id);
        }
    }

    return res;
}

}    // namespace slang::sema
