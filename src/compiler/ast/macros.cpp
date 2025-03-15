/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (macros).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "shared/module.h"
#include "compiler/codegen.h"
#include "compiler/typing.h"
#include "ast.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/*
 * macro_branch.
 */

std::unique_ptr<expression> macro_branch::clone() const
{
    return std::make_unique<macro_branch>(
      get_location(),
      args,
      args_end_with_list,
      std::unique_ptr<block>{static_cast<block*>(body->clone().release())});    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
}

std::unique_ptr<cg::value> macro_branch::generate_code(
  cg::context& ctx,
  memory_context mc) const
{
    return body->generate_code(ctx, mc);
}

std::string macro_branch::to_string() const
{
    std::string ret = fmt::format("MacroBranch(args=(");
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            const auto& arg = args[i];
            ret += fmt::format("(name={}, type={}), ", arg.first.s, arg.second.s);
        }
        ret += fmt::format("(name={}, type={})", args.back().first.s, args.back().second.s);
    }
    ret += fmt::format(
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
    std::vector<std::unique_ptr<expression>> cloned_expr_list;
    cloned_expr_list.reserve(expr_list.size());
    for(const auto& expr: expr_list)
    {
        cloned_expr_list.emplace_back(expr->clone());
    }

    return std::make_unique<macro_expression_list>(loc, std::move(cloned_expr_list));
}

void macro_expression_list::collect_names(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] ty::context& type_ctx) const
{
    throw cg::codegen_error(loc, "Non-expanded macro expression list.");
}

std::unique_ptr<cg::value> macro_expression_list::generate_code(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] memory_context mc) const
{
    throw cg::codegen_error(loc, "Non-expanded macro expression list.");
}

std::optional<ty::type_info> macro_expression_list::type_check([[maybe_unused]] ty::context& ctx)
{
    return std::nullopt;
}

std::string macro_expression_list::to_string() const
{
    std::string ret = fmt::format("MacroExpressionList(expr_list=(");
    if(!expr_list.empty())
    {
        for(std::size_t i = 0; i < expr_list.size() - 1; ++i)
        {
            ret += fmt::format("{}, ", expr_list[i]->to_string());
        }
        ret += fmt::format("{}", expr_list.back()->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * macro_expression.
 */

std::unique_ptr<expression> macro_expression::clone() const
{
    std::vector<std::unique_ptr<macro_branch>> cloned_branches;
    cloned_branches.reserve(branches.size());
    for(const auto& branch: branches)
    {
        cloned_branches.emplace_back(
          static_cast<macro_branch*>(branch->clone().release())    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        );
    }

    return std::make_unique<macro_expression>(loc, name, std::move(cloned_branches));
}

void macro_expression::collect_names(
  cg::context& ctx,
  [[maybe_unused]] ty::context& type_ctx) const
{
    std::vector<std::pair<std::string, module_::directive_descriptor>> directives;
    std::transform(
      ctx.get_directives().cbegin(),
      ctx.get_directives().cend(),
      std::back_inserter(directives),
      [](const cg::directive& d) -> std::pair<std::string, module_::directive_descriptor>
      {
          std::vector<std::pair<std::string, std::string>> args;
          std::transform(
            d.args.cbegin(),
            d.args.cend(),
            std::back_inserter(args),
            [](const std::pair<token, token>& arg) -> std::pair<std::string, std::string>
            {
                return std::make_pair(arg.first.s, arg.second.s);
            });
          return std::make_pair(d.name.s, module_::directive_descriptor{std::move(args)});
      });

    ctx.add_macro(
      name.s,
      module_::macro_descriptor{std::move(directives)},
      get_namespace_path());
}

std::unique_ptr<cg::value> macro_expression::generate_code(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] memory_context mc) const
{
    // empty, as macros don't generate code.
    return nullptr;
}

bool macro_expression::supports_directive(const std::string& name) const
{
    return super::supports_directive(name)
           || name == "builtin";
}

std::string macro_expression::to_string() const
{
    std::string ret = fmt::format("Macro(name={}, branches=(", name.s);
    if(!branches.empty())
    {
        for(std::size_t i = 0; i < branches.size() - 1; ++i)
        {
            const auto& branch = branches[i];
            ret += fmt::format("{}, ", branch->to_string());
        }
        ret += fmt::format("{}", branches.back()->to_string());
    }
    ret += "))";
    return ret;
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
  token_location loc,
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
          fmt::format(
            "Macro branches at {} and {} both match.",
            slang::to_string(match.first->get_location()),
            slang::to_string(tie.first->get_location())));
    }

    if(match.first == nullptr)
    {
        throw cg::codegen_error(
          loc,
          fmt::format(
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
    std::vector<std::unique_ptr<expression>> invocation_exprs;
    invocation_exprs.reserve(invocation.get_exprs().size());
    for(const auto& expr: invocation.get_exprs())
    {
        invocation_exprs.emplace_back(expr->clone());
    }

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
                  fmt::format(
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

    const std::string prefix = fmt::format("${}", ctx.generate_macro_invocation_id());
    auto rename_visitor = [&prefix](expression& e) -> void
    {
        if(e.is_macro_branch())
        {
            // rename arguments.
            auto* expr = e.as_macro_branch();
            for(auto& arg: expr->args)
            {
                std::get<0>(arg).s = fmt::format("{}{}", prefix, std::get<0>(arg).s);
            }
        }
        else if(e.is_variable_declaration())
        {
            // rename macro variable.
            auto* expr = e.as_variable_declaration();
            expr->name.s = fmt::format("{}{}", prefix, expr->name.s);
        }
        else if(e.is_variable_reference())
        {
            // rename macro variable.
            auto* expr = e.as_variable_reference();
            expr->name.s = fmt::format("{}{}", prefix, expr->name.s);
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

        // FIXME Move this out.
        if(std::find_if(
             arg_pos.begin(),
             arg_pos.end(),
             [arg](const auto& p) -> bool
             {
                 return arg.first.s == p.first;
             })
           != arg_pos.end())
        {
            throw cg::codegen_error(
              arg.first.location,
              fmt::format(
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
            auto it = std::find_if(
              arg_pos.begin(),
              arg_pos.end(),
              [ref_expr](const auto& p) -> bool
              {
                  return ref_expr->get_name().s == p.first;
              });
            if(it == arg_pos.end())
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
