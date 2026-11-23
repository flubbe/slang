/**
 * slang - a simple scripting language.
 *
 * LSP diagnostics bridge and serialization.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <sstream>

#include "compiler/diagnostics.h"
#include "compiler/errors.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "lsp/diagnostics.h"
#include "lsp/json_rpc.h"

namespace slang::lsp
{

lsp_diagnostic_severity to_lsp_severity(slang::compiler::diagnostics::severity s)
{
    switch(s)
    {
    case slang::compiler::diagnostics::severity::error:
        return lsp_diagnostic_severity::error;
    case slang::compiler::diagnostics::severity::warning:
        return lsp_diagnostic_severity::warning;
    case slang::compiler::diagnostics::severity::note:
        return lsp_diagnostic_severity::information;
    case slang::compiler::diagnostics::severity::help:
        return lsp_diagnostic_severity::hint;
    }
    return lsp_diagnostic_severity::error;
}

lsp_range to_lsp_range(const slang::compiler::diagnostics::source_span& span)
{
    // source_location is 1-based, LSP is 0-based.
    std::size_t line = (span.location.line > 0) ? (span.location.line - 1) : 0;
    std::size_t start_col = (span.location.col > 0) ? (span.location.col - 1) : 0;
    std::size_t end_col = start_col + (span.length > 0 ? span.length : 1);

    return lsp_range{
      .start = {.line = line, .character = start_col},
      .end = {.line = line, .character = end_col}};
}

std::vector<lsp_diagnostic> compute_diagnostics(
  const std::string& uri,
  const std::string& source)
{
    std::vector<lsp_diagnostic> result;

    if(source.empty())
    {
        return result;
    }

    slang::compiler::diagnostics::context diag_ctx;
    diag_ctx.register_file(uri, source);

    try
    {
        auto tokens = slang::lexer::lex(source);
        slang::parser parser;
        parser.parse(std::move(tokens));
    }
    catch(const slang::source_error& e)
    {
        std::size_t length = 1;
        if(e.text.has_value() && !e.text->empty())
        {
            length = e.text->size();
        }

        diag_ctx.report_error(
          uri,
          slang::compiler::diagnostics::source_span{e.location, length},
          e.message);
    }
    catch(const std::exception& e)
    {
        diag_ctx.report_error(
          uri,
          slang::compiler::diagnostics::source_span{{1, 1}, 1},
          e.what());
    }

    for(const auto& diag: diag_ctx.get_diagnostics())
    {
        lsp_range range{
          .start = {.line = 0, .character = 0},
          .end = {.line = 0, .character = 1}};

        if(diag.span.has_value())
        {
            range = to_lsp_range(*diag.span);
        }

        std::string full_message = diag.message;
        if(diag.label.has_value())
        {
            full_message += std::format(" ({})", *diag.label);
        }

        result.push_back(lsp_diagnostic{
          .range = range,
          .severity = to_lsp_severity(diag.sev),
          .message = std::move(full_message),
          .source = "slang"});
    }

    return result;
}

std::string format_publish_diagnostics(
  const std::string& uri,
  const std::vector<lsp_diagnostic>& diagnostics)
{
    std::string diags_json = "[";
    for(std::size_t i = 0; i < diagnostics.size(); ++i)
    {
        if(i > 0)
        {
            diags_json += ",";
        }

        const auto& d = diagnostics[i];
        diags_json += std::format(
          "{{\"range\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}},\"severity\":{},\"message\":\"{}\",\"source\":\"{}\"}}",
          d.range.start.line,
          d.range.start.character,
          d.range.end.line,
          d.range.end.character,
          static_cast<int>(d.severity),
          escape_json_string(d.message),
          escape_json_string(d.source));
    }
    diags_json += "]";

    std::string params_json = std::format(
      "{{\"uri\":\"{}\",\"diagnostics\":{}}}",
      escape_json_string(uri),
      diags_json);

    return make_notification("textDocument/publishDiagnostics", params_json);
}

}    // namespace slang::lsp
