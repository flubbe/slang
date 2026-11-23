/**
 * slang - a simple scripting language.
 *
 * LSP navigation features: Document Symbols, Go to Definition, Find References.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <memory>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "lsp/json_rpc.h"
#include "lsp/navigation.h"

namespace slang::lsp
{

static lsp_range token_to_range(const slang::token& tok)
{
    std::size_t line = (tok.location.line > 0) ? (tok.location.line - 1) : 0;
    std::size_t col = (tok.location.col > 0) ? (tok.location.col - 1) : 0;
    std::size_t len = !tok.s.empty() ? tok.s.size() : 1;

    return lsp_range{
      .start = {.line = line, .character = col},
      .end = {.line = line, .character = col + len}};
}

static lsp_range node_to_range(const slang::ast::expression* expr)
{
    const auto& loc = expr->get_location();
    std::size_t line = (loc.line > 0) ? (loc.line - 1) : 0;
    std::size_t col = (loc.col > 0) ? (loc.col - 1) : 0;

    return lsp_range{
      .start = {.line = line, .character = col},
      .end = {.line = line, .character = col + 1}};
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
                  .range = node_to_range(fe),
                  .selection_range = token_to_range(proto->get_name_token()),
                  .children = {}};

                for(const auto& arg: proto->get_args())
                {
                    const auto& tok = std::get<0>(arg);
                    sym.children.push_back(document_symbol{
                      .name = tok.s,
                      .kind = symbol_kind::variable,
                      .range = token_to_range(tok),
                      .selection_range = token_to_range(tok),
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
                                  .range = node_to_range(vde),
                                  .selection_range = token_to_range(vde->get_name_token()),
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
              .range = node_to_range(sde),
              .selection_range = token_to_range(sde->get_name_token()),
              .children = {}};

            for(const auto* member: sde->get_children())
            {
                if(member && member->is_named_expression())
                {
                    const auto* ne = member->as_named_expression();
                    sym.children.push_back(document_symbol{
                      .name = ne->get_name(),
                      .kind = symbol_kind::field,
                      .range = token_to_range(ne->get_name_token()),
                      .selection_range = token_to_range(ne->get_name_token()),
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
                  .range = node_to_range(vde),
                  .selection_range = token_to_range(vde->get_name_token()),
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
                  .range = node_to_range(cde),
                  .selection_range = token_to_range(cde->get_name_token()),
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
  lsp_position pos)
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
                        found_range = token_to_range(proto->get_name_token());
                        return;
                    }
                    for(const auto& arg: proto->get_args())
                    {
                        const auto& arg_tok = std::get<0>(arg);
                        if(arg_tok.s == target_name)
                        {
                            found_range = token_to_range(arg_tok);
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
                    found_range = token_to_range(sde->get_name_token());
                    return;
                }
                for(const auto* member: sde->get_children())
                {
                    if(member && member->is_named_expression())
                    {
                        const auto* ne = member->as_named_expression();
                        if(ne->get_name() == target_name)
                        {
                            found_range = token_to_range(ne->get_name_token());
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
                    found_range = token_to_range(ne->get_name_token());
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

    auto def_loc = find_definition(uri, source, pos);

    for(const auto& tok: stream.data())
    {
        if(tok.type == slang::token_type::identifier && tok.s == target_name)
        {
            auto range = token_to_range(tok);
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
