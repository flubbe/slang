/**
 * slang - a simple scripting language.
 *
 * LSP diagnostics bridge and serialization.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "compiler/diagnostics.h"

namespace slang::lsp
{

/** 0-based position in an LSP text document. */
struct lsp_position
{
    std::size_t line{0};
    std::size_t character{0};
};

/** 0-based range in an LSP text document. */
struct lsp_range
{
    lsp_position start;
    lsp_position end;
};

/** Standard LSP diagnostic severities. */
enum class lsp_diagnostic_severity : int
{
    error = 1,
    warning = 2,
    information = 3,
    hint = 4
};

/** An LSP diagnostic. */
struct lsp_diagnostic
{
    lsp_range range;
    lsp_diagnostic_severity severity{lsp_diagnostic_severity::error};
    std::string message;
    std::string source{"slang"};
};

/** Convert compiler diagnostic severity to LSP severity. */
lsp_diagnostic_severity to_lsp_severity(slang::compiler::diagnostics::severity s);

/** Convert compiler source_span to LSP range. */
lsp_range to_lsp_range(const slang::compiler::diagnostics::source_span& span);

/**
 * Run compiler front-end passes (lexing & parsing) on document source text
 * and return the resulting LSP diagnostics.
 *
 * @param uri Document URI.
 * @param source Document source code.
 * @return List of LSP diagnostics.
 */
std::vector<lsp_diagnostic> compute_diagnostics(
  const std::string& uri,
  const std::string& source);

/**
 * Format a `textDocument/publishDiagnostics` notification for sending to the client.
 *
 * @param uri Document URI.
 * @param diagnostics List of diagnostics (empty list clears existing diagnostics).
 * @return Serialized JSON-RPC notification.
 */
std::string format_publish_diagnostics(
  const std::string& uri,
  const std::vector<lsp_diagnostic>& diagnostics);

}    // namespace slang::lsp
