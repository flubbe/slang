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

/** A single completion item. */
struct completion_item
{
    /** The symbol name. */
    std::string label;

    /** Symbol kind, e.g. "field", "variable", "function", "type". */
    std::string kind;

    /** Optional detail, e.g. the field's type for a struct member. */
    std::optional<std::string> detail;
};

/**
 * Gather completions visible at the given position.
 *
 * In a member-access context (the cursor sits right after a `.`), the
 * object expression to the left of the dot is type-checked and its struct
 * fields (or `length` for arrays) are offered, filtered by the partial
 * member prefix.  Otherwise all symbol names visible in the document are
 * offered, filtered by the identifier prefix at `pos`.
 *
 * @param uri Document URI.
 * @param source Document source code.
 * @param pos Cursor position where completions were requested.
 * @param search_paths Extra directories for resolving imported types/structs.
 * @return List of completion items matching the prefix at `pos`.
 */
std::vector<completion_item> get_completions(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  const std::vector<std::string>& search_paths = {});

/**
 * Serialize a completion result as a JSON-RPC array of completion items.
 *
 * @param items List of completion items to serialize.
 * @return A JSON array string for `textDocument/completion`.
 */
std::string format_completion_result(
  const std::vector<completion_item>& items);

}    // namespace slang::lsp
