/**
 * slang - a simple scripting language.
 *
 * LSP completion items.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lsp/navigation.h"

namespace slang::lsp
{

/** A single completion item label. */
struct completion_item
{
    /** The symbol name */
    std::string label;

    /** Symbol kind, e.g. "variable", "function", "type". */
    std::string kind;
};

/**
 * Gather completions visible at the given position.
 * Collects all symbol names from the semantic environment, filters to
 * those matching the character prefix at `pos`.
 *
 * @param uri Document URI.
 * @param source Document source code.
 * @param pos Cursor position where completions were requested.
 * @return List of completion items matching the prefix at `pos`.
 */
std::vector<completion_item> get_completions(
  const std::string& uri,
  const std::string& source,
  lsp_position pos);

/**
 * Serialize a completion result as a JSON-RPC array of completion items.
 *
 * @param items List of completion items to serialize.
 * @return A JSON array string for `textDocument/completion`.
 */
std::string format_completion_result(
  const std::vector<completion_item>& items);

}    // namespace slang::lsp
