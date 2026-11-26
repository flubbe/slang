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
#include <charconv>
#include <filesystem>
#include <format>
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

    for(const ast::expression* child: expr->get_children())
    {
        if(const auto* found = find_expression_at(child, pos))
        {
            return found;
        }
    }

    return nullptr;
}

std::optional<fs::path> path_from_file_uri(
  std::string_view uri)
{
    constexpr std::string_view prefix{"file://"};
    if(!uri.starts_with(prefix))
    {
        return std::nullopt;
    }

    std::string path{uri.substr(prefix.size())};
    if(path.starts_with("localhost/"))
    {
        path.erase(
          0,
          std::string_view{"localhost"}.size());
    }

    std::string decoded;
    decoded.reserve(path.size());
    for(std::size_t i = 0; i < path.size(); ++i)
    {
        if(path[i] != '%')
        {
            decoded += path[i];
            continue;
        }

        if(i + 2 >= path.size())
        {
            return std::nullopt;
        }

        unsigned int value = 0;
        const char* first = path.data() + i + 1;
        const char* last = first + 2;
        const auto [end, error] = std::from_chars(first, last, value, 16);
        if(error != std::errc{} || end != last)
        {
            return std::nullopt;
        }

        decoded += static_cast<char>(value);
        i += 2;
    }

#if defined(_WIN32)
    // Remove leading slash from URI-decoded Windows drive-letter paths.
    if(decoded.size() >= 3
       && decoded[0] == '/'
       && decoded[2] == ':')
    {
        decoded.erase(0, 1);
    }
#endif

    return fs::path{decoded};
}

void add_search_path(
  slang::file_manager& file_manager,
  const fs::path& path)
{
    std::error_code error;
    if(!fs::is_directory(path, error)
       || error)
    {
        return;
    }

    const fs::path canonical_path = fs::canonical(path, error);
    if(!error)
    {
        file_manager.add_search_path(canonical_path);
    }
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
    std::error_code error;
    const fs::path current_path = fs::current_path(error);
    if(!error)
    {
        add_search_path(file_manager, current_path);
        add_search_path(file_manager, current_path / "lang");
    }
    if(const auto source_path = path_from_file_uri(uri))
    {
        add_search_path(file_manager, source_path->parent_path());
    }

    for(const auto& path: search_paths)
    {
        add_search_path(file_manager, path);
    }

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
