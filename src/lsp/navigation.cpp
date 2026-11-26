/**
 * slang - a simple scripting language.
 *
 * LSP navigation features: Document Symbols, Go to Definition, Find References.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
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
#include "navigation.h"
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

std::string file_uri_from_path(const std::filesystem::path& path)
{
    std::error_code error;
    const fs::path canonical_path = fs::canonical(path, error);
    const std::string generic = (!error ? canonical_path : path).generic_string();
#if defined(_WIN32)
    return std::format("file:///{}", generic);
#else
    return std::format("file://{}", generic);
#endif
}

void configure_search_paths(
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

std::optional<std::filesystem::path> resolve_module_source_path(
  const slang::file_manager& file_manager,
  std::string_view module_qualified_name)
{
    if(module_qualified_name.empty())
    {
        return std::nullopt;
    }

    std::string rel_path;
    rel_path.reserve(module_qualified_name.size() + 3);
    for(std::size_t i = 0; i < module_qualified_name.size(); ++i)
    {
        if(i + 1 < module_qualified_name.size()
           && module_qualified_name[i] == ':'
           && module_qualified_name[i + 1] == ':')
        {
            rel_path += '/';
            ++i;
        }
        else
        {
            rel_path += module_qualified_name[i];
        }
    }
    rel_path += ".sl";

    try
    {
        const fs::path resolved = file_manager.resolve(fs::path{rel_path});
        std::error_code error;
        if(fs::is_regular_file(resolved, error) && !error)
        {
            return resolved;
        }
    }
    catch(const std::exception&)
    {
    }

    return std::nullopt;
}

static lsp_range range_of_token(const slang::token& tok)
{
    std::size_t line = (tok.location.line > 0) ? (tok.location.line - 1) : 0;
    std::size_t col = (tok.location.col > 0) ? (tok.location.col - 1) : 0;
    std::size_t len = !tok.s.empty() ? tok.s.size() : 1;

    return lsp_range{
      .start = {.line = line, .character = col},
      .end = {.line = line, .character = col + len}};
}

static std::string format_range(const lsp_range& r)
{
    return std::format(
      "{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}}",
      r.start.line,
      r.start.character,
      r.end.line,
      r.end.character);
}

std::vector<document_symbol> compute_document_symbols(const std::string& source)
{
    std::vector<document_symbol> symbols;
    if(source.empty())
    {
        return symbols;
    }

    std::shared_ptr<slang::ast::expression> root;
    try
    {
        auto tokens = slang::lexer::lex(source);
        slang::parser parser;
        parser.parse(std::move(tokens));
        root = parser.get_ast();
    }
    catch(...)
    {
        return symbols;
    }

    if(!root)
    {
        return symbols;
    }

    auto process_node = [&](const slang::ast::expression* expr, auto& self) -> void
    {
        if(!expr)
            return;

        auto id = expr->get_id();
        if(id == slang::ast::node_identifier::translation_unit)
        {
            for(const auto* child: expr->get_children())
            {
                self(child, self);
            }
        }
        else if(id == slang::ast::node_identifier::function_expression)
        {
            const auto* fe = static_cast<const slang::ast::function_expression*>(expr);
            const auto* proto = fe->get_prototype();
            if(proto)
            {
                document_symbol sym{
                  .name = proto->get_name(),
                  .kind = symbol_kind::function,
                  .range = range_of_token(proto->get_name_token()),
                  .selection_range = range_of_token(proto->get_name_token()),
                  .children = {}};

                for(const auto& arg: proto->get_args())
                {
                    const auto& tok = std::get<0>(arg);
                    sym.children.push_back(document_symbol{
                      .name = tok.s,
                      .kind = symbol_kind::variable,
                      .range = range_of_token(tok),
                      .selection_range = range_of_token(tok),
                      .children = {}});
                }

                if(fe->get_body())
                {
                    for(const auto* stmt: fe->get_body()->get_children())
                    {
                        if(stmt && stmt->get_id() == slang::ast::node_identifier::variable_declaration_expression)
                        {
                            const auto* vde = stmt->as_named_expression();
                            if(vde)
                            {
                                sym.children.push_back(document_symbol{
                                  .name = vde->get_name(),
                                  .kind = symbol_kind::variable,
                                  .range = range_of_token(vde->get_name_token()),
                                  .selection_range = range_of_token(vde->get_name_token()),
                                  .children = {}});
                            }
                        }
                    }
                }

                symbols.push_back(std::move(sym));
            }
        }
        else if(id == slang::ast::node_identifier::struct_definition_expression)
        {
            const auto* sde = static_cast<const slang::ast::struct_definition_expression*>(expr);
            document_symbol sym{
              .name = sde->get_name(),
              .kind = symbol_kind::struct_,
              .range = range_of_token(sde->get_name_token()),
              .selection_range = range_of_token(sde->get_name_token()),
              .children = {}};

            for(const auto* member: sde->get_children())
            {
                if(member && member->is_named_expression())
                {
                    const auto* ne = member->as_named_expression();
                    sym.children.push_back(document_symbol{
                      .name = ne->get_name(),
                      .kind = symbol_kind::field,
                      .range = range_of_token(ne->get_name_token()),
                      .selection_range = range_of_token(ne->get_name_token()),
                      .children = {}});
                }
            }

            symbols.push_back(std::move(sym));
        }
        else if(id == slang::ast::node_identifier::variable_declaration_expression)
        {
            const auto* vde = expr->as_named_expression();
            if(vde)
            {
                symbols.push_back(document_symbol{
                  .name = vde->get_name(),
                  .kind = symbol_kind::variable,
                  .range = range_of_token(vde->get_name_token()),
                  .selection_range = range_of_token(vde->get_name_token()),
                  .children = {}});
            }
        }
        else if(id == slang::ast::node_identifier::constant_declaration_expression)
        {
            const auto* cde = expr->as_named_expression();
            if(cde)
            {
                symbols.push_back(document_symbol{
                  .name = cde->get_name(),
                  .kind = symbol_kind::constant,
                  .range = range_of_token(cde->get_name_token()),
                  .selection_range = range_of_token(cde->get_name_token()),
                  .children = {}});
            }
        }
    };

    process_node(root.get(), process_node);
    return symbols;
}

static void serialize_symbol(const document_symbol& sym, std::string& out)
{
    out += std::format(
      "{{\"name\":\"{}\",\"kind\":{},\"range\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}},\"selectionRange\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}}",
      escape_json_string(sym.name),
      static_cast<int>(sym.kind),
      sym.range.start.line,
      sym.range.start.character,
      sym.range.end.line,
      sym.range.end.character,
      sym.selection_range.start.line,
      sym.selection_range.start.character,
      sym.selection_range.end.line,
      sym.selection_range.end.character);

    if(!sym.children.empty())
    {
        out += ",\"children\":[";
        for(std::size_t i = 0; i < sym.children.size(); ++i)
        {
            if(i > 0)
                out += ",";
            serialize_symbol(sym.children[i], out);
        }
        out += "]";
    }
    out += "}";
}

std::string format_document_symbols_result(const std::vector<document_symbol>& symbols)
{
    std::string out = "[";
    for(std::size_t i = 0; i < symbols.size(); ++i)
    {
        if(i > 0)
            out += ",";
        serialize_symbol(symbols[i], out);
    }
    out += "]";
    return out;
}

static const slang::token* find_token_at(const std::vector<slang::token>& tokens, lsp_position pos)
{
    for(const auto& tok: tokens)
    {
        if(tok.location.line == 0)
            continue;

        std::size_t line = tok.location.line - 1;
        std::size_t start_col = (tok.location.col > 0) ? (tok.location.col - 1) : 0;
        std::size_t end_col = start_col + tok.s.size();

        if(line == pos.line && start_col <= pos.character && pos.character <= end_col)
        {
            return &tok;
        }
    }
    return nullptr;
}

std::optional<lsp_location> find_definition(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  const std::vector<std::string>& searchPaths)
{
    if(source.empty())
    {
        return std::nullopt;
    }

    slang::token_stream stream;
    try
    {
        stream = slang::lexer::lex(source);
    }
    catch(...)
    {
        return std::nullopt;
    }

    const auto* tok = find_token_at(stream.data(), pos);
    if(!tok || tok->type != slang::token_type::identifier)
    {
        return std::nullopt;
    }

    const std::string& target_name = tok->s;

    std::shared_ptr<slang::ast::expression> root;
    try
    {
        auto copy_stream = stream;
        slang::parser parser;
        parser.parse(std::move(copy_stream));
        root = parser.get_ast();
    }
    catch(...)
    {
        // Even if parse fails, fall back to searching declaration tokens.
    }

    if(root)
    {
        std::optional<lsp_range> found_range;
        auto search_decl = [&](const slang::ast::expression* expr, auto& self) -> void
        {
            if(!expr || found_range.has_value())
                return;

            auto id = expr->get_id();
            if(id == slang::ast::node_identifier::function_expression)
            {
                const auto* fe = static_cast<const slang::ast::function_expression*>(expr);
                const auto* proto = fe->get_prototype();
                if(proto)
                {
                    if(proto->get_name() == target_name)
                    {
                        found_range = range_of_token(proto->get_name_token());
                        return;
                    }
                    for(const auto& arg: proto->get_args())
                    {
                        const auto& arg_tok = std::get<0>(arg);
                        if(arg_tok.s == target_name)
                        {
                            found_range = range_of_token(arg_tok);
                            return;
                        }
                    }
                }
            }
            else if(id == slang::ast::node_identifier::struct_definition_expression)
            {
                const auto* sde = static_cast<const slang::ast::struct_definition_expression*>(expr);
                if(sde->get_name() == target_name)
                {
                    found_range = range_of_token(sde->get_name_token());
                    return;
                }
                for(const auto* member: sde->get_children())
                {
                    if(member && member->is_named_expression())
                    {
                        const auto* ne = member->as_named_expression();
                        if(ne->get_name() == target_name)
                        {
                            found_range = range_of_token(ne->get_name_token());
                            return;
                        }
                    }
                }
            }
            else if(id == slang::ast::node_identifier::variable_declaration_expression
                    || id == slang::ast::node_identifier::constant_declaration_expression)
            {
                const auto* ne = expr->as_named_expression();
                if(ne && ne->get_name() == target_name)
                {
                    found_range = range_of_token(ne->get_name_token());
                    return;
                }
            }

            for(const auto* child: expr->get_children())
            {
                self(child, self);
            }
        };

        search_decl(root.get(), search_decl);
        if(found_range.has_value())
        {
            return lsp_location{.uri = uri, .range = *found_range};
        }
    }

    // Search imported/external .sl source files when compiled modules have sources available.
    slang::file_manager file_manager;
    configure_search_paths(file_manager, uri, searchPaths);

    if(root)
    {
        diag::context diagnostics;
        diagnostics.register_file(uri, source);

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
        catch(...)
        {
        }

        // Helper to search an external .sl file for a declaration.
        auto search_module_source =
          [&](std::string_view module_name,
              std::string_view sym_name,
              std::string_view struct_name) -> std::optional<lsp_location>
        {
            const auto sl_path = resolve_module_source_path(file_manager, module_name);
            if(!sl_path.has_value())
            {
                return std::nullopt;
            }

            std::ifstream sl_file(*sl_path, std::ios::binary);
            if(!sl_file)
            {
                return std::nullopt;
            }
            std::string file_source{
              std::istreambuf_iterator<char>(sl_file),
              std::istreambuf_iterator<char>()};
            if(file_source.empty())
            {
                return std::nullopt;
            }

            std::shared_ptr<ast::expression> mod_root;
            try
            {
                auto mod_tokens = slang::lexer::lex(file_source);
                slang::parser mod_parser;
                mod_parser.parse(std::move(mod_tokens));
                mod_root = mod_parser.get_ast();
            }
            catch(...)
            {
                return std::nullopt;
            }

            if(!mod_root)
            {
                return std::nullopt;
            }

            std::optional<lsp_range> found;
            auto search = [&](const ast::expression* expr, auto& self) -> void
            {
                if(!expr || found.has_value())
                {
                    return;
                }

                const auto id = expr->get_id();
                if(struct_name.empty())
                {
                    if(id == ast::node_identifier::function_expression)
                    {
                        const auto* fe = static_cast<const ast::function_expression*>(expr);
                        const auto* proto = fe->get_prototype();
                        if(proto && proto->get_name() == sym_name)
                        {
                            found = range_of_token(proto->get_name_token());
                            return;
                        }
                    }
                    else if(id == ast::node_identifier::struct_definition_expression)
                    {
                        const auto* sde = static_cast<const ast::struct_definition_expression*>(expr);
                        if(sde->get_name() == sym_name)
                        {
                            found = range_of_token(sde->get_name_token());
                            return;
                        }
                    }
                    else if(id == ast::node_identifier::variable_declaration_expression
                            || id == ast::node_identifier::constant_declaration_expression)
                    {
                        const auto* ne = expr->as_named_expression();
                        if(ne && ne->get_name() == sym_name)
                        {
                            found = range_of_token(ne->get_name_token());
                            return;
                        }
                    }
                }
                else if(id == ast::node_identifier::struct_definition_expression)
                {
                    const auto* sde = static_cast<const ast::struct_definition_expression*>(expr);
                    if(sde->get_name() == struct_name)
                    {
                        for(const auto* member: sde->get_children())
                        {
                            if(member && member->is_named_expression())
                            {
                                const auto* ne = member->as_named_expression();
                                if(ne->get_name() == sym_name)
                                {
                                    found = range_of_token(ne->get_name_token());
                                    return;
                                }
                            }
                        }
                    }
                }

                for(const auto* child: expr->get_children())
                {
                    self(child, self);
                }
            };

            search(mod_root.get(), search);
            if(found.has_value())
            {
                return lsp_location{
                  .uri = file_uri_from_path(*sl_path),
                  .range = *found};
            }
            return std::nullopt;
        };

        // Helper to find expression at cursor (descending into access_expression).
        auto token_matches = [pos](const slang::token& t) -> bool
        {
            if(t.s.empty() || t.location.line == 0)
            {
                return false;
            }
            const std::size_t l = t.location.line - 1;
            const std::size_t sc = (t.location.col > 0) ? (t.location.col - 1) : 0;
            const std::size_t ec = sc + t.s.size();
            return l == pos.line && sc <= pos.character && pos.character <= ec;
        };

        auto find_expr = [&](const ast::expression* expr, auto& self) -> const ast::expression*
        {
            if(!expr)
            {
                return nullptr;
            }
            if(expr->is_call_expression())
            {
                const auto* call = static_cast<const ast::call_expression*>(expr);
                if(token_matches(call->get_callee()))
                {
                    return expr;
                }
            }
            if(expr->get_id() == ast::node_identifier::access_expression)
            {
                const auto* access = expr->as_access_expression();
                if(const auto* found = self(access->get_left_expression(), self))
                {
                    return found;
                }
                const auto* rhs = access->get_right_expression();
                if(rhs && rhs->is_named_expression()
                   && token_matches(rhs->as_named_expression()->get_name_token()))
                {
                    return expr;
                }
            }
            if(expr->is_named_expression())
            {
                const auto* ne = expr->as_named_expression();
                if(ne && token_matches(ne->get_name_token()))
                {
                    return expr;
                }
            }
            for(const auto* child: expr->get_children())
            {
                if(const auto* found = self(child, self))
                {
                    return found;
                }
            }
            return nullptr;
        };

        if(const auto* target_expr = find_expr(root.get(), find_expr))
        {
            if(target_expr->get_id() == ast::node_identifier::access_expression)
            {
                const auto* access = target_expr->as_access_expression();
                const auto* rhs = access->get_right_expression();
                if(rhs && rhs->is_named_expression() && access->get_left_expression())
                {
                    const std::string field_name = rhs->as_named_expression()->get_name();
                    if(const auto lhs_type = type_context.get_expression_type(*access->get_left_expression()))
                    {
                        if(const auto* si = type_context.get_struct_info(*lhs_type))
                        {
                            if(si->qualified_name.has_value() && si->qualified_name->contains("::"))
                            {
                                const auto sep = si->qualified_name->rfind("::");
                                const std::string mod_name = si->qualified_name->substr(0, sep);
                                const std::string struct_name = si->qualified_name->substr(sep + 2);
                                if(auto loc = search_module_source(mod_name, field_name, struct_name))
                                {
                                    return loc;
                                }
                            }
                        }
                    }
                }
            }

            if(const auto symbol_id = target_expr->get_symbol_id())
            {
                const auto sym = environment.symbol_table.find(*symbol_id);
                if(sym != environment.symbol_table.end())
                {
                    const auto mod = environment.symbol_table.find(sym->second.declaring_module);
                    if(mod != environment.symbol_table.end() && !mod->second.qualified_name.empty())
                    {
                        if(auto loc = search_module_source(mod->second.qualified_name, sym->second.name, {}))
                        {
                            return loc;
                        }
                    }
                }
            }
        }

        // Check if cursor is on an imported type name (e.g. `math::Point` or `Point`).
        const std::string suffix = std::format("::{}", target_name);
        for(const auto& candidate: type_context.get_type_names())
        {
            if(candidate == target_name || candidate.ends_with(suffix))
            {
                const auto sep = candidate.rfind("::");
                if(sep != std::string::npos)
                {
                    const std::string mod_name = candidate.substr(0, sep);
                    const std::string type_name = candidate.substr(sep + 2);
                    if(auto loc = search_module_source(mod_name, type_name, {}))
                    {
                        return loc;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::string format_definition_result(const std::optional<lsp_location>& loc)
{
    if(!loc.has_value())
    {
        return "null";
    }

    return std::format(
      "{{\"uri\":\"{}\",\"range\":{}}}",
      escape_json_string(loc->uri),
      format_range(loc->range));
}

std::vector<lsp_location> find_references(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  bool include_declaration)
{
    std::vector<lsp_location> result;
    if(source.empty())
    {
        return result;
    }

    slang::token_stream stream;
    try
    {
        stream = slang::lexer::lex(source);
    }
    catch(...)
    {
        return result;
    }

    const auto* target_tok = find_token_at(stream.data(), pos);
    if(!target_tok || target_tok->type != slang::token_type::identifier)
    {
        return result;
    }

    const std::string& target_name = target_tok->s;

    auto def_loc = find_definition(uri, source, pos, {});

    for(const auto& tok: stream.data())
    {
        if(tok.type == slang::token_type::identifier && tok.s == target_name)
        {
            auto range = range_of_token(tok);
            if(!include_declaration && def_loc.has_value())
            {
                if(range.start.line == def_loc->range.start.line
                   && range.start.character == def_loc->range.start.character)
                {
                    continue;
                }
            }

            result.push_back(lsp_location{
              .uri = uri,
              .range = range});
        }
    }

    return result;
}

std::string format_references_result(const std::vector<lsp_location>& locations)
{
    std::string out = "[";
    for(std::size_t i = 0; i < locations.size(); ++i)
    {
        if(i > 0)
            out += ",";
        const auto& loc = locations[i];
        out += std::format(
          "{{\"uri\":\"{}\",\"range\":{}}}",
          escape_json_string(loc.uri),
          format_range(loc.range));
    }
    out += "]";
    return out;
}

}    // namespace slang::lsp
