/**
 * slang - a simple scripting language.
 *
 * LSP completion — struct member completion and visible symbol completion.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <charconv>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "compiler/ast/ast.h"
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
#include "json_rpc.h"
#include "completion.h"
#include "utils.h"

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

constexpr std::string_view completion_marker = "__slang_lsp_completion__";

bool is_ident_char(char c)
{
    return (c >= 'a' && c <= 'z')
           || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9')
           || c == '_';
}

/** Convert a 0-based (line, character) LSP position to a byte offset in `source`. */
std::size_t position_to_offset(
  const std::string& source,
  lsp_position pos)
{
    std::size_t line = 0;
    std::size_t i = 0;
    while(i < source.size() && line < pos.line)
    {
        if(source[i] == '\n')
        {
            ++line;
        }
        ++i;
    }

    if(line < pos.line)
    {
        return source.size();
    }

    std::size_t col = 0;
    while(i < source.size() && col < pos.character && source[i] != '\n' && source[i] != '\r')
    {
        ++i;
        ++col;
    }

    return i;
}

struct member_access_context
{
    bool is_member_access{false};
    std::size_t ident_start{0};
    std::size_t ident_end{0};
    std::string prefix;
};

member_access_context detect_member_access(
  const std::string& source,
  std::size_t offset)
{
    member_access_context ctx;

    std::size_t start = offset;
    while(start > 0 && is_ident_char(source[start - 1]))
    {
        --start;
    }

    std::size_t end = offset;
    while(end < source.size() && is_ident_char(source[end]))
    {
        ++end;
    }

    ctx.ident_start = start;
    ctx.ident_end = end;
    ctx.prefix = source.substr(start, offset - start);

    // Check whether there is a '.' immediately before the identifier (allowing horizontal whitespace).
    std::size_t dot_check = start;
    while(dot_check > 0 && (source[dot_check - 1] == ' ' || source[dot_check - 1] == '\t'))
    {
        --dot_check;
    }

    if(dot_check > 0 && source[dot_check - 1] == '.')
    {
        // Guard against '..' if ever present.
        if(dot_check < 2 || source[dot_check - 2] != '.')
        {
            ctx.is_member_access = true;
        }
    }

    return ctx;
}

void configure_file_manager(
  slang::file_manager& file_manager,
  const std::string& uri,
  const std::vector<std::string>& search_paths)
{
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
}

std::shared_ptr<ast::expression> try_parse_source(const std::string& source)
{
    try
    {
        slang::token_stream tokens = slang::lexer::lex(source);
        slang::parser parser;
        parser.parse(std::move(tokens));
        return parser.get_ast();
    }
    catch(const std::exception&)
    {
        return nullptr;
    }
}

const ast::access_expression* find_completion_access(
  const ast::expression* expr,
  std::string_view marker_name)
{
    if(!expr)
    {
        return nullptr;
    }

    if(expr->get_id() == ast::node_identifier::access_expression)
    {
        const auto* access = expr->as_access_expression();
        const auto* rhs = access->get_right_expression();
        if(rhs && rhs->get_id() == ast::node_identifier::variable_reference_expression)
        {
            const auto* var_ref = static_cast<const ast::variable_reference_expression*>(rhs);
            if(var_ref->get_name() == marker_name)
            {
                return access;
            }
        }
        if(const auto* found = find_completion_access(access->get_left_expression(), marker_name))
        {
            return found;
        }
        if(const auto* found = find_completion_access(access->get_right_expression(), marker_name))
        {
            return found;
        }
    }

    for(const ast::expression* child: expr->get_children())
    {
        if(const auto* found = find_completion_access(child, marker_name))
        {
            return found;
        }
    }

    return nullptr;
}

bool contains_expression(
  const ast::expression* root,
  const ast::expression* target)
{
    if(!root || !target)
    {
        return false;
    }
    if(root == target)
    {
        return true;
    }
    if(root->get_id() == ast::node_identifier::access_expression)
    {
        const auto* access = root->as_access_expression();
        return contains_expression(access->get_left_expression(), target)
               || contains_expression(access->get_right_expression(), target);
    }
    for(const ast::expression* child: root->get_children())
    {
        if(contains_expression(child, target))
        {
            return true;
        }
    }
    return false;
}

std::vector<completion_item> get_member_completions(
  const std::string& uri,
  const std::string& source,
  const member_access_context& ctx,
  const std::vector<std::string>& search_paths)
{
    // Replace the partial member identifier (or empty spot after '.') with a known marker.
    std::string patched_source =
      source.substr(0, ctx.ident_start)
      + std::string{completion_marker}
      + source.substr(ctx.ident_end);

    auto root = try_parse_source(patched_source);
    if(!root)
    {
        // Try inserting a semicolon after the marker in case the user is typing an incomplete statement.
        std::string patched_with_semi =
          source.substr(0, ctx.ident_start)
          + std::string{completion_marker} + ";"
          + source.substr(ctx.ident_end);
        root = try_parse_source(patched_with_semi);
    }

    if(!root)
    {
        return {};
    }

    diag::context diagnostics;
    diagnostics.register_file(uri, patched_source);

    slang::file_manager file_manager;
    configure_file_manager(file_manager, uri, search_paths);

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
    catch(const std::exception&)
    {
        // Expected when type_check hits the completion marker or an earlier error.
    }

    const ast::access_expression* target_access =
      find_completion_access(root.get(), completion_marker);
    if(!target_access || !target_access->get_left_expression())
    {
        return {};
    }

    auto lhs_type = type_context.get_expression_type(*target_access->get_left_expression());
    if(!lhs_type.has_value())
    {
        // If an earlier top-level declaration failed type_check, type-check the enclosing child directly.
        for(ast::expression* child: root->get_children())
        {
            if(contains_expression(child, target_access))
            {
                try
                {
                    child->type_check(type_context, environment);
                }
                catch(const std::exception&)
                {
                }
                break;
            }
        }
        lhs_type = type_context.get_expression_type(*target_access->get_left_expression());
    }

    if(!lhs_type.has_value())
    {
        return {};
    }

    std::vector<completion_item> items;
    if(type_context.is_array(*lhs_type))
    {
        if(ctx.prefix.empty() || std::string_view{"length"}.starts_with(ctx.prefix))
        {
            items.push_back(completion_item{
              .label = "length",
              .kind = "field",
              .detail = "i32"});
        }
        return items;
    }

    if(type_context.is_struct(*lhs_type))
    {
        const ty::struct_info* info = type_context.get_struct_info(*lhs_type);
        if(!info)
        {
            return {};
        }

        for(const auto& field: info->fields)
        {
            if(ctx.prefix.empty() || field.name.starts_with(ctx.prefix))
            {
                std::optional<std::string> detail;
                if(type_context.get_type_info(field.type).has_value())
                {
                    detail = type_context.to_string(field.type);
                }

                items.push_back(completion_item{
                  .label = field.name,
                  .kind = "field",
                  .detail = std::move(detail)});
            }
        }
    }

    return items;
}

std::string symbol_kind_string(sema::symbol_type type)
{
    switch(type)
    {
    case sema::symbol_type::function:
        return "function";
    case sema::symbol_type::constant:
        return "constant";
    case sema::symbol_type::variable:
    case sema::symbol_type::argument:
        return "variable";
    case sema::symbol_type::type:
        return "type";
    case sema::symbol_type::module_:
        return "module";
    case sema::symbol_type::macro:
    case sema::symbol_type::macro_argument:
        return "identifier";
    }
    return "identifier";
}

int completion_kind_code(std::string_view kind)
{
    if(kind == "text")
    {
        return 1;
    }
    if(kind == "method")
    {
        return 2;
    }
    if(kind == "function")
    {
        return 3;
    }
    if(kind == "constructor")
    {
        return 4;
    }
    if(kind == "field")
    {
        return 5;
    }
    if(kind == "variable" || kind == "identifier")
    {
        return 6;
    }
    if(kind == "class" || kind == "type")
    {
        return 7;
    }
    if(kind == "interface")
    {
        return 8;
    }
    if(kind == "module")
    {
        return 9;
    }
    if(kind == "property")
    {
        return 10;
    }
    if(kind == "constant")
    {
        return 21;
    }
    if(kind == "struct")
    {
        return 22;
    }
    return 6;
}

}    // namespace

std::vector<completion_item> get_completions(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  const std::vector<std::string>& search_paths)
{
    if(source.empty())
    {
        return {};
    }

    try
    {
        const std::size_t offset = position_to_offset(source, pos);
        const auto member_ctx = detect_member_access(source, offset);
        if(member_ctx.is_member_access)
        {
            return get_member_completions(uri, source, member_ctx, search_paths);
        }

        auto root = try_parse_source(source);
        if(!root && member_ctx.ident_end <= source.size())
        {
            std::string with_semi =
              source.substr(0, member_ctx.ident_end)
              + ";"
              + source.substr(member_ctx.ident_end);
            root = try_parse_source(with_semi);
        }
        if(!root)
        {
            return {};
        }

        diag::context diagnostics;
        diagnostics.register_file(uri, source);

        slang::file_manager file_manager;
        configure_file_manager(file_manager, uri, search_paths);

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
        }
        catch(const std::exception&)
        {
            // Partial analysis is still useful for completion.
        }

        std::unordered_set<std::string> seen_names;
        std::vector<completion_item> result;

        for(const auto& [sym_id, sym_info]: environment.symbol_table)
        {
            if(sym_info.name.empty() || sym_info.name == completion_marker)
            {
                continue;
            }
            if(!member_ctx.prefix.empty() && !sym_info.name.starts_with(member_ctx.prefix))
            {
                continue;
            }
            if(seen_names.insert(sym_info.name).second)
            {
                result.push_back(completion_item{
                  .label = sym_info.name,
                  .kind = symbol_kind_string(sym_info.type),
                  .detail = std::nullopt});
            }
        }

        return result;
    }
    catch(...)
    {
        return {};
    }
}

std::string format_completion_result(
  const std::vector<completion_item>& items)
{
    std::string result = "[";
    for(std::size_t i = 0; i < items.size(); ++i)
    {
        if(i > 0)
        {
            result += ",";
        }

        const std::string escaped_label = escape_json_string(items[i].label);
        const int kind_code = completion_kind_code(items[i].kind);

        if(items[i].detail.has_value() && !items[i].detail->empty())
        {
            const std::string escaped_detail = escape_json_string(*items[i].detail);
            result += std::format(
              "{{\"label\":\"{}\",\"kind\":{},\"detail\":\"{}\"}}",
              escaped_label,
              kind_code,
              escaped_detail);
        }
        else
        {
            result += std::format(
              "{{\"label\":\"{}\",\"kind\":{}}}",
              escaped_label,
              kind_code);
        }
    }
    result += "]";
    return result;
}

}    // namespace slang::lsp
