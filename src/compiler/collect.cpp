/**
 * slang - a simple scripting language.
 *
 * name collection.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <ranges>

#include "compiler/collect.h"
#include "compiler/name_utils.h"

namespace slang::collect
{

/*
 * helpers.
 */

/**
 * Check if the environment contains a scope.
 *
 * @param env Environment to search for the scope.
 * @param id Scope id.
 * @returns `true` if the environment contains the scope, and `false` otherwise.
 */
static bool has_scope(sema::env& env, sema::scope_id id)
{
    return env.scope_map.contains(id);
}

/*
 * redefinition_error.
 */

redefinition_error::redefinition_error(
  const std::string& symbol_name,
  sema::symbol_type type,
  source_location loc,    // NOLINT(bugprone-easily-swappable-parameters)
  source_location original_loc)
: collection_error{std::format(
    "{}: Redeclaration of '{}' (was already defined at {})",
    to_string(loc),
    symbol_name,
    to_string(original_loc))}
, symbol_name{symbol_name}
, type{type}
, loc{loc}
, original_loc{original_loc}
{
}

/*
 * context.
 */

std::string context::generate_scope_name()
{
    return std::format("scope#{}", anonymous_scope_counter++);
}

sema::scope_id context::create_scope(
  sema::scope_id parent,
  const std::optional<std::string>& name,
  source_location loc)
{
    if(parent == sema::scope::invalid_id
       && !env.scope_map.empty())
    {
        throw std::runtime_error("Scope table not empty.");
    }

    auto new_scope_id = generate_scope_id();

    auto it = env.scope_map.insert(
      {new_scope_id,
       sema::scope{
         .parent = parent,
         .name = name.value_or(generate_scope_name()),
         .loc = loc}});
    if(!it.second)
    {
        throw collection_error(
          std::format(
            "Scope with id '{}' already exists to scope table.",
            new_scope_id));
    }

    return it.first->first;
}

sema::symbol_id context::declare(
  std::string name,
  std::string qualified_name,
  sema::symbol_type type,
  source_location loc,
  sema::symbol_id declaring_module,
  bool transitive,
  std::optional<
    std::variant<
      const ast::expression*,
      const module_::exported_symbol*>>
    reference)
{
    sema::scope* s = get_scope(current_scope);

    auto it = std::ranges::find_if(
      s->bindings,
      [&name, type](const auto& b) -> bool
      {
          if(b.first != name)
          {
              return false;
          }

          return b.second.contains(type);
      });
    if(it != s->bindings.end())
    {
        auto symbol_id = it->second[type];

        // Redefinitions are allowed if they change transitivity from `true` to `false`.
        if(!transitive && env.transitive_imports.contains(symbol_id))
        {
            // TODO qualified name? reference?

            env.transitive_imports.erase(symbol_id);
            return symbol_id;
        }
        auto original = env.symbol_table.find(symbol_id);
        if(original == env.symbol_table.end())
        {
            throw std::runtime_error(
              std::format(
                "{}: Redefinition of symbol '{}', but original definition not found in symbol table.",
                to_string(loc),
                name));
        }

        throw redefinition_error(
          name,
          type,
          loc,
          original->second.loc);
    }

    // insert new declaration.
    auto new_symbol_id = generate_symbol_id();

    it = std::ranges::find_if(
      s->bindings,
      [&name](const auto& b) -> bool
      {
          return b.first == name;
      });
    if(it == s->bindings.end())
    {
        s->bindings.insert(
          {name,
           std::unordered_map<sema::symbol_type, sema::symbol_id>{
             {type, new_symbol_id}}});
    }
    else
    {
        it->second.insert({type, new_symbol_id});
    }

    auto [entry, success] = env.symbol_table.insert(
      {new_symbol_id,
       sema::symbol_info{
         .name = std::move(name),
         .qualified_name = std::move(qualified_name),
         .type = type,
         .loc = loc,
         .scope = current_scope,
         .declaring_module = declaring_module,
         .reference = reference}});
    if(!success)
    {
        throw redefinition_error(
          entry->second.name,
          type,
          loc,
          entry->second.loc);
    }

    if(transitive)
    {
        env.transitive_imports.insert(new_symbol_id);
    }

    return new_symbol_id;
}

bool context::declare_external(
  std::string qualified_name,
  sema::symbol_type type,
  source_location loc)
{
    sema::scope* s = get_scope(context::global_scope_id);

    auto it = std::ranges::find_if(
      s->bindings,
      [&qualified_name, type](const auto& b) -> bool
      {
          if(b.first != qualified_name)
          {
              return false;
          }

          return b.second.contains(type);
      });
    if(it != s->bindings.end())
    {
        auto symbol_id = it->second[type];

        // Redefinitions are allowed (e.g. multiple module referencing the same module / symbol).
        if(env.transitive_imports.contains(symbol_id))
        {
            env.transitive_imports.erase(symbol_id);
        }

        return false;
    }

    // insert new declaration.
    auto new_symbol_id = generate_symbol_id();

    it = std::ranges::find_if(
      s->bindings,
      [&qualified_name](const auto& b) -> bool
      {
          return b.first == qualified_name;
      });
    if(it == s->bindings.end())
    {
        s->bindings.insert(
          {qualified_name,
           std::unordered_map<sema::symbol_type, sema::symbol_id>{
             {type, new_symbol_id}}});
    }
    else
    {
        it->second.insert({type, new_symbol_id});
    }

    auto [entry, success] = env.symbol_table.insert(
      {new_symbol_id,
       sema::symbol_info{
         .name = qualified_name,
         .qualified_name = qualified_name,
         .type = type,
         .loc = loc,
         .scope = current_scope,
         .declaring_module = sema::symbol_id::invalid,
         .reference = std::nullopt}});
    if(!success)
    {
        throw redefinition_error(
          entry->second.name,
          type,
          loc,
          entry->second.loc);
    }

    return true;
}

bool context::has_symbol(
  const std::string& name,
  sema::symbol_type type) const
{
    if(name.find("::") != std::string::npos)
    {
        auto it = std::ranges::find_if(
          env.symbol_table,
          [&name, type](const auto& p) -> bool
          {
              return p.second.type == type
                     && p.second.qualified_name == name;
          });

        return it != env.symbol_table.cend();
    }

    const sema::scope* s = get_scope(current_scope);

    auto it = std::ranges::find_if(
      s->bindings,
      [&name, type](const auto& b) -> bool
      {
          if(b.first != name)
          {
              return false;
          }

          return b.second.contains(type);
      });

    return it != s->bindings.cend();
}

sema::scope_id context::push_scope(
  const std::optional<std::string>& name,
  source_location loc)
{
    current_scope = create_scope(current_scope, name, loc);
    return current_scope;
}

void context::push_scope(sema::scope_id id)
{
    if(id == sema::scope::invalid_id)
    {
        throw collection_error(
          std::format(
            "Cannot enter invalid scope."));
    }

    if(!has_scope(env, id))
    {
        throw collection_error(
          std::format(
            "Cannot enter unknown scope '{}'.",
            id));
    }

    current_scope = id;
}

void context::pop_scope()
{
    current_scope = get_scope(current_scope)->parent;
    if(current_scope != sema::scope::invalid_id
       && !has_scope(env, current_scope))
    {
        if(reference == nullptr
           || !has_scope(reference->env, current_scope))
        {
            throw std::runtime_error("Invalid scope after pop.");
        }
    }
}

/**
 * Scope getter implementation.
 *
 * @throws Throws `std::runtime_error` if the scope id is
 *         invalid or if the scope is not found.
 * @param env Environment to search for the scope.
 * @param id Scope id.
 * @returns Returns the scope.
 */
template<typename T>
T* get_scope(
  std::conditional_t<
    std::is_const_v<T>,
    const sema::env,
    sema::env>&
    env,
  sema::scope_id id)
{
    if(id == sema::scope::invalid_id)
    {
        throw std::runtime_error("Invalid scope id.");
    }

    auto it = env.scope_map.find(id);
    if(it == env.scope_map.end())
    {
        throw std::runtime_error("Scope not found in scope table.");
    }

    return &it->second;
}

sema::scope* context::get_scope(sema::scope_id id)
{
    return ::slang::collect::get_scope<sema::scope>(env, id);
}

const sema::scope* context::get_scope(sema::scope_id id) const
{
    return ::slang::collect::get_scope<const sema::scope>(env, id);
}

std::string context::get_canonical_scope_name(sema::scope_id id) const
{
    if(!has_scope(env, id))
    {
        if(reference == nullptr)
        {
            throw std::runtime_error("Scope not found in scope table.");
        }

        return reference->get_canonical_scope_name(id);
    }

    const auto* s = ::slang::collect::get_scope<sema::scope>(env, id);
    std::string name = s->name;

    for(id = s->parent; id != sema::scope::invalid_id && has_scope(env, id);)
    {
        const auto* s = get_scope(id);
        name = name::qualified_name(s->name, name);

        id = s->parent;
    }

    if(id != sema::scope::invalid_id)
    {
        if(reference == nullptr)
        {
            throw std::runtime_error("Scope not found in scope table.");
        }

        return name::qualified_name(
          reference->get_canonical_scope_name(id),
          name);
    }

    return name;
}

}    // namespace slang::collect