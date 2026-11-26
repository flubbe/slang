/**
 * slang - a simple scripting language.
 *
 * LSP diagnostics tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <sstream>

#include "../../src/lsp/diagnostics.h"
#include "../../src/lsp/json_rpc.h"
#include "../../src/lsp/document_store.h"

namespace
{

/* === Phase 5.1: Severity mapping === */

TEST(diagnostics, severity_mapping)
{
    using comp_sev = slang::compiler::diagnostics::severity;
    EXPECT_EQ(
      slang::lsp::to_lsp_severity(comp_sev::error),
      slang::lsp::lsp_diagnostic_severity::error);
    EXPECT_EQ(
      slang::lsp::to_lsp_severity(comp_sev::warning),
      slang::lsp::lsp_diagnostic_severity::warning);
    EXPECT_EQ(
      slang::lsp::to_lsp_severity(comp_sev::note),
      slang::lsp::lsp_diagnostic_severity::information);
    EXPECT_EQ(
      slang::lsp::to_lsp_severity(comp_sev::help),
      slang::lsp::lsp_diagnostic_severity::hint);
}

TEST(diagnostics, range_conversion)
{
    // 1-based: line 5, column 10, length 3
    // -> 0-based: line 4, char 9 to 12
    slang::compiler::diagnostics::source_span span{{5, 10}, 3};
    const auto range = slang::lsp::to_lsp_range(span);
    EXPECT_EQ(range.start.line, 4);
    EXPECT_EQ(range.start.character, 9);
    EXPECT_EQ(range.end.line, 4);
    EXPECT_EQ(range.end.character, 12);
}

/* === Phase 5.1: Diagnostic computation === */

TEST(diagnostics, clean_source_produces_no_errors)
{
    const std::string source = "fn main() -> int { return 0; }";
    const auto diags =
      slang::lsp::compute_diagnostics("file:///test.sl", source);
    EXPECT_TRUE(diags.empty());
}

TEST(diagnostics, syntax_error_produces_diagnostic)
{
    // Missing closing brace → syntax error
    const std::string source = "fn main() -> int {";
    const auto diags =
      slang::lsp::compute_diagnostics("file:///test.sl", source);
    EXPECT_FALSE(diags.empty());
    EXPECT_EQ(diags[0].severity,
              slang::lsp::lsp_diagnostic_severity::error);
    EXPECT_FALSE(diags[0].message.empty());
    EXPECT_EQ(diags[0].source, "slang");
}

/* === Phase 5.1: Publish diagnostics === */

TEST(diagnostics, publish_notification_format)
{
    std::vector<slang::lsp::lsp_diagnostic> diags;
    diags.push_back(slang::lsp::lsp_diagnostic{
      .range = {.start = {.line = 1, .character = 5},
                .end = {.line = 1, .character = 10}},
      .severity = slang::lsp::lsp_diagnostic_severity::error,
      .message = "Syntax error",
      .source = "slang"});

    const std::string notif =
      slang::lsp::format_publish_diagnostics("file:///test.sl",
                                             diags);
    const auto parsed = slang::lsp::parse_message(notif);
    EXPECT_TRUE(parsed.is_valid());
    EXPECT_TRUE(parsed.is_notification());
    EXPECT_EQ(parsed.method, "textDocument/publishDiagnostics");
    EXPECT_TRUE(
      parsed.params_json->find("file:///test.sl") != std::string::npos);
    EXPECT_TRUE(
      parsed.params_json->find("Syntax error") != std::string::npos);
}

TEST(diagnostics, clear_on_empty_diagnostics)
{
    const std::string notif =
      slang::lsp::format_publish_diagnostics("file:///test.sl",
                                             {});
    const auto parsed = slang::lsp::parse_message(notif);
    EXPECT_TRUE(parsed.is_valid());
    EXPECT_TRUE(parsed.is_notification());
    EXPECT_EQ(parsed.method, "textDocument/publishDiagnostics");
    EXPECT_TRUE(
      parsed.params_json->find("\"diagnostics\":[]") != std::string::npos);
}

}    // namespace
