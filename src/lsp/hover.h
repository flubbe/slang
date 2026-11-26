/**
 * slang - a simple scripting language.
 *
 * LSP hover information.
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

/** An LSP hover item. */
struct hover_item
{
    std::string label;                         // e.g. "x" or "fn foo"
    std::string detail;                        // e.g. "i32" or "fn(i32 a) -> i32"
    std::string kind;                          // e.g. "let", "const", "function", etc.
    std::optional<lsp_location> definition;    // optional: where this symbol is defined
};

/**
 * Look up the symbol at the given position and return its hover information.
 *
 * Runs the full compiler pipeline (lex → parse → resolve → type_check)
 * to determine the symbol's type.
 *
 * @param uri Document URI.
 * @param source Document source code.
 * @param pos Cursor position to look up.
 * @param search_paths Additional module search paths.
 * @return Hover info if a typeable symbol is found at `pos`, otherwise `nullopt`.
 */
std::optional<hover_item> find_hover_info(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  const std::vector<std::string>& search_paths = {});

/**
 * Serialize a hover item as a JSON-RPC `MarkupContent` result.
 *
 * @param item The hover item to serialize (empty means no-hover).
 * @return A JSON response object for `textDocument/hover`.
 */
std::string format_hover_result(const std::optional<hover_item>& item);

}    // namespace slang::lsp
