/**
 * slang - a simple scripting language.
 *
 * LSP signature help — describe a function's parameters at a call site.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "signature_help.h"

#include <cctype>
#include <optional>
#include <string>
#include <vector>

#include "lsp/json_rpc.h"

namespace slang::lsp
{

namespace
{

/// Extract the function name at `pos` by scanning backwards for an identifier.
static std::string extract_function_name(
  const std::string& source,
  lsp_position pos)
{
    std::string func_name;
    std::size_t col = pos.character;

    /// Walk backwards.
    while(col > 0)
    {
        --col;
        char c = source[col];
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
        {
            func_name = c + func_name;
        }
        else
        {
            break;
        }
    }

    return func_name;
}

}    // namespace

std::optional<signature_info> get_signature_help(
  [[maybe_unused]] const std::string& uri,
  const std::string& source,
  lsp_position pos)
{
    /// Extract the function name at `pos`.
    std::string func_name = extract_function_name(source, pos);
    if(func_name.empty())
        return std::nullopt;

    /// Check if we're inside parentheses (a function call).
    /// Scan forward from `pos` to find the next '('.
    bool inside_parens = false;
    bool inside_close_paren = false;
    for(std::size_t i = pos.character; i < source.size(); ++i)
    {
        char c = source[i];
        if(c == '(')
        {
            inside_parens = true;
            break;
        }
        else if(c == ')')
        {
            inside_close_paren = true;
            break;
        }
        else if(std::isspace(static_cast<unsigned char>(c)) && inside_close_paren)
        {
            break;
        }
    }

    if(!inside_parens)
        return std::nullopt;

    /// Build a basic signature info.
    signature_info sig;
    sig.label = "fn " + func_name + "(...)";
    sig.label_detail = func_name;
    sig.parameters.push_back({.name = "arg", .type = "unknown"});

    return sig;
}

std::string format_signature_help_result(
  const std::optional<signature_info>& sig)
{
    if(!sig.has_value())
        return std::string{"{}"};

    std::string result;
    result += "{\"label\":\"";

    /// Escape label.
    for(char c: sig->label)
    {
        if(c == '"')
            result += "\\\"";
        else if(c == '\\')
            result += "\\\\";
        else
            result += c;
    }
    result += "\",\"parameters\":[";

    /// Escape parameters.
    for(const auto& param: sig->parameters)
    {
        result += "{\"name\":\"";
        for(char c: param.name)
        {
            if(c == '"')
                result += "\\\"";
            else if(c == '\\')
                result += "\\\\";
            else
                result += c;
        }
        result += "\",\"type\":\"";
        for(char c: param.type)
        {
            if(c == '"')
                result += "\\\"";
            else if(c == '\\')
                result += "\\\\";
            else
                result += c;
        }
        result += "\"}";
    }

    result += "]}";
    return result;
}

}    // namespace slang::lsp
