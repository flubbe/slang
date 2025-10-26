/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (macros).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "archives/archive.h"
#include "compiler/codegen/codegen.h"
#include "compiler/macro.h"
#include "compiler/name_utils.h"
#include "compiler/typing.h"
#include "shared/module.h"
#include "ast.h"
#include "builtins.h"
#include "node_registry.h"
#include "loader.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace ld = slang::loader;

namespace slang::ast
{

/*
 * expression.
 */

bool expression::expand_macros(
  cg::context& codegen_ctx,
  ty::context& type_ctx,
  macro::env& macro_env,
  const std::vector<expression*>& macro_asts)
{
    std::size_t macro_expansion_count{0};
    std::vector<ast::macro_invocation*> expanded_imported_macros;

    auto filter_macros = [](const expression& e) -> bool
    {
        return !e.is_macro_expression();
    };

    // replace macro nodes by the macro AST.
    auto macro_expansion_visitor =
      [&codegen_ctx,
       &macro_env,
       &macro_asts,
       &macro_expansion_count,
       &expanded_imported_macros](expression& e)
    {
        if(!e.is_macro_invocation())
        {
            return;
        }

        auto* macro_expr = e.as_macro_invocation();
        if(macro_expr->has_expansion())
        {
            return;
        }

        if(!macro_expr->get_namespace_path().has_value())
        {
            // expand local macro.
            auto it = std::ranges::find_if(
              macro_asts,
              [macro_expr](const expression* m) -> bool
              {
                  if(!m->is_macro_expression())
                  {
                      throw cg::codegen_error(
                        m->get_location(),
                        std::format("Non-macro expression in macro list."));
                  }

                  if(!m->is_named_expression())
                  {
                      throw cg::codegen_error(
                        m->get_location(),
                        std::format("Unnamed expression in macro list."));
                  }

                  return macro_expr->get_name().s == m->as_named_expression()->get_name().s;
              });

            if(it == macro_asts.cend())
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' not found.",
                    macro_expr->get_name().s));
            }

            macro_expr->set_expansion(
              (*it)->as_macro_expression()->expand(codegen_ctx, *macro_expr));
        }
        else
        {
            // expand imported macro.
            auto* m = macro_env.get_macro(
              macro_expr->get_name().s,
              macro_expr->get_namespace_path());

            // check for built-in macros.
            if(m->get_import_path().value_or("<invalid-import-path>") == "std"
               && m->get_name() == "format!")
            {
                macro_expr->set_expansion(
                  std::make_unique<format_macro_expression>(
                    macro_expr->get_location(),
                    macro_expr->get_exprs()));
            }
            else
            {
                if(!m->get_desc().serialized_ast.has_value())
                {
                    throw cg::codegen_error(
                      macro_expr->get_location(),
                      std::format(
                        "Could not load macro '{}' (no data).",
                        macro_expr->get_name().s));
                }

                memory_read_archive ar{
                  m->get_desc().serialized_ast.value(),
                  true,
                  std::endian::little};

                std::unique_ptr<expression> macro_ast;
                ar& expression_serializer{macro_ast};

                // Local macros and functions need to have their namespace added here.
                macro_ast->visit_nodes(
                  [&macro_expr](expression& e)
                  {
                      if(!e.is_call_expression() && !e.is_macro_invocation())
                      {
                          return;
                      }

                      // make sure that we have the actual expression (and not a namespace access),
                      // and that the namespace is empty (meaning the expression invokes a local macro/function).
                      if(e.get_id() != ast::node_identifier::namespace_access_expression
                         && !e.get_namespace_path().has_value())
                      {
                          // Set the namespace to the import's name (stored in macro_expr).
                          e.set_namespace(macro_expr->get_namespace());
                      }
                  },
                  false);

                macro_expr->set_expansion(
                  macro_ast->as_macro_expression()->expand(codegen_ctx, *macro_expr));

                expanded_imported_macros.push_back(macro_expr);
            }
        }

        ++macro_expansion_count;
    };

    auto function_import_visitor = [&codegen_ctx, &type_ctx](expression& e)
    {
        if(!e.is_macro_invocation())
        {
            return;
        }

        auto* macro_expr = e.as_macro_invocation();
        if(!macro_expr->has_expansion())
        {
            return;
        }

        // Handle transitive import names.
        macro_expr->expansion->visit_nodes(
          [&codegen_ctx, &type_ctx](expression& e) -> void
          {
              if(!e.is_call_expression())
              {
                  return;
              }

              // function calls can never be made for transitive imports,
              // that is, all modules have to be explicit imports here.

              auto* c = e.as_call_expression();
              std::optional<std::string> namespace_path = c->get_namespace_path();
              if(namespace_path.has_value())
              {
                  const auto& path = namespace_path.value();

                  // TODO type context
                  throw std::runtime_error("expression::expand_macros");
              }
          },
          true,
          false);
    };

    visit_nodes(
      macro_expansion_visitor,
      true,  /* visit this node */
      false, /* pre-order traversal */
      filter_macros);

    visit_nodes(
      function_import_visitor,
      true,  /* visit this node */
      false, /* pre-order traversal */
      filter_macros);

    for(auto& macro_expr: expanded_imported_macros)
    {
        // adjust local namespaces and transitive import names.
        macro_expr->expansion->visit_nodes(
          [&macro_expr, &type_ctx](expression& e) -> void
          {
              if(e.is_macro_invocation())
              {
                  if(e.get_id() != ast::node_identifier::namespace_access_expression
                     && !e.get_namespace_path().has_value())
                  {
                      // Set the namespace to the import's name (stored in macro_expr).
                      e.set_namespace(macro_expr->get_namespace());
                  }

                  auto* m = e.as_macro_invocation();

                  // TODO type context
                  throw std::runtime_error("expand_macros");

                  //   m->name.s = ld::make_import_name(
                  //     m->name.s,
                  //     type_ctx.is_transitive_import(
                  //       m->get_namespace_path().value()));
              }
          },
          true,
          false);
    }

    return macro_expansion_count != 0;
}

/*
 * macro_invocation.
 */

std::unique_ptr<expression> macro_invocation::clone() const
{
    return std::make_unique<macro_invocation>(*this);
}

void macro_invocation::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{exprs};
    ar& expression_serializer{index_expr};
    ar& expression_serializer{expansion};
}

std::unique_ptr<cg::value> macro_invocation::generate_code(
  cg::context& ctx,
  memory_context mc) const
{
    // Code generation for macros.
    if(!expansion)
    {
        throw cg::codegen_error(loc, "Macro was not expanded.");
    }

    auto return_type = expansion->generate_code(ctx, mc);
    if(index_expr)
    {
        // evaluate the index expression.
        index_expr->generate_code(ctx, memory_context::load);

        auto type = ctx.deref(return_type->get_type());
        ctx.generate_load_element(
          cg::type_argument{type});
        return std::make_unique<cg::value>(type);
    }

    return return_type;
}

void macro_invocation::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    for(auto& expr: exprs)
    {
        expr->collect_names(ctx);
    }

    if(index_expr)
    {
        index_expr->collect_names(ctx);
    }

    if(expansion)
    {
        expansion->collect_names(ctx);
    }
}

std::optional<ty::type_id> macro_invocation::type_check(ty::context& ctx, sema::env& env)
{
    // Type checking for macros.
    if(!expansion)
    {
        throw cg::codegen_error(loc, "Macro was not expanded.");
    }

    if(expansion->is_macro_branch())
    {
        macro_branch* branch = expansion->as_macro_branch();
        const std::vector<expression*> exprs = branch->get_body()->get_children();
        if(exprs.empty())
        {
            return {};
        }

        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            exprs[i]->type_check(ctx, env);
        }

        auto expr_type = exprs.back()->type_check(ctx, env);
        if(expr_type.has_value())
        {
            ctx.set_expression_type(*this, expr_type);
        }

        return expr_type;
    }

    auto expr_type = expansion->type_check(ctx, env);
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string macro_invocation::to_string() const
{
    std::string ret = std::format(
      "MacroInvocation(callee={}, exprs=(", get_name().s);

    if(!exprs.empty())
    {
        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            ret += std::format("{}, ", exprs[i]->to_string());
        }
        ret += std::format("{}", exprs.back()->to_string());
    }
    if(expansion)
    {
        ret += std::format("), expansion=({}", expansion->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * macro_branch.
 */

std::unique_ptr<expression> macro_branch::clone() const
{
    return std::make_unique<macro_branch>(*this);
}

void macro_branch::serialize(archive& ar)
{
    super::serialize(ar);
    ar & args;
    ar & args_end_with_list;
    ar & body;
}

std::unique_ptr<cg::value> macro_branch::generate_code(
  cg::context& ctx,
  memory_context mc) const
{
    return body->generate_code(ctx, mc);
}

void macro_branch::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    // FIXME macro branches are uniquely identified by their
    //       parent macro name and its parameters.
    ctx.push_scope(std::nullopt, loc);

    for(const auto& [arg_name, arg_type]: args)
    {
        const std::string canonical_name = name::qualified_name(
          ctx.get_canonical_scope_name(ctx.get_current_scope()),
          arg_name.s);

        ctx.declare(
          arg_name.s,
          canonical_name,
          sema::symbol_type::macro_argument,
          arg_name.location,
          sema::symbol_id::invalid,
          false);
    }

    body->collect_names(ctx, false);

    ctx.pop_scope();
}

std::string macro_branch::to_string() const
{
    std::string ret = std::format("MacroBranch(args=(");
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            const auto& arg = args[i];
            ret += std::format("(name={}, type={}), ", arg.first.s, arg.second.s);
        }
        ret += std::format("(name={}, type={})", args.back().first.s, args.back().second.s);
    }
    ret += std::format(
      "), args_end_with_list={}, body={})",
      args_end_with_list ? "true" : "false",
      body->to_string());
    return ret;
}

/*
 * macro_expression_list.
 */

std::unique_ptr<expression> macro_expression_list::clone() const
{
    return std::make_unique<macro_expression_list>(*this);
}

void macro_expression_list::serialize(archive& ar)
{
    super::serialize(ar);
    ar & expr_list;
}

void macro_expression_list::collect_names(
  [[maybe_unused]] co::context& ctx)
{
    throw cg::codegen_error(loc, "Non-expanded macro expression list.");
}

std::unique_ptr<cg::value> macro_expression_list::generate_code(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] memory_context mc) const
{
    throw cg::codegen_error(loc, "Non-expanded macro expression list.");
}

std::optional<ty::type_id> macro_expression_list::type_check(
  [[maybe_unused]] ty::context& ctx,
  [[maybe_unused]] sema::env& env)
{
    return std::nullopt;
}

std::string macro_expression_list::to_string() const
{
    std::string ret = std::format("MacroExpressionList(expr_list=(");
    if(!expr_list.empty())
    {
        for(std::size_t i = 0; i < expr_list.size() - 1; ++i)
        {
            ret += std::format("{}, ", expr_list[i]->to_string());
        }
        ret += std::format("{}", expr_list.back()->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * macro_expression.
 */

std::unique_ptr<expression> macro_expression::clone() const
{
    return std::make_unique<macro_expression>(*this);
}

void macro_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{branches};
}

void macro_expression::collect_names(
  co::context& ctx)
{
    super::collect_names(ctx);

    const std::string canonical_name = name::qualified_name(
      ctx.get_canonical_scope_name(ctx.get_current_scope()),
      name.s);

    symbol_id = ctx.declare(
      name.s,
      canonical_name,
      sema::symbol_type::macro,
      name.location,
      sema::symbol_id::invalid,
      false,
      this);

    ctx.push_scope(std::format("{}@macro", name.s), name.location);

    for(auto& macro_branch: branches)
    {
        macro_branch->collect_names(ctx);
    }

    ctx.pop_scope();
}

std::unique_ptr<cg::value> macro_expression::generate_code(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] memory_context mc) const
{
    // empty, as macros don't generate code.
    return nullptr;
}

std::optional<ty::type_id> macro_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    // Check that all necessary imports exist in the type context.

    visit_nodes(
      [this, &ctx, &env](expression& e) -> void
      {
          std::optional<std::string> namespace_path{std::nullopt};

          if(e.get_id() == ast::node_identifier::namespace_access_expression)
          {
              e.update_namespace();

              namespace_path = e.get_namespace_path();
              if(!namespace_path.has_value())
              {
                  throw ty::type_error(
                    loc,
                    std::format(
                      "Unable to get namespace in macro expansion at {}.",
                      ::slang::to_string(e.get_location())));
              }

              // TODO check imports.
              throw std::runtime_error("macro_expression::type_check (imports)");

              //   if(!ctx.has_import(namespace_path.value()))
              //   {
              //       throw ty::type_error(
              //         loc,
              //         std::format(
              //           "Unresolved import '{}',",
              //           namespace_path.value()));
              //   }
          }

          if(e.is_call_expression())
          {
              if(!env.get_symbol_id(
                       e.as_call_expression()->get_qualified_callee_name(),
                       sema::symbol_type::function,
                       scope_id.value())
                    .has_value())
              {
                  throw ty::type_error(
                    e.get_location(),
                    std::format(
                      "Unresolved symbol '{}'.",
                      e.as_call_expression()->get_qualified_callee_name()));
              }
          }
          else if(e.is_macro_invocation())
          {
              if(!env.get_symbol_id(
                       e.as_macro_invocation()->get_qualified_name(),
                       sema::symbol_type::macro,
                       scope_id.value())
                    .has_value())
              {
                  throw ty::type_error(
                    loc,
                    std::format(
                      "Unresolved symbol '{}',",
                      e.as_macro_invocation()->get_qualified_name()));
              }
          }
      },
      false,
      true);

    return std::nullopt;
}

std::string macro_expression::to_string() const
{
    std::string ret = std::format("Macro(name={}, branches=(", name.s);
    if(!branches.empty())
    {
        for(std::size_t i = 0; i < branches.size() - 1; ++i)
        {
            const auto& branch = branches[i];
            ret += std::format("{}, ", branch->to_string());
        }
        ret += std::format("{}", branches.back()->to_string());
    }
    ret += "))";
    return ret;
}

void macro_expression::collect_macro(sema::env& sema_env, macro::env& macro_env) const
{
    std::vector<std::pair<std::string, module_::directive_descriptor>> attributes;

    // FIXME Module macro descriptors have to be updated.
    if(sema_env.attribute_map.contains(symbol_id.value()))
    {
        attributes = sema_env.attribute_map[symbol_id.value()]
                     | std::views::transform(
                       [](const attribs::attribute_info& info) -> std::pair<std::string, module_::directive_descriptor>
                       {
                           if(info.kind == attribs::attribute_kind::builtin)
                           {
                               return std::make_pair(
                                 attribs::to_string(info.kind),
                                 module_::directive_descriptor{});
                           }

                           throw attribs::attribute_error(
                             std::format(
                               "{}: Attribute '{}' invalid for macro expressions.",
                               ::slang::to_string(info.loc),
                               attribs::to_string(info.kind)));
                       })
                     | std::ranges::to<std::vector>();
    }

    memory_write_archive ar{true, std::endian::little};
    auto cloned_expr = clone();    // FIXME Needed for std::unique_ptr, so that expression_serializer matches
    ar& expression_serializer{cloned_expr};

    macro_env.add_macro(
      name.s,
      module_::macro_descriptor{
        std::move(attributes),
        ar.get_buffer()},
      get_namespace_path());
}

/**
 * Get the best-matching macro branch for a macro invocation.
 *
 * @param loc Invocation location.
 * @param macro_expr The macro expression.
 * @param invocation_exprs The expressions used for macro invocation.
 * @returns Returns a matching macro branch.
 * @throws Throws `cg::codegen_error` if `macro_expr` is not a macro expression,
 *         multiple matches were found, or no matches were found.
 */
static const macro_branch* get_matching_branch(
  source_location loc,
  const macro_expression* macro_expr,
  const std::vector<std::unique_ptr<expression>>& invocation_exprs)
{
    if(!macro_expr->is_macro_expression())
    {
        throw cg::codegen_error(
          macro_expr->get_location(),
          "Cannot match macro branches for non-macro expression.");
    }

    std::pair<const macro_branch*, std::size_t> match{nullptr, 0};
    std::pair<const macro_branch*, std::size_t> tie{nullptr, 0};

    auto update_match = [&match, &tie](const macro_branch* mb, std::size_t score) -> void
    {
        if(match.second < score)
        {
            match = std::make_pair(mb, score);
            tie = {nullptr, 0};
        }
        else if(match.second == score)
        {
            tie = std::make_pair(mb, score);
        }
    };

    for(const auto* b: macro_expr->get_children())
    {
        if(!b->is_macro_branch())
        {
            throw cg::codegen_error(
              b->get_location(),
              "Macro contains non-branch expression.");
        }
        const auto* mb = b->as_macro_branch();

        // check if arguments match exactly.
        if(invocation_exprs.size() == mb->get_args().size())
        {
            if(!mb->ends_with_list_capture())
            {
                // arguments match exactly.
                update_match(mb, 3);
            }
            else
            {
                update_match(mb, 2);
            }
        }
        else if(invocation_exprs.size() > mb->get_args().size()
                && mb->ends_with_list_capture())
        {
            update_match(mb, 1);
        }
    }

    if(tie.first != nullptr)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Macro branches at {} and {} both match.",
            slang::to_string(match.first->get_location()),
            slang::to_string(tie.first->get_location())));
    }

    if(match.first == nullptr)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Could not match branch for macro '{}' defined at {}.",
            macro_expr->as_named_expression()->get_name().s,
            slang::to_string(macro_expr->get_location())));
    }

    return match.first;
}

/**
 * Get invocation expressions and expand macro expression lists.
 *
 * @param invocation Macro invocation.
 * @return Returns the invocation arguments.
 */
static std::vector<std::unique_ptr<expression>> expand_invocation_args(
  const macro_invocation& invocation)
{
    std::vector<std::unique_ptr<expression>> invocation_exprs =
      invocation.get_exprs()
      | std::views::transform([](const auto& e)
                              { return e->clone(); })
      | std::ranges::to<std::vector>();

    if(!invocation_exprs.empty())
    {
        for(std::size_t i = 0; i < invocation_exprs.size() - 1; ++i)
        {
            const auto& expr = invocation_exprs[i];

            if(!expr->is_variable_reference())
            {
                continue;
            }
            const auto* var_ref = expr->as_variable_reference();

            if(!var_ref->has_expansion())
            {
                continue;
            }

            if(var_ref->get_expansion()->is_macro_expression_list())
            {
                throw cg::codegen_error(
                  invocation_exprs[i]->get_location(),
                  std::format(
                    "Argument {} cannot be a macro expression list.",
                    i));
            }
        }

        if(invocation_exprs.back()->is_variable_reference())
        {
            const auto* var_ref = invocation_exprs.back()->as_variable_reference();
            if(var_ref->has_expansion() && var_ref->get_expansion()->is_macro_expression_list())
            {
                auto expr_list = std::move(invocation_exprs.back());
                invocation_exprs.pop_back();

                for(auto& expr: var_ref->get_expansion()->as_macro_expression_list()->get_expr_list())
                {
                    invocation_exprs.emplace_back(std::move(expr));
                }
            }
        }
    }

    return invocation_exprs;
}

/**
 * Create a local name to avoid name clashes in macros.
 *
 * @param invocation_id Macro invocation id, as generated by the code generation context.
 * @param name Original name.
 * @returns Returns a local, non-clashing version of the name.
 */
static std::string make_local_name(std::size_t invocation_id, std::string name)
{
    return std::format("${}{}", invocation_id, name);
}

std::unique_ptr<expression> macro_expression::expand(
  cg::context& ctx,
  const macro_invocation& invocation) const
{
    // Get invocation expressions and expand macro expression lists.
    std::vector<std::unique_ptr<expression>> invocation_exprs = expand_invocation_args(invocation);

    // Get matching macro branch for the argument list.
    std::unique_ptr<macro_branch> branch =
      std::unique_ptr<macro_branch>{
        static_cast<macro_branch*>(    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          get_matching_branch(
            invocation.get_location(),
            this,
            invocation_exprs)
            ->clone()
            .release())};

    std::set<std::string> original_local_names;

    auto collect_names_visitor = [&original_local_names](expression& e) -> void
    {
        if(e.is_macro_branch())
        {
            // collect arguments.
            auto* expr = e.as_macro_branch();
            for(auto& arg: expr->args)
            {
                original_local_names.insert(std::get<0>(arg).s);
            }
        }
        else if(e.is_variable_declaration())
        {
            // collect macro variable.
            auto* expr = e.as_variable_declaration();
            original_local_names.insert(expr->name.s);
        }
    };

    branch->visit_nodes(
      collect_names_visitor,
      true /* visit this node */
    );

    const std::size_t invocation_id = ctx.generate_macro_invocation_id();
    auto rename_visitor = [&original_local_names, invocation_id, &ctx](expression& e) -> void
    {
        if(e.is_macro_branch())
        {
            // rename arguments.
            auto* expr = e.as_macro_branch();
            for(auto& arg: expr->args)
            {
                std::get<0>(arg).s = make_local_name(invocation_id, std::get<0>(arg).s);
            }
        }
        else if(e.is_variable_declaration())
        {
            // rename macro variable.
            auto* expr = e.as_variable_declaration();
            expr->name.s = make_local_name(invocation_id, expr->name.s);
        }
        else if(e.is_variable_reference())
        {
            // rename macro variable.
            auto* expr = e.as_variable_reference();
            if(original_local_names.contains(expr->name.s)
               || !ctx.has_registered_constant_name(expr->name.s))
            {
                expr->name.s = make_local_name(invocation_id, expr->name.s);
            }
        }
        else if(e.is_struct_member_access())
        {
            // rename macro variable.
            auto* expr = e.as_access_expression()->get_left_expression()->as_named_expression();
            if(original_local_names.contains(expr->name.s))
            {
                expr->name.s = make_local_name(invocation_id, expr->name.s);
            }
        }
    };

    branch->visit_nodes(
      rename_visitor,
      true /* visit this node */
    );

    std::unordered_map<std::string, size_t> arg_pos;
    for(std::size_t i = 0; i < branch->get_args().size(); ++i)
    {
        const auto& arg = branch->get_args()[i];

        if(std::ranges::find_if(
             arg_pos,
             [arg](const auto& p) -> bool
             {
                 return arg.first.s == p.first;
             })
           != arg_pos.end())
        {
            throw cg::codegen_error(
              arg.first.location,
              std::format(
                "Argument '{}' was already defined.",
                arg.first.s));
        }

        arg_pos.insert({arg.first.s, i});
    }

    // Expand with the invocation expressions.
    auto expand_visitor = [&invocation, &invocation_exprs, &branch, &arg_pos](expression& e) -> void
    {
        if(e.is_variable_reference())
        {
            // if the variable is one of the arguments, expand with the
            // corresponding invocation item.

            auto* ref_expr = e.as_variable_reference();
            auto it = std::ranges::find_if(
              std::as_const(arg_pos),
              [ref_expr](const auto& p) -> bool
              {
                  return ref_expr->get_name().s == p.first;
              });
            if(it == arg_pos.cend())
            {
                // not an argument.
                return;
            }

            // handle expression lists.
            if(it->second == branch->args.size() - 1
               && branch->ends_with_list_capture())
            {
                // capture all arguments from arg_pos.second on.
                std::size_t expr_count = invocation_exprs.size() - it->second;
                if(expr_count == 0)
                {
                    throw cg::codegen_error(
                      invocation.get_location(),
                      "Empty expression list.");
                }

                if(expr_count == 1)
                {
                    ref_expr->set_expansion(invocation_exprs.at(it->second)->clone());
                }
                else
                {
                    std::vector<std::unique_ptr<expression>> exprs;
                    exprs.reserve(expr_count);

                    for(std::size_t i = 0; i < expr_count; ++i)
                    {
                        exprs.emplace_back(
                          invocation_exprs.at(it->second + i)->clone());
                    }

                    ref_expr->set_expansion(
                      std::make_unique<macro_expression_list>(
                        invocation.get_location(),
                        std::move(exprs)));
                }
            }
            else
            {
                ref_expr->set_expansion(invocation_exprs.at(it->second)->clone());
            }
        }
    };

    branch->visit_nodes(
      expand_visitor,
      false /* don't visit this node. */
    );

    return branch;
}

}    // namespace slang::ast
