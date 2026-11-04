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
#include "compiler/loader.h"
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
namespace ld = slang::loader;

namespace slang::ast
{

/*
 * expression.
 */

bool expression::expand_macros(
  co::context& co_ctx,
  rs::context& rs_ctx,
  cg::context& codegen_ctx,
  macro::env& macro_env,
  syntax_context_table& syntax_contexts,
  const std::vector<expression*>& macro_asts)
{
    std::size_t macro_expansion_count{0};

    // TODO(macro-migration): Fold namespace fixup into the expansion
    // pipeline and remove this bookkeeping.
    std::vector<ast::macro_invocation*> expanded_imported_macros;

    auto filter_macros = [](const expression& e) -> bool
    {
        return !e.is_macro_expression();
    };

    // TODO(macro-migration): Remove the AST lookup and fallback once all
    // local macro shapes use token expansion.
    // Shared token-stream expansion helper.
    auto try_token_expansion =
      [&](const macro::macro* descriptor_macro, macro_invocation* expr) -> bool
    {
        // format! uses a dedicated late-expansion path with type-aware
        // format string evaluation; skip token expansion for it.
        if(expr->get_name() == "format!")
        {
            return false;
        }

        if(!descriptor_macro->supports_token_expansion(
              expr->get_argument_tokens()))
        {
            return false;
        }
        try
        {
            auto expanded_tokens = descriptor_macro->expand_macro(
              expr->get_argument_tokens(),
              expr->get_name_token().context,
              syntax_contexts,
              expr->get_location(),
              &rs_ctx.get_type_ctx());
            expr->set_expansion(
              parser::parse_expression(std::move(expanded_tokens)));
            return true;
        }
        catch(const macro::macro_error&)
        {
            // TODO(macro-migration): Remove this fallback once token
            // expansion reports complete, authoritative results.
        }
        catch(const syntax_error&)
        {
            // TODO(macro-migration): Remove this fallback once token
            // expansion reports complete, authoritative results.
        }
        return false;
    };

    // Replace macro nodes by their token or legacy AST expansion.
    auto macro_expansion_visitor =
      [&co_ctx,
       &rs_ctx,
       &codegen_ctx,
       &macro_env,
       &syntax_contexts,
       &macro_asts,
       &macro_expansion_count,
       &expanded_imported_macros,
       &try_token_expansion](expression& e)
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
                macro_expr->get_exprs()));

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

                  return macro_expr->get_name() == m->as_named_expression()->get_name();
              });

            if(it == macro_asts.cend())
            {
                throw cg::codegen_error(
                  macro_expr->get_location(),
                  std::format(
                    "Macro '{}' not found.",
                    macro_expr->get_name()));
            }

            auto* descriptor_macro = macro_env.get_macro(
              macro_expr->get_name(),
              std::nullopt);
            if(!try_token_expansion(descriptor_macro, macro_expr))
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

            if(!try_token_expansion(m, macro_expr))
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

            // Adjust local namespaces for macro invocations and calls inside the expansion
            // so they resolve in the imported macro's defining module. Also promote any
            // referenced transitive modules to external so the expansion can resolve them.
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

    for(auto& macro_expr: expanded_imported_macros)
    {
        // adjust local namespaces and transitive import names.
        macro_expr->expansion->visit_nodes(
          [&macro_expr](expression& e) -> void
          {
              if(e.is_macro_invocation())
              {
                  auto macro_namespace = macro_expr->get_namespace_path();
                  if(!macro_namespace.has_value())
                  {
                      throw cg::codegen_error(
                        macro_expr->get_location(),
                        std::format(
                          "Macro '{}' has no namespace path.",
                          macro_expr->get_name()));
                  }

                  if(e.get_id() != ast::node_identifier::namespace_access_expression
                     && !e.get_namespace_path().has_value())
                  {
                      // Set the namespace to the import's name (stored in macro_expr).
                      e.set_namespace(macro_expr->get_namespace());
                  }
              }
          },
          true,
          false);
    }

    return macro_expansion_count != 0;
}

void expression::expand_late_macros(
  co::context& co_ctx,
  rs::context& rs_ctx,
  ty::context& type_ctx,
  sema::env& env)
{
    // Post-order traversal that fills in format! string-building
    // expansions in place.
    std::function<void(expression&)> visit = [&](expression& e)
    {
        // Recurse into children first
        for (auto* child : e.get_children())
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

            co_ctx.push_scope(macro_expr->scope_id.value());
            macro_expr->collect_names(co_ctx);
            co_ctx.pop_scope();
            macro_expr->resolve_names(rs_ctx);
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
 * TODO(macro-migration): This AST branch implementation is the legacy macro
 * representation. Remove its AST expansion, serialization, and semantic
 * traversal once token descriptors are complete.
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

void macro_branch::generate_code(
  cg::context& ctx) const
{
    body->generate_code(ctx);
}

std::unique_ptr<cg::rvalue> macro_branch::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    return body->emit_rvalue(ctx, result_used);
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

    body->collect_names(ctx, false);

    ctx.pop_scope();
}

std::optional<ty::type_id> macro_branch::type_check(
  ty::context& ctx,
  sema::env& env)
{
    return body->type_check(ctx, env);
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
  [[maybe_unused]] ty::context& ctx,
  sema::env& env)
{
    // Check that all necessary imports exist in the type context.

    visit_nodes(
      [this, &env](expression& e) -> void
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

              if(!env.get_symbol_id(
                       namespace_path.value(),
                       sema::symbol_type::module_,
                       env.global_scope_id)
                    .has_value())
              {
                  throw ty::type_error(
                    loc,
                    std::format(
                      "Unresolved import '{}',",
                      namespace_path.value()));
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
    if(!symbol_id.has_value())
    {
        throw std::runtime_error(
          std::format(
            "{}: Macro '{}' has no symbol id.",
            ::slang::to_string(get_location()),
            get_qualified_name()));
    }

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

        // A token body is supported for two shapes:
        //   - a single-expression body (exactly one trailing ';'), and
        //   - a block body: a statement list terminated by ';' (one or more
        //     ';'), rebuilt by the parse-back as a block whose value is its
        //     tail expression.
        // TODO(macro-migration): Validate the supported body shape directly
        // from tokens, then remove this dependency on the legacy AST body.
        bool token_body_supported = false;
        std::size_t semicolon_count = 0;
        if(branch->get_body_tokens().has_value()
           && !branch->get_body_tokens()->empty()
           && branch->get_body_tokens()->back().s == ";")
        {
            for(const auto& tok: *branch->get_body_tokens())
                if(tok.s == ";")
                    ++semicolon_count;
            // One ';' is a single-expression body; more than one is a
            // block/statement-list body. Both are token-expandable.
            token_body_supported = (semicolon_count >= 1);
        }

        if(token_body_supported)
        {
            descriptor.body = branch->get_body_tokens();
            descriptor.capability =
              module_::macro_expansion_capability::token_stream;
            // body_is_expression distinguishes the two shapes for the
            // expander: true -> drop the trailing ';' and parse as a single
            // expression; false -> keep the ';' and parse as a block.
            descriptor.body_is_expression = (semicolon_count == 1);
        }
        else
        {
            // Unsupported branch: leave without a token body. It will throw
            // at expansion time with a clear message.
        }

        // Local macro: definition context is already in this module's
        // syntax_context_table. Mark it as root for the importer.
        descriptor.definition_context = root_syntax_context;

        branch_descriptors.emplace_back(std::move(descriptor));
    }

    // Detect format! and inject a placeholder body for token-stream expansion.
    if(name.s == "format!")
    {
        branch_descriptors.clear();
        module_::macro_branch_descriptor format_branch;
        format_branch.params = {{
            {"format_str", module_::macro_fragment_kind::expression}
        }};
        format_branch.body_is_expression = true;
        format_branch.args_end_with_list = true;
        format_branch.capability = module_::macro_expansion_capability::token_stream;
        format_branch.body = token_stream{std::vector<token>{
            token{"__format_builtin__", loc, token_type::macro_identifier},
            token{";", loc, token_type::delimiter}
        }};
        format_branch.definition_context = root_syntax_context;
        branch_descriptors.emplace_back(std::move(format_branch));
    }

    // Create descriptor and attach branch metadata. Unsupported branches
    // will throw at expansion time.
    module_::macro_descriptor desc;
    desc.directives = std::move(attributes);
    desc.branches = std::move(branch_descriptors);
    macro_env.add_macro(
      name.s,
      desc,
      get_namespace_path(),
      name.context);
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
            macro_expr->as_named_expression()->get_name(),
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
    // TODO(macro-migration): Remove this legacy AST expander after all local
    // and imported macro shapes have equivalent token-stream coverage.
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
            bool is_global_constant =
              ctx.get_sema_env().get_symbol_id(
                                  expr->name.s,
                                  sema::symbol_type::constant,
                                  ctx.get_sema_env().global_scope_id)    // constants are always global.
                .has_value();

            if(original_local_names.contains(expr->name.s)
               || !is_global_constant)
            {
                expr->name.s = make_local_name(invocation_id, expr->name.s);
            }
        }
        else if(e.is_struct_member_access())
        {
            // rename macro variable.
            expression* expr = e.as_access_expression()->get_left_expression();
            while(expr != nullptr && expr->is_struct_member_access())
            {
                expr = expr->as_access_expression()->get_left_expression();
            }

            if(expr == nullptr || !expr->is_named_expression())
            {
                throw cg::codegen_error(
                  e.get_location(),
                  "Could not get variable name for struct access.");
            }

            auto* expr_named = expr->as_named_expression();
            if(original_local_names.contains(expr_named->name.s))
            {
                expr_named->name.s = make_local_name(invocation_id, expr_named->name.s);
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
                  return ref_expr->get_name() == p.first;
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
