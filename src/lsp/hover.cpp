/**
 * slang - a simple scripting language.
 *
 * LSP hover information — find type and definition for a symbol at a cursor position.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "compiler/collect.h"
#include "compiler/constant.h"
#include "compiler/diagnostics.h"
#include "compiler/lexer.h"
#include "compiler/loader.h"
#include "compiler/macro.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/sema.h"
#include "compiler/typing.h"
#include "filemanager.h"
#include "lsp/json_rpc.h"
#include "shared/module.h"
#include "hover.h"

namespace ast = slang::ast;
namespace co = slang::collect;
namespace const_ = slang::const_;
namespace diag = slang::compiler::diagnostics;
namespace ld = slang::loader;
namespace macro = slang::macro;
namespace rs = slang::resolve;
namespace sema = slang::sema;
namespace ty = slang::typing;
namespace fs = std::filesystem;

namespace slang::lsp
{

namespace
{

bool token_position_matches(
  const slang::token& tok,
  lsp_position pos)
{
    if(tok.s.empty())
    {
        return false;
    }

    // Lexer locations are 1-based; LSP positions are 0-based.
    const std::size_t line = (tok.location.line > 0) ? (tok.location.line - 1) : 0;
    const std::size_t start_col = (tok.location.col > 0) ? (tok.location.col - 1) : 0;
    const std::size_t end_col = start_col + tok.s.size();

    return line == pos.line
           && start_col <= pos.character
           && pos.character < end_col;
}

const ast::expression* find_expression_at(
  const ast::expression* expr,
  lsp_position pos)
{
    if(!expr)
    {
        return nullptr;
    }

    if(expr->get_id() == ast::node_identifier::function_expression)
    {
        const auto* function = static_cast<const ast::function_expression*>(expr);
        const auto* prototype = function->get_prototype();
        if(prototype
           && token_position_matches(
             prototype->get_name_token(),
             pos))
        {
            return expr;
        }
    }

    if(expr->is_named_expression())
    {
        const auto* named = expr->as_named_expression();
        if(named
           && token_position_matches(
             named->get_name_token(),
             pos))
        {
            return expr;
        }
    }

    if(expr->is_call_expression())
    {
        const auto* call = static_cast<const ast::call_expression*>(expr);
        if(token_position_matches(
             call->get_callee(),
             pos))
        {
            return expr;
        }
    }

    if(expr->get_id() == ast::node_identifier::access_expression)
    {
        const auto* access = expr->as_access_expression();
        if(const auto* found = find_expression_at(access->get_left_expression(), pos))
        {
            return found;
        }
        const auto* rhs = access->get_right_expression();
        if(rhs && rhs->is_named_expression())
        {
            const auto* named = rhs->as_named_expression();
            if(named && token_position_matches(named->get_name_token(), pos))
            {
                return expr;
            }
        }
    }

    for(const ast::expression* child: expr->get_children())
    {
        if(const auto* found = find_expression_at(child, pos))
        {
            return found;
        }
    }

    return nullptr;
}

std::optional<std::string> format_const_value(
  const const_::const_info& info)
{
    if(std::holds_alternative<std::int64_t>(info.value))
    {
        return std::to_string(std::get<std::int64_t>(info.value));
    }
    if(std::holds_alternative<double>(info.value))
    {
        return std::format("{}", std::get<double>(info.value));
    }
    if(std::holds_alternative<std::string>(info.value))
    {
        return std::format("\"{}\"", std::get<std::string>(info.value));
    }
    return std::nullopt;
}

const ast::function_expression* find_function_in_ast(
  const ast::expression* expr,
  std::string_view name)
{
    if(!expr)
    {
        return nullptr;
    }

    if(expr->get_id() == ast::node_identifier::function_expression)
    {
        const auto* fn = static_cast<const ast::function_expression*>(expr);
        if(fn->get_prototype() && fn->get_prototype()->get_name() == name)
        {
            return fn;
        }
    }

    for(const ast::expression* child: expr->get_children())
    {
        if(const auto* found = find_function_in_ast(child, name))
        {
            return found;
        }
    }

    return nullptr;
}

const ast::expression* find_constant_in_ast(
  const ast::expression* expr,
  std::string_view name)
{
    if(!expr)
    {
        return nullptr;
    }

    if(expr->get_id() == ast::node_identifier::constant_declaration_expression)
    {
        const auto* named = expr->as_named_expression();
        if(named && named->get_name() == name)
        {
            return expr;
        }
    }

    for(const ast::expression* child: expr->get_children())
    {
        if(const auto* found = find_constant_in_ast(child, name))
        {
            return found;
        }
    }

    return nullptr;
}

std::string display_type(
  ty::type_id id,
  const ty::context& type_context)
{
    if(!type_context.get_type_info(id).has_value())
    {
        return "<unknown type>";
    }

    return type_context.to_string(id);
}

std::string function_signature(
  const ast::function_expression& function,
  const ty::context& type_context)
{
    const auto* prototype = function.get_prototype();
    if(!prototype)
    {
        return {};
    }

    const auto& args = prototype->get_args();
    const auto& arg_type_ids = prototype->get_arg_type_ids();
    if(args.size() != arg_type_ids.size())
    {
        // Internal invariant violation; gracefully omit the signature.
        return {};
    }

    std::string signature = std::format(
      "fn {}(",
      prototype->get_name());

    for(std::size_t i = 0; i < args.size(); ++i)
    {
        if(i > 0)
        {
            signature += ", ";
        }
        signature += std::format(
          "{}: {}",
          std::get<0>(args[i]).s,
          display_type(arg_type_ids[i], type_context));
    }
    signature += std::format(
      ") -> {}",
      display_type(prototype->get_return_type_id(), type_context));
    return signature;
}

std::string imported_type_name(
  const module_::variable_type& type,
  const sema::symbol_info& symbol,
  const sema::env& environment,
  const ty::context& type_context)
{
    std::string name = type.base_type();
    if(!name.contains("::") && !type_context.has_type(name))
    {
        const auto module = environment.symbol_table.find(
          symbol.declaring_module);
        if(module != environment.symbol_table.end()
           && !module->second.qualified_name.empty())
        {
            name = std::format(
              "{}::{}",
              module->second.qualified_name,
              name);
        }
    }

    if(type.is_array())
    {
        name += "[]";
    }

    return name;
}

std::optional<std::string> imported_function_signature(
  const sema::symbol_info& symbol,
  const sema::env& environment,
  const ty::context& type_context)
{
    if(!symbol.reference.has_value()
       || !std::holds_alternative<const module_::exported_symbol*>(*symbol.reference))
    {
        return std::nullopt;
    }

    const auto* exported = std::get<const module_::exported_symbol*>(*symbol.reference);
    if(!exported || exported->type != module_::symbol_type::function
       || !std::holds_alternative<module_::function_descriptor>(exported->desc))
    {
        return std::nullopt;
    }

    const auto& descriptor = std::get<module_::function_descriptor>(exported->desc);
    std::string signature = std::format("fn {}(", exported->name);
    for(std::size_t i = 0; i < descriptor.signature.arg_types.size(); ++i)
    {
        if(i > 0)
        {
            signature += ", ";
        }

        signature += std::format(
          "arg{}: {}",
          i + 1,
          imported_type_name(
            descriptor.signature.arg_types[i],
            symbol,
            environment,
            type_context));
    }
    signature += std::format(
      ") -> {}",
      imported_type_name(
        descriptor.signature.return_type,
        symbol,
        environment,
        type_context));
    return signature;
}

std::optional<hover_item> find_type_hover(
  const slang::token_stream& tokens,
  lsp_position pos,
  const ty::context& type_context)
{
    const auto token = std::ranges::find_if(
      tokens.data(),
      [pos](const slang::token& candidate)
      {
          return token_position_matches(candidate, pos);
      });
    if(token == tokens.data().end()
       || token->type != slang::token_type::identifier)
    {
        return std::nullopt;
    }

    // Resolve the type under the cursor.  First try the bare identifier
    // (e.g. "i32"), then fall back to a qualified name from the known type
    // names (e.g. "std::i32s").  An imported type may be a known name
    // without a resolvable type id, so track whether a name matched
    // separately from whether a type id was found.
    std::string resolved_name = token->s;
    auto type_id = type_context.get_type(resolved_name);
    bool matched = type_id.has_value();
    if(!matched)
    {
        const std::string suffix = std::format("::{}", token->s);
        for(const auto& candidate: type_context.get_type_names())
        {
            if(candidate.ends_with(suffix))
            {
                resolved_name = candidate;
                type_id = type_context.get_type(candidate);
                matched = true;
                break;
            }
        }
    }
    if(!matched)
    {
        return std::nullopt;
    }

    return hover_item{
      .label = std::format("type {}", resolved_name),
      .detail = type_id.has_value() ? type_context.to_string(*type_id) : std::string{},
      .kind = "type"};
}

}    // namespace

std::optional<hover_item> find_hover_info(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  const std::vector<std::string>& search_paths)
{
    if(source.empty())
    {
        return std::nullopt;
    }

    diag::context diagnostics;
    diagnostics.register_file(uri, source);

    slang::token_stream tokens;
    std::shared_ptr<ast::expression> root;
    try
    {
        tokens = slang::lexer::lex(source);
        auto parse_tokens = tokens;
        slang::parser parser;
        parser.parse(std::move(parse_tokens));
        root = parser.get_ast();
    }
    catch(const std::exception& error)
    {
        std::println(stderr, "[lsp] Could not parse document for hover: {}", error.what());
        return std::nullopt;
    }

    if(!root)
    {
        return std::nullopt;
    }

    slang::file_manager file_manager;
    configure_search_paths(file_manager, uri, search_paths);

    ld::context loader{file_manager};
    sema::env environment;
    const_::env constants;
    macro::env macros;
    ty::context type_context{diagnostics, uri};
    co::context collector{environment};
    rs::context resolver{
      environment,
      constants,
      macros,
      type_context,
      diagnostics,
      uri};

    try
    {
        root->collect_names(collector);
        resolver.resolve_imports(loader);
        root->resolve_names(resolver);
        root->declare_types(type_context, environment);
        root->define_types(type_context);
        root->declare_functions(type_context, environment);
        root->bind_constant_declarations(environment, constants);
        root->type_check(type_context, environment);
        root->evaluate_constant_expressions(type_context, constants);
    }
    catch(const std::exception& analysis_error)
    {
        std::println(
          stderr,
          "[lsp] Incomplete hover analysis for '{}': {}",
          uri,
          analysis_error.what());
    }

    if(const auto* target = find_expression_at(root.get(), pos))
    {
        if(target->get_id() == ast::node_identifier::function_expression)
        {
            const auto* function = static_cast<const ast::function_expression*>(target);
            return hover_item{
              .label = function_signature(*function, type_context),
              .detail = "function",
              .kind = "function"};
        }

        if(target->get_id() == ast::node_identifier::access_expression)
        {
            const auto* access = target->as_access_expression();
            const auto* rhs = access->get_right_expression();
            if(rhs && rhs->is_named_expression())
            {
                const std::string member_name = rhs->as_named_expression()->get_name();
                const auto owner_type = access->get_left_expression()
                                          ? type_context.get_expression_type(*access->get_left_expression())
                                          : std::nullopt;
                const auto member_type = type_context.get_expression_type(*access);

                std::string label = member_name;
                if(owner_type.has_value())
                {
                    label = std::format(
                      "{}.{}",
                      display_type(*owner_type, type_context),
                      member_name);
                }

                return hover_item{
                  .label = std::move(label),
                  .detail = member_type.has_value()
                              ? display_type(*member_type, type_context)
                              : std::string{},
                  .kind = "field"};
            }
        }

        if(target->is_call_expression())
        {
            const auto symbol_id = target->get_symbol_id();
            if(symbol_id.has_value())
            {
                const auto symbol = environment.symbol_table.find(*symbol_id);
                if(symbol != environment.symbol_table.end())
                {
                    if(symbol->second.reference.has_value()
                       && std::holds_alternative<const ast::expression*>(*symbol->second.reference))
                    {
                        const auto* declaration =
                          std::get<const ast::expression*>(*symbol->second.reference);
                        if(declaration
                           && declaration->get_id() == ast::node_identifier::function_expression)
                        {
                            const auto* function =
                              static_cast<const ast::function_expression*>(declaration);
                            return hover_item{
                              .label = function_signature(*function, type_context),
                              .detail = "function",
                              .kind = "function"};
                        }
                    }

                    if(const auto mod = environment.symbol_table.find(symbol->second.declaring_module);
                       mod != environment.symbol_table.end()
                       && !mod->second.qualified_name.empty())
                    {
                        if(const auto sl_path = resolve_module_source_path(file_manager, mod->second.qualified_name))
                        {
                            std::ifstream sl_file(*sl_path, std::ios::binary);
                            std::string mod_source{
                              std::istreambuf_iterator<char>(sl_file),
                              std::istreambuf_iterator<char>()};
                            if(!mod_source.empty())
                            {
                                const std::string mod_uri = file_uri_from_path(*sl_path);
                                diag::context mod_diag;
                                mod_diag.register_file(mod_uri, mod_source);
                                try
                                {
                                    auto mod_tokens = slang::lexer::lex(mod_source);
                                    slang::parser mod_parser;
                                    mod_parser.parse(std::move(mod_tokens));
                                    if(auto mod_root = mod_parser.get_ast())
                                    {
                                        ld::context mod_loader{file_manager};
                                        sema::env mod_env;
                                        const_::env mod_consts;
                                        macro::env mod_macros;
                                        ty::context mod_types{mod_diag, mod_uri};
                                        co::context mod_col{mod_env};
                                        rs::context mod_res{mod_env, mod_consts, mod_macros, mod_types, mod_diag, mod_uri};
                                        mod_root->collect_names(mod_col);
                                        mod_res.resolve_imports(mod_loader);
                                        mod_root->resolve_names(mod_res);
                                        mod_root->declare_types(mod_types, mod_env);
                                        mod_root->define_types(mod_types);
                                        mod_root->declare_functions(mod_types, mod_env);
                                        if(const auto* mod_fn = find_function_in_ast(mod_root.get(), symbol->second.name))
                                        {
                                            return hover_item{
                                              .label = function_signature(*mod_fn, mod_types),
                                              .detail = "function",
                                              .kind = "function",
                                              .definition = lsp_location{
                                                .uri = mod_uri,
                                                .range = lsp_range{}}};
                                        }
                                    }
                                }
                                catch(const std::exception&)
                                {
                                }
                            }
                        }
                    }

                    if(auto signature = imported_function_signature(
                         symbol->second,
                         environment,
                         type_context))
                    {
                        return hover_item{
                          .label = *signature,
                          .detail = "function",
                          .kind = "function"};
                    }
                }
            }
        }

        if(target->is_named_expression())
        {
            const auto* named = target->as_named_expression();
            hover_item item{
              .label = named->get_name(),
              .detail = {},
              .kind = "variable"};

            if(target->get_id() == ast::node_identifier::constant_declaration_expression)
            {
                item.kind = "const";
            }
            else if(target->get_id() == ast::node_identifier::struct_definition_expression)
            {
                item.kind = "struct";
            }
            else if(target->get_id() == ast::node_identifier::variable_declaration_expression)
            {
                item.kind = "let";
            }

            if(const auto symbol_id = target->get_symbol_id())
            {
                const auto symbol = environment.symbol_table.find(*symbol_id);
                if(symbol != environment.symbol_table.end())
                {
                    if(symbol->second.type == sema::symbol_type::constant)
                    {
                        item.kind = "const";
                    }
                    else if(symbol->second.type == sema::symbol_type::type)
                    {
                        item.kind = "type";
                    }
                }
            }

            if(const auto type_id = type_context.get_expression_type(*target))
            {
                item.detail = type_context.to_string(*type_id);
            }

            if(item.kind == "const")
            {
                std::optional<const_::const_info> const_info;
                if(const auto symbol_id = target->get_symbol_id())
                {
                    const_info = constants.get_const_info(*symbol_id);

                    // If this is an imported constant and its .sl source is available, prefer source info.
                    if(!const_info.has_value() || item.detail.empty())
                    {
                        const auto sym = environment.symbol_table.find(*symbol_id);
                        if(sym != environment.symbol_table.end())
                        {
                            const auto mod = environment.symbol_table.find(sym->second.declaring_module);
                            if(mod != environment.symbol_table.end()
                               && !mod->second.qualified_name.empty())
                            {
                                if(const auto sl_path = resolve_module_source_path(file_manager, mod->second.qualified_name))
                                {
                                    std::ifstream sl_file(*sl_path, std::ios::binary);
                                    std::string mod_source{
                                      std::istreambuf_iterator<char>(sl_file),
                                      std::istreambuf_iterator<char>()};
                                    if(!mod_source.empty())
                                    {
                                        const std::string mod_uri = file_uri_from_path(*sl_path);
                                        diag::context mod_diag;
                                        mod_diag.register_file(mod_uri, mod_source);
                                        try
                                        {
                                            auto mod_tokens = slang::lexer::lex(mod_source);
                                            slang::parser mod_parser;
                                            mod_parser.parse(std::move(mod_tokens));
                                            if(auto mod_root = mod_parser.get_ast())
                                            {
                                                ld::context mod_loader{file_manager};
                                                sema::env mod_env;
                                                const_::env mod_consts;
                                                macro::env mod_macros;
                                                ty::context mod_types{mod_diag, mod_uri};
                                                co::context mod_col{mod_env};
                                                rs::context mod_res{mod_env, mod_consts, mod_macros, mod_types, mod_diag, mod_uri};
                                                mod_root->collect_names(mod_col);
                                                mod_res.resolve_imports(mod_loader);
                                                mod_root->resolve_names(mod_res);
                                                mod_root->declare_types(mod_types, mod_env);
                                                mod_root->define_types(mod_types);
                                                mod_root->declare_functions(mod_types, mod_env);
                                                mod_root->bind_constant_declarations(mod_env, mod_consts);
                                                mod_root->type_check(mod_types, mod_env);
                                                mod_root->evaluate_constant_expressions(mod_types, mod_consts);
                                                if(const auto* mod_const = find_constant_in_ast(mod_root.get(), sym->second.name))
                                                {
                                                    if(item.detail.empty())
                                                    {
                                                        if(const auto tid = mod_types.get_expression_type(*mod_const))
                                                        {
                                                            item.detail = mod_types.to_string(*tid);
                                                        }
                                                    }
                                                    if(const auto mod_sid = mod_const->get_symbol_id())
                                                    {
                                                        if(auto info = mod_consts.get_const_info(*mod_sid))
                                                        {
                                                            const_info = info;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        catch(const std::exception&)
                                        {
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if(const_info.has_value())
                {
                    if(const auto formatted = format_const_value(*const_info))
                    {
                        if(item.detail.empty())
                        {
                            item.detail = std::format(
                              "{} = {}",
                              const_::to_string(const_info->type),
                              *formatted);
                        }
                        else
                        {
                            item.detail += std::format(" = {}", *formatted);
                        }
                    }
                }
            }

            return item;
        }
    }

    return find_type_hover(tokens, pos, type_context);
}

std::string format_hover_result(
  const std::optional<hover_item>& item)
{
    if(!item.has_value())
    {
        return "null";
    }

    std::string value = std::format(
      "```slang\n{}\n```",
      item->label);

    if(!item->detail.empty())
    {
        value += std::format(
          "\n\n{}",
          item->detail);
    }

    return std::format(
      "{{\"contents\":{{\"kind\":\"markdown\",\"value\":\"{}\"}}}}",
      escape_json_string(value));
}

}    // namespace slang::lsp
