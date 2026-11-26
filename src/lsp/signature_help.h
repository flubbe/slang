/**
 * slang - a simple scripting language.
 *
 * LSP signature help — describe a function's parameters at a call site.
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

/** Parameter information for a function. */
struct signature_parameter
{
    std::string name;       // Parameter name (or "argN" if unnamed)
    std::string type;       // Parameter type (e.g. "i32")
};

/** A function signature for signature help. */
struct signature_info
{
    std::string label;          // e.g. "fn foo(i32 a, i32 b)"
    std::string label_detail;   // The function name (e.g. "foo")
    std::vector<signature_parameter> parameters;
};

/**
 * Determine the function signature visible at the given position.

 * Looks up the function name at `pos` and the argument index (determined by
 * the cursor's position relative to commas or opening parentheses).
 *
 * @param uri Document URI.
 * @param source Document source code.
 * @param pos Cursor position.
 * @return Signature info if a function call is detected at `pos`, otherwise `nullopt`.
 */
std::optional<signature_info> get_signature_help(
    [[maybe_unused]] const std::string& uri,
    const std::string& source,
    lsp_position pos);

/**
 * Serialize a signature help result as a JSON-RPC `SignatureHelp` object.
 *
 * @param sig Signature info to serialize (empty means no-signature info).
 * @return A JSON string for `textDocument/signatureHelp`.
 */
std::string format_signature_help_result(
    const std::optional<signature_info>& sig);

} // namespace slang::lsp
