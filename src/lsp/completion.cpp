/**
 * slang - a simple scripting language.
 *
 * LSP completion — return visible symbol names matching the prefix at cursor.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "completion.h"

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "compiler/collect.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "compiler/sema.h"
#include "lsp/json_rpc.h"

namespace slang::lsp
{

namespace
{

/** Extract the text prefix at `pos` — alphanumeric characters immediately before cursor. */
static std::string extract_prefix(
  const std::string& source,
  lsp_position pos)
{
    std::string prefix;
    std::size_t col = pos.character;

    // Walk backwards to grab the prefix.
    while(col > 0)
    {
        --col;
        char c = source[col];
        if((c >= 'a' && c <= 'z')
           || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9')
           || c == '_')
        {
            prefix = c + prefix;
        }
        else
        {
            break;
        }
    }
    return prefix;
}

}    // namespace

std::vector<completion_item> get_completions(
  const std::string& uri,
  const std::string& source,
  lsp_position pos)
{
    (void)uri;

    try
    {
        // 1. Parse + collect names.
        slang::token_stream tokens = slang::lexer::lex(source);

        slang::parser parser;
        parser.parse(std::move(tokens));

        sema::env sema_env;
        collect::context co_ctx{sema_env};
        auto ast_root = parser.get_ast();
        if(!ast_root)
        {
            return {};
        }

        ast_root->collect_names(co_ctx);

        // 2. Collect all unique symbol names from the semantic environment.
        std::unordered_set<std::string> all_names;
        for(const auto& [sym_id, sym_info]: sema_env.symbol_table)
        {
            if(!sym_info.name.empty())
            {
                all_names.insert(sym_info.name);
            }
        }

        // 3. Extract prefix at cursor and filter.
        std::string prefix = extract_prefix(source, pos);
        std::vector<completion_item> result;

        for(const auto& name: all_names)
        {
            if(prefix.empty() || name.compare(0, prefix.size(), prefix) == 0)
            {
                result.push_back(completion_item{
                  .label = name,
                  .kind = "identifier"});
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

        // Escape label.
        std::string escaped_label;
        for(char c: items[i].label)
        {
            if(c == '"')
            {
                escaped_label += "\\\"";
            }
            else if(c == '\\')
            {
                escaped_label += "\\\\";
            }
            else
            {
                escaped_label += c;
            }
        }

        // Escape kind.
        std::string escaped_kind;
        for(char c: items[i].kind)
        {
            if(c == '"')
            {
                escaped_kind += "\\\"";
            }
            else if(c == '\\')
            {
                escaped_kind += "\\\\";
            }
            else
            {
                escaped_kind += c;
            }
        }

        result += std::format(
          "{{\"label\":\"{}\",\"kind\":{}}}",
          escaped_label,
          escaped_kind);
    }
    result += "]";
    return result;
}

}    // namespace slang::lsp
