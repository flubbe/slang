/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (macros).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "archives/archive.h"
#include "compiler/codegen/codegen.h"
#include "compiler/macro.h"
#include "compiler/name_utils.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "shared/module.h"
#include "ast.h"
#include "builtins.h"
#include "node_registry.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

namespace
{

module_::macro_body_kind get_token_body_kind(
  const token_stream& body,
  source_location loc)
{
    if(!body.empty() && body.back().s == ";")
    {
        std::vector<token> expression_tokens;
        expression_tokens.reserve(body.size() - 1);
        expression_tokens.insert(
          expression_tokens.end(),
          body.data().begin(),
          body.data().end() - 1);

        try
        {
            (void)parser::parse_expression(
              token_stream{std::move(expression_tokens)});
            return module_::macro_body_kind::expression;
        }
        catch(const syntax_error&)
        {
            // A statement list may parse as a block rather than an expression.
        }
    }

    std::vector<token> block_tokens;
    block_tokens.reserve(body.size() + 2);
    block_tokens.emplace_back("{", loc, token_type::delimiter);
    block_tokens.insert(block_tokens.end(), body.data().begin(), body.data().end());
    block_tokens.emplace_back("}", loc, token_type::delimiter);

    (void)parser::parse_expression(token_stream{std::move(block_tokens)});
    return module_::macro_body_kind::statement_list;
}

std::unique_ptr<block> parse_macro_branch_body(const macro_branch& branch)
{
    const auto& opening = branch.get_body_opening_delimiter();
    const auto& closing = branch.get_body_closing_delimiter();
    const auto& body = branch.get_body_tokens();
    if(!opening.has_value() || !closing.has_value() || !body.has_value())
    {
        throw parser_error(
          token{
            "=>",
            branch.get_location(),
            token_type::delimiter},
          "Macro branch body tokens are incomplete.");
    }

    std::vector<token> tokens;
    tokens.reserve(body->size() + 2);
    tokens.push_back(opening.value());
    tokens.insert(tokens.end(), body->data().begin(), body->data().end());
    tokens.push_back(closing.value());

    auto parsed_body = parser::parse_expression(token_stream{std::move(tokens)});
    if(parsed_body->get_id() != node_identifier::block)
    {
        throw parser_error(
          opening.value(),
          "Expected a block as the macro branch body.");
    }

    return std::unique_ptr<block>{
      static_cast<block*>(parsed_body.release())};    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
}

}    // namespace

/*
 * expression.
 */

bool expression::expand_macros(
  co::context& co_ctx,
  rs::context& rs_ctx,
  cg::context& codegen_ctx,
  macro::env& macro_env,
  syntax_context_table& syntax_contexts)
{
    std::size_t macro_expansion_count{0};

    auto filter_macros = [](const expression& e) -> bool
    {
        return !e.is_macro_expression();
    };

    // Shared token-stream expansion helper.
    auto expand_token_macro =
      [&](const macro::macro* descriptor_macro, macro_invocation* expr) -> bool
    {
        if(!descriptor_macro->supports_token_expansion(
             expr->get_argument_tokens(),
             expr->get_argument_separators()))
        {
            return false;
        }
        auto expanded_tokens = descriptor_macro->expand_macro(
          expr->get_argument_tokens(),
          expr->get_name_token().context,
          syntax_contexts,
          expr->get_location(),
          expr->get_argument_separators());
        expr->set_expansion(
          parser::parse_expression(std::move(expanded_tokens)));
        return true;
    };

    // Replace macro invocations with token-stream expansions.
    auto macro_expansion_visitor =
      [&co_ctx,
       &rs_ctx,
       &codegen_ctx,
       &macro_env,
       &syntax_contexts,
       &macro_expansion_count,
       &expand_token_macro](expression& e)
    {
        if(!e.is_macro_invocation())
        {
            return;
        }

        auto* macro_expr = e.as_macro_invocation();

        // format! uses a dedicated late-expansion path with type-aware
        // format string evaluation. Create the placeholder node here,
        // before the main type-check pass, so that type_check sees a
        // valid expansion; the actual string building is filled in later
        // by expand_late_macros (which runs after the type map is bound).
        if(macro_expr->get_name() == "format!")
        {
            // Already expanded on a previous pass. Do nothing and, crucially,
            // do not count this as an expansion. The driver runs
            // do { ... } while(ast->expand_macros(...)), so re-counting the
            // same format! invocation on every pass would loop forever.
            if(macro_expr->has_expansion())
            {
                return;
            }

            macro_expr->set_expansion(
              std::make_unique<format_macro_expression>(
                macro_expr->get_location(),
                macro_expr->get_exprs(),
                macro_expr->get_argument_tokens()));

            if(!macro_expr->scope_id.has_value())
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' has no scope.",
                    macro_expr->get_name()));
            }

            co_ctx.push_scope(macro_expr->scope_id.value());
            macro_expr->expansion->collect_names(co_ctx);
            co_ctx.pop_scope();
            macro_expr->expansion->resolve_names(rs_ctx);
            ++macro_expansion_count;
            return;
        }

        if(macro_expr->has_expansion())
        {
            return;
        }

        if(!macro_expr->get_namespace_path().has_value())
        {
            // expand local macro.
            auto* descriptor_macro = macro_env.get_macro(
              macro_expr->get_name(),
              std::nullopt);
            if(!expand_token_macro(descriptor_macro, macro_expr))
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' is not supported by token expansion.",
                    macro_expr->get_name()));
            }

            if(!macro_expr->scope_id.has_value())
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' has no scope.",
                    macro_expr->get_name()));
            }
        }
        else
        {
            // Expand imported macro.
            auto* m = macro_env.get_macro(
              macro_expr->get_name(),
              macro_expr->get_namespace_path());

            if(!expand_token_macro(m, macro_expr))
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' is not supported by token expansion.",
                    macro_expr->get_name()));
            }

            if(!macro_expr->scope_id.has_value())
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' has no scope.",
                    macro_expr->get_name()));
            }

            // Resolve unqualified calls relative to the imported macro's defining
            // module and register referenced modules for import resolution.
            macro_expr->expansion->visit_nodes(
              [&macro_expr, &co_ctx](expression& e) -> void
              {
                  if(e.is_macro_invocation() || e.is_call_expression())
                  {
                      if(e.get_id() != ast::node_identifier::namespace_access_expression
                         && !e.get_namespace_path().has_value())
                      {
                          e.set_namespace(macro_expr->get_namespace());
                      }
                  }

                  if(e.get_id() == ast::node_identifier::namespace_access_expression)
                  {
                      co_ctx.declare_external(
                        e.as_named_expression()->get_name(),
                        sema::symbol_type::module_,
                        e.get_location());
                  }

                  auto ns = e.get_namespace_path();
                  if(ns.has_value())
                  {
                      co_ctx.declare_external(
                        ns.value(),
                        sema::symbol_type::module_,
                        e.get_location());
                  }
              },
              true,
              false);
        }

        co_ctx.push_scope(macro_expr->scope_id.value());
        macro_expr->expansion->collect_names(co_ctx);
        co_ctx.pop_scope();

        macro_expr->expansion->resolve_names(rs_ctx);

        ++macro_expansion_count;
    };

    visit_nodes(
      macro_expansion_visitor,
      true,  /* visit this node */
      false, /* pre-order traversal */
      filter_macros);

    return macro_expansion_count != 0;
}

void expression::expand_late_macros(
  co::context& co_ctx,
  rs::context& rs_ctx,
  ty::context& type_ctx,
  sema::env& env,
  cg::context* codegen_ctx,
  macro::env* macro_env,
  syntax_context_table* syntax_contexts)
{
    // Post-order traversal that fills in format! string-building
    // expansions in place.
    std::function<void(expression&)> visit = [&](expression& e)
    {
        // Recurse into children first
        for(auto* child: e.get_children())
            visit(*child);

        // The format! placeholder node (a format_macro_expression) is
        // created as the macro_invocation's expansion during the early
        // expand_macros pass, so it is reachable here as a child. Fill
        // its string-building expansion in place, then bind it.
        if(e.get_id() == node_identifier::format_macro_expression)
        {
            auto* macro_expr = static_cast<format_macro_expression*>(&e);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            macro_expr->expand_late_macros(type_ctx, env);

            if(!macro_expr->scope_id.has_value())
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  "Macro 'format!' has no scope.");
            }

            // Collect names first so newly parsed nodes receive scope_id.
            co_ctx.push_scope(macro_expr->scope_id.value());
            macro_expr->collect_names(co_ctx);
            co_ctx.pop_scope();
            macro_expr->resolve_names(rs_ctx);

            // Expand any macro invocations produced by the expansion (e.g. format args that were macros).
            if(codegen_ctx && macro_env && syntax_contexts && macro_expr->expansion)
            {
                while(macro_expr->expansion->expand_macros(
                  co_ctx,
                  rs_ctx,
                  *codegen_ctx,
                  *macro_env,
                  *syntax_contexts))
                {
                }

                co_ctx.push_scope(macro_expr->scope_id.value());
                macro_expr->collect_names(co_ctx);
                co_ctx.pop_scope();
                macro_expr->resolve_names(rs_ctx);
            }

            (void)macro_expr->type_check(type_ctx, env);
        }
    };

    visit(*this);
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

std::unique_ptr<cg::rvalue> macro_invocation::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    // Code generation for macros.
    if(!expansion)
    {
        throw cg::codegen_error(loc, "Macro was not expanded.");
    }

    auto return_type = expansion->emit_rvalue(ctx, result_used);
    if(index_expr)
    {
        // evaluate the index expression.
        index_expr->emit_rvalue(ctx);

        auto type = ctx.deref(return_type->get_type());
        ctx.generate_load(
          cg::lvalue{
            type,
            cg::array_location_info{}});
        return std::make_unique<cg::rvalue>(type);
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

std::optional<ty::type_id> macro_invocation::type_check(
  ty::context& ctx,
  sema::env& env)
{
    // Type checking for macros.
    if(!expansion)
    {
        throw cg::codegen_error(loc, "Macro was not expanded.");
    }

    auto return_type = expansion->type_check(ctx, env);

    if(index_expr)
    {
        if(!return_type.has_value())
        {
            throw ty::type_error(
              index_expr->get_location(),
              "Indexing into expression without type.");
        }

        auto v = index_expr->type_check(ctx, env);
        if(!v.has_value())
        {
            throw ty::type_error(
              index_expr->get_location(),
              "Index expression has no type.");
        }
        if(!ctx.are_types_compatible(ctx.get_i32_type(), v.value()))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Expected <integer> for array element access, got '{}'.",
                ctx.to_string(v.value())));
        }

        if(!ctx.is_array(return_type.value()))
        {
            throw ty::type_error(loc, "Cannot use subscript on non-array type.");
        }

        expr_type = ctx.array_element_type(return_type.value());
    }
    else
    {
        expr_type = return_type;
    }

    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string macro_invocation::to_string() const
{
    std::string ret = std::format(
      "MacroInvocation(callee={}, exprs=(", get_name());

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

std::string macro_invocation::get_qualified_callee_name() const
{
    std::optional<std::string> path = get_namespace_path();
    if(!path.has_value())
    {
        return name.s;
    }
    return name::qualified_name(
      path.value(),
      name.s);
}

/*
 * macro_branch.
 *
 * Branch bodies are captured as tokens and parsed temporarily for definition-
 * time semantic validation.
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
    ar & argument_separators;
    ar & argument_opening_delimiter;
    ar & argument_closing_delimiter;
    ar & body_tokens;
    ar & body_opening_delimiter;
    ar & body_closing_delimiter;
}

void macro_branch::generate_code([[maybe_unused]] cg::context& ctx) const
{
}

std::unique_ptr<cg::rvalue> macro_branch::emit_rvalue(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    throw cg::codegen_error(loc, "Macro branch cannot be emitted.");
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
          arg_name,
          canonical_name,
          sema::symbol_type::macro_argument,
          sema::symbol_id::invalid,
          false);
    }

    validation_body = parse_macro_branch_body(*this);
    validation_body->collect_names(ctx, false);
    ctx.pop_scope();
}

std::optional<ty::type_id> macro_branch::type_check(
  ty::context& ctx,
  sema::env& env)
{
    return validation_body
             ? validation_body->type_check(ctx, env)
             : std::nullopt;
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
      validation_body
        ? validation_body->to_string()
        : parse_macro_branch_body(*this)->to_string());
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
    // no-op.
}

void macro_expression_list::generate_code(
  [[maybe_unused]] cg::context& ctx) const
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
      name,
      canonical_name,
      sema::symbol_type::macro,
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

void macro_expression::generate_code(
  [[maybe_unused]] cg::context& ctx) const
{
    // empty, as macros don't generate code.
}

std::optional<ty::type_id> macro_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    referenced_modules.clear();

    auto validate_references = [this, &env](expression& e) -> void
    {
        std::optional<std::string> namespace_path{std::nullopt};

        if(e.get_id() == node_identifier::namespace_access_expression)
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

            auto module_id = env.get_symbol_id(
              namespace_path.value(),
              sema::symbol_type::module_,
              env.global_scope_id);
            if(!module_id.has_value())
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "Unresolved import '{}',",
                    namespace_path.value()));
            }

            if(!env.transitive_imports.contains(module_id.value()))
            {
                referenced_modules.insert(namespace_path.value());
            }
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
    };

    visit_nodes(
      validate_references,
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
    if(!symbol_id.has_value())
    {
        throw std::runtime_error(
          std::format(
            "{}: Macro '{}' has no symbol id.",
            ::slang::to_string(get_location()),
            get_qualified_name()));
    }

    std::vector<std::pair<std::string, module_::directive_descriptor>> attributes;

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

    // Preserve each branch's capture list independently. Branches may have
    // different arities and may use different parameter names.
    std::vector<module_::macro_branch_descriptor> branch_descriptors;
    branch_descriptors.reserve(branches.size());
    for(const auto& branch: branches)
    {
        module_::macro_branch_descriptor descriptor;
        descriptor.args_end_with_list = branch->ends_with_list_capture();
        descriptor.separators = branch->get_argument_separators();

        // Convert branch args to descriptor params.
        for(const auto& [arg_name, type_name]: branch->args)
            descriptor.params.emplace_back(module_::macro_parameter{
              .name = arg_name.s,
              .fragment = module_::macro_fragment_kind::expression});

        if(!branch->get_body_tokens().has_value())
        {
            throw parser_error(
              token{
                "=>",
                branch->get_location(),
                token_type::delimiter},
              "Macro branch body tokens are incomplete.");
        }
        const auto body_tokens = *branch->get_body_tokens();
        descriptor.body = module_::macro_body_descriptor{
          .tokens = body_tokens,
          .kind = get_token_body_kind(body_tokens, branch->get_location())};

        // Local macro: definition context is already in this module's
        // syntax_context_table. Mark it as root for the importer.
        descriptor.definition_context = root_syntax_context;

        branch_descriptors.emplace_back(std::move(descriptor));
    }

    // Create descriptor and attach branch metadata.
    module_::macro_descriptor desc;
    desc.directives = std::move(attributes);
    desc.branches = std::move(branch_descriptors);
    macro_env.add_macro(
      name.s,
      desc,
      get_namespace_path(),
      name.context);
}

}    // namespace slang::ast
