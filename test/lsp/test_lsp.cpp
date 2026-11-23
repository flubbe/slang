/**
 * slang - a simple scripting language.
 *
 * LSP message framing tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <print>
#include <sstream>

#include "../src/lsp/lsp_transport.h"
#include "../src/lsp/json_rpc.h"
#include "../src/lsp/document_store.h"
#include "../src/lsp/diagnostics.h"
#include "../src/lsp/navigation.h"

namespace
{

/** Frame `body` and return the raw serialized bytes. */
static std::string frame(std::string_view body)
{
    std::istringstream in;
    std::ostringstream out;

    slang::commandline::detail::lsp_transport
      transport{&in, &out};
    transport.send_message(body);
    return out.str();
}

/** Feed `wire` into an input stream and parse and read a message body. */
static bool read_from_wire(
  std::string_view wire,
  std::string& body)
{
    std::istringstream in{std::string{wire}};
    std::ostringstream out;

    slang::commandline::detail::lsp_transport
      transport{&in, &out};
    return transport.read_message(body);
}

/* Send message tests. */

TEST(lsp, send_single_message)
{
    std::string result = frame("hello");
    ASSERT_EQ(result, "Content-Length: 5\r\n\r\nhello");
}

TEST(lsp, send_empty_body)
{
    std::string result = frame("");
    ASSERT_EQ(result, "Content-Length: 0\r\n\r\n");
}

TEST(lsp, send_large_body)
{
    std::string body;
    body.resize(1'000'000, 'X');
    std::string result = frame(body);
    ASSERT_TRUE(
      result.find("Content-Length: 1000000") != std::string::npos);

    std::string_view sv = result;
    auto pos = sv.find("\r\n\r\n");
    ASSERT_NE(pos, std::string_view::npos);
    sv.remove_prefix(pos + 4);
    ASSERT_EQ(sv.size(), body.size());
    ASSERT_EQ(std::string{sv}, body);
}

TEST(lsp, send_consecutive_messages)
{
    std::istringstream in;
    std::ostringstream out;

    slang::commandline::detail::lsp_transport
      transport{&in, &out};
    transport.send_message("msg1");
    transport.send_message("msg2");

    std::string expected =
      frame("msg1") + frame("msg2");
    ASSERT_EQ(out.str(), expected);
}

TEST(lsp, send_non_ascii_utf8_body)
{
    // "こんにちは" is 5 Japanese characters, encoded as 15 UTF-8 bytes.
    std::string utf8_text = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF";
    std::string result = frame(utf8_text);
    ASSERT_EQ(result, "Content-Length: 15\r\n\r\n" + utf8_text);
}

/* Read message tests. */

TEST(lsp, read_single_message)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "Content-Length: 5\r\n\r\nhello",
      body));
    ASSERT_EQ(body, "hello");
}

TEST(lsp, read_non_ascii_utf8_body)
{
    std::string utf8_text = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF";
    std::string wire = "Content-Length: 15\r\n\r\n" + utf8_text;
    std::string body;
    ASSERT_TRUE(read_from_wire(wire, body));
    ASSERT_EQ(body, utf8_text);
}

TEST(lsp, read_empty_body)
{
    std::string body;
    ASSERT_TRUE(
      read_from_wire(
        "Content-Length: 0\r\n\r\n",
        body));
    ASSERT_EQ(body, "");
}

TEST(lsp, read_multiple_consecutive_messages)
{
    std::string b1, b2;
    std::string wire =
      "Content-Length: 5\r\n\r\n"
      "hello"
      "Content-Length: 6\r\n\r\n"
      "world!";
    std::istringstream in{wire};
    std::ostringstream out;
    slang::commandline::detail::lsp_transport
      transport{&in, &out};

    ASSERT_TRUE(transport.read_message(b1));
    ASSERT_EQ(b1, "hello");

    ASSERT_TRUE(transport.read_message(b2));
    ASSERT_EQ(b2, "world!");

    // EOF after the second message.
    std::string b3;
    ASSERT_FALSE(transport.read_message(b3));
}

TEST(lsp, read_message_with_additional_headers)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "Cache-Control: no-cache\r\n"
      "Content-Length: 5\r\n\r\nhello",
      body));
    ASSERT_EQ(body, "hello");
}

TEST(lsp, read_message_with_additional_headers_reversed)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "Content-Length: 5\r\n"
      "X-Extra: value\r\n\r\nhello",
      body));
    ASSERT_EQ(body, "hello");
}

TEST(lsp, read_message_with_crlf_headers)
{
    std::string body;
    ASSERT_TRUE(
      read_from_wire(
        "Content-Length: 5\r\n\r\nworld",
        body));
    ASSERT_EQ(body, "world");
}

TEST(lsp, read_message_with_lf_headers)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "Content-Length: 5\n\nhello",
      body));
    ASSERT_EQ(body, "hello");
}

TEST(lsp, read_different_body_sizes)
{
    std::string body;

    ASSERT_TRUE(read_from_wire(
      "Content-Length: 1\r\n\r\nx",
      body));
    ASSERT_EQ(body, "x");

    ASSERT_TRUE(read_from_wire(
      "Content-Length: 2\r\n\r\nxy",
      body));
    ASSERT_EQ(body, "xy");

    ASSERT_TRUE(read_from_wire(
      "Content-Length: 17\r\n\r\n"
      "{\"jsonrpc\":\"2.0\"}",
      body));
    ASSERT_EQ(
      body,
      "{\"jsonrpc\":\"2.0\"}");
}

/* Error handling tests. */

TEST(lsp, handle_empty_input_and_eof)
{
    std::string body;
    ASSERT_FALSE(read_from_wire("", body));
}

TEST(lsp, handle_incomplete_message_body)
{
    std::string body;
    ASSERT_FALSE(read_from_wire(
      "Content-Length: 100\r\n\r\nhello", body));
}

// `X-Content-Length` contains `"Content-Length:"`
// as a substring, so it matches.
TEST(lsp, accepts_content_length_as_substr)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "X-Content-Length: 5\r\n\r\nhello", body));
    ASSERT_EQ(body, "hello");
}

TEST(lsp, handle_malformed_header_missing_colon)
{
    // `X-Custom: 5` does NOT contain
    // `"Content-Length:"`.
    std::string body;
    ASSERT_FALSE(read_from_wire(
      "X-Custom: 5\r\n\r\nhello", body));
}

TEST(lsp, handle_malformed_header_non_numeric)
{
    std::string body;
    ASSERT_FALSE(read_from_wire(
      "Content-Length: abc\r\n\r\nhello", body));
}

TEST(lsp, handle_whitespace_only_body)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "Content-Length: 3\r\n\r\n   ", body));
    ASSERT_EQ(body, "   ");
}

TEST(lsp, header_not_first_line_accepted)
{
    // `Content-Length` anywhere in the header block
    // is acceptable.
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "X-Extra: value\r\n"
      "Content-Length: 5\r\n\r\nhello",
      body));
    ASSERT_EQ(body, "hello");
}

TEST(lsp, logging_does_not_contaminate_protocol_output)
{
    // The transport has no fixed stream for logging —
    // all I/O goes through injected streams, so stderr
    // output is impossible.
    std::istringstream in;
    std::ostringstream out;
    slang::commandline::detail::lsp_transport
      transport{&in, &out};

    std::print(stderr, "noise\n");    // write to stderr.

    transport.send_message("x");
    ASSERT_EQ(
      out.str(),
      "Content-Length: 1\r\n\r\nx");
}

TEST(lsp, duplex_streams_independent)
{
    std::istringstream input{
      "Content-Length: 5\r\n\r\nreply"};
    std::ostringstream output;
    slang::commandline::detail::lsp_transport
      transport{&input, &output};

    std::string body;
    ASSERT_TRUE(transport.read_message(body));
    ASSERT_EQ(body, "reply");
    transport.send_message("response");

    ASSERT_EQ(
      output.str(),
      "Content-Length: 8\r\n\r\nresponse");
}

/* JSON-RPC message parser tests (Phase 2.1). */

TEST(lsp, parse_valid_request)
{
    std::string json = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":1234}})";
    auto msg = slang::lsp::parse_message(json);

    ASSERT_TRUE(msg.is_valid());
    ASSERT_TRUE(msg.is_request());
    ASSERT_FALSE(msg.is_notification());
    ASSERT_FALSE(msg.is_response());
    ASSERT_EQ(msg.method, "initialize");
    ASSERT_TRUE(msg.id.has_value());
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
    ASSERT_EQ(std::get<std::int64_t>(*msg.id), 1);
    ASSERT_TRUE(msg.params_json.has_value());
    ASSERT_TRUE(msg.params_json->find("processId") != std::string::npos);
}

TEST(lsp, parse_valid_notification)
{
    std::string json = R"({"jsonrpc":"2.0","method":"initialized","params":{}})";
    auto msg = slang::lsp::parse_message(json);

    ASSERT_TRUE(msg.is_valid());
    ASSERT_FALSE(msg.is_request());
    ASSERT_TRUE(msg.is_notification());
    ASSERT_FALSE(msg.is_response());
    ASSERT_EQ(msg.method, "initialized");
    ASSERT_FALSE(msg.id.has_value());
}

TEST(lsp, handle_integer_ids)
{
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":0,"method":"test"})");
        ASSERT_TRUE(msg.is_valid());
        ASSERT_TRUE(msg.id.has_value());
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
        ASSERT_EQ(std::get<std::int64_t>(*msg.id), 0);
    }
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":9223372036854775807,"method":"test"})");
        ASSERT_TRUE(msg.is_valid());
        ASSERT_TRUE(msg.id.has_value());
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
        ASSERT_EQ(std::get<std::int64_t>(*msg.id), 9223372036854775807LL);
    }
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":-42,"method":"test"})");
        ASSERT_TRUE(msg.is_valid());
        ASSERT_TRUE(msg.id.has_value());
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
        ASSERT_EQ(std::get<std::int64_t>(*msg.id), -42);
    }
}

TEST(lsp, handle_string_ids)
{
    auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":"request-abc-123","method":"test"})");
    ASSERT_TRUE(msg.is_valid());
    ASSERT_TRUE(msg.is_request());
    ASSERT_TRUE(msg.id.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(*msg.id));
    ASSERT_EQ(std::get<std::string>(*msg.id), "request-abc-123");
}

TEST(lsp, handle_missing_or_invalid_fields)
{
    // Missing jsonrpc field.
    {
        auto msg = slang::lsp::parse_message(R"({"id":1,"method":"test"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Invalid jsonrpc version.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"1.0","id":1,"method":"test"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Non-string jsonrpc version.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":2.0,"id":1,"method":"test"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Missing method in request/notification.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":1})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Non-string method.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":1,"method":123})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Boolean ID.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":true,"method":"test"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Array ID.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":[1,2],"method":"test"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Object ID.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":{"a":1},"method":"test"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Non-structured params (string instead of array/object).
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0","id":1,"method":"test","params":"invalid"})");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
    // Root is not a JSON object.
    {
        auto msg = slang::lsp::parse_message(R"(["jsonrpc", "2.0"])");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
    }
}

TEST(lsp, handle_malformed_json)
{
    // Incomplete JSON.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0")");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::parse_error);
    }
    // Trailing garbage / syntax error.
    {
        auto msg = slang::lsp::parse_message(R"({"jsonrpc":"2.0", "id": })");
        ASSERT_FALSE(msg.is_valid());
        ASSERT_EQ(msg.error.code, slang::lsp::error_code::parse_error);
    }
}

TEST(lsp, parse_response_message)
{
    // Result response.
    {
        std::string json = R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{}}})";
        auto msg = slang::lsp::parse_message(json);
        ASSERT_TRUE(msg.is_valid());
        ASSERT_TRUE(msg.is_response());
        ASSERT_FALSE(msg.is_request());
        ASSERT_FALSE(msg.is_notification());
        ASSERT_TRUE(msg.id.has_value());
        ASSERT_TRUE(msg.result_json.has_value());
        ASSERT_FALSE(msg.error_json.has_value());
    }
    // Error response.
    {
        std::string json = R"({"jsonrpc":"2.0","id":2,"error":{"code":-32601,"message":"Method not found"}})";
        auto msg = slang::lsp::parse_message(json);
        ASSERT_TRUE(msg.is_valid());
        ASSERT_TRUE(msg.is_response());
        ASSERT_TRUE(msg.id.has_value());
        ASSERT_FALSE(msg.result_json.has_value());
        ASSERT_TRUE(msg.error_json.has_value());
    }
}

/* Phase 2.2 Response formatting tests */

TEST(lsp, format_successful_responses)
{
    // Integer ID
    {
        std::string res = slang::lsp::make_response(42, "{\"capabilities\":{}}");
        auto parsed = slang::lsp::parse_message(res);
        ASSERT_TRUE(parsed.is_valid());
        ASSERT_TRUE(parsed.is_response());
        ASSERT_TRUE(parsed.id.has_value());
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(*parsed.id));
        ASSERT_EQ(std::get<std::int64_t>(*parsed.id), 42);
        ASSERT_TRUE(parsed.result_json.has_value());
    }
    // String ID
    {
        std::string res = slang::lsp::make_response("req-id-1", "null");
        auto parsed = slang::lsp::parse_message(res);
        ASSERT_TRUE(parsed.is_valid());
        ASSERT_TRUE(parsed.is_response());
        ASSERT_TRUE(parsed.id.has_value());
        ASSERT_TRUE(std::holds_alternative<std::string>(*parsed.id));
        ASSERT_EQ(std::get<std::string>(*parsed.id), "req-id-1");
    }
}

TEST(lsp, format_error_responses)
{
    // Error with integer ID
    {
        std::string res = slang::lsp::make_error_response(10, slang::lsp::error_code::method_not_found, "Method not found");
        auto parsed = slang::lsp::parse_message(res);
        ASSERT_TRUE(parsed.is_valid());
        ASSERT_TRUE(parsed.is_response());
        ASSERT_TRUE(parsed.id.has_value());
        ASSERT_EQ(std::get<std::int64_t>(*parsed.id), 10);
        ASSERT_TRUE(parsed.error_json.has_value());
    }
    // Error with null ID (e.g. parse error)
    {
        std::string res = slang::lsp::make_error_response(std::nullopt, slang::lsp::error_code::parse_error, "Parse error");
        ASSERT_TRUE(res.find("\"id\":null") != std::string::npos);
        ASSERT_TRUE(res.find("\"code\":-32700") != std::string::npos);
    }
}

TEST(lsp, format_notification)
{
    std::string notif = slang::lsp::make_notification("custom/notification", "{\"status\":\"ok\"}");
    auto parsed = slang::lsp::parse_message(notif);
    ASSERT_TRUE(parsed.is_valid());
    ASSERT_TRUE(parsed.is_notification());
    ASSERT_EQ(parsed.method, "custom/notification");
    ASSERT_FALSE(parsed.id.has_value());
}

/* Phase 4 Document store tests */

TEST(lsp, document_store_lifecycle)
{
    slang::lsp::document_store store;
    const std::string uri = "file:///path/to/test.sl";

    ASSERT_EQ(store.size(), 0);
    ASSERT_FALSE(store.contains(uri));
    ASSERT_EQ(store.get(uri), nullptr);

    // Open
    store.did_open(uri, 1, "fn main() -> int { return 0; }");
    ASSERT_EQ(store.size(), 1);
    ASSERT_TRUE(store.contains(uri));
    const auto* doc = store.get(uri);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->version, 1);
    ASSERT_EQ(doc->text, "fn main() -> int { return 0; }");

    // Change
    ASSERT_TRUE(store.did_change(uri, 2, "fn main() -> int { return 42; }"));
    doc = store.get(uri);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->version, 2);
    ASSERT_EQ(doc->text, "fn main() -> int { return 42; }");

    // Change nonexistent document
    ASSERT_FALSE(store.did_change("file:///nonexistent.sl", 1, "text"));

    // Close
    ASSERT_TRUE(store.did_close(uri));
    ASSERT_EQ(store.size(), 0);
    ASSERT_FALSE(store.contains(uri));
    ASSERT_EQ(store.get(uri), nullptr);

    // Close again
    ASSERT_FALSE(store.did_close(uri));
}

TEST(lsp, document_store_multiple_documents)
{
    slang::lsp::document_store store;
    store.did_open("file:///doc1.sl", 1, "code 1");
    store.did_open("file:///doc2.sl", 10, "code 2");

    ASSERT_EQ(store.size(), 2);
    ASSERT_EQ(store.get("file:///doc1.sl")->text, "code 1");
    ASSERT_EQ(store.get("file:///doc2.sl")->text, "code 2");

    store.did_change("file:///doc1.sl", 2, "new code 1");
    ASSERT_EQ(store.get("file:///doc1.sl")->text, "new code 1");
    ASSERT_EQ(store.get("file:///doc2.sl")->text, "code 2");

    store.did_close("file:///doc1.sl");
    ASSERT_EQ(store.size(), 1);
    ASSERT_FALSE(store.contains("file:///doc1.sl"));
    ASSERT_TRUE(store.contains("file:///doc2.sl"));
}

/* Phase 5 Diagnostics bridge tests */

TEST(lsp, diagnostics_severity_and_range_mapping)
{
    using comp_sev = slang::compiler::diagnostics::severity;
    ASSERT_EQ(slang::lsp::to_lsp_severity(comp_sev::error), slang::lsp::lsp_diagnostic_severity::error);
    ASSERT_EQ(slang::lsp::to_lsp_severity(comp_sev::warning), slang::lsp::lsp_diagnostic_severity::warning);
    ASSERT_EQ(slang::lsp::to_lsp_severity(comp_sev::note), slang::lsp::lsp_diagnostic_severity::information);
    ASSERT_EQ(slang::lsp::to_lsp_severity(comp_sev::help), slang::lsp::lsp_diagnostic_severity::hint);

    // 1-based source_span: line 5, column 10, length 3 -> 0-based range: line 4, char 9 to 12
    slang::compiler::diagnostics::source_span span{{5, 10}, 3};
    auto range = slang::lsp::to_lsp_range(span);
    ASSERT_EQ(range.start.line, 4);
    ASSERT_EQ(range.start.character, 9);
    ASSERT_EQ(range.end.line, 4);
    ASSERT_EQ(range.end.character, 12);
}

TEST(lsp, diagnostics_clean_source_produces_no_errors)
{
    std::string clean_source = "fn main() -> int { return 0; }";
    auto diags = slang::lsp::compute_diagnostics("file:///test.sl", clean_source);
    ASSERT_TRUE(diags.empty());
}

TEST(lsp, diagnostics_syntax_error_produces_diagnostic)
{
    // Malformed function definition (missing closing brace / syntax error)
    std::string invalid_source = "fn main() -> int {";
    auto diags = slang::lsp::compute_diagnostics("file:///test.sl", invalid_source);
    ASSERT_FALSE(diags.empty());
    ASSERT_EQ(diags[0].severity, slang::lsp::lsp_diagnostic_severity::error);
    ASSERT_FALSE(diags[0].message.empty());
    ASSERT_EQ(diags[0].source, "slang");
}

TEST(lsp, diagnostics_publish_notification_formatting)
{
    std::vector<slang::lsp::lsp_diagnostic> diags;
    diags.push_back(slang::lsp::lsp_diagnostic{
      .range = {.start = {.line = 1, .character = 5}, .end = {.line = 1, .character = 10}},
      .severity = slang::lsp::lsp_diagnostic_severity::error,
      .message = "Syntax error",
      .source = "slang"});

    std::string notif = slang::lsp::format_publish_diagnostics("file:///test.sl", diags);
    auto parsed = slang::lsp::parse_message(notif);
    ASSERT_TRUE(parsed.is_valid());
    ASSERT_TRUE(parsed.is_notification());
    ASSERT_EQ(parsed.method, "textDocument/publishDiagnostics");
    ASSERT_TRUE(parsed.params_json.has_value());
    ASSERT_TRUE(parsed.params_json->find("file:///test.sl") != std::string::npos);
    ASSERT_TRUE(parsed.params_json->find("Syntax error") != std::string::npos);
}

TEST(lsp, diagnostics_clear_on_empty)
{
    std::string notif = slang::lsp::format_publish_diagnostics("file:///test.sl", {});
    auto parsed = slang::lsp::parse_message(notif);
    ASSERT_TRUE(parsed.is_valid());
    ASSERT_TRUE(parsed.is_notification());
    ASSERT_EQ(parsed.method, "textDocument/publishDiagnostics");
    ASSERT_TRUE(parsed.params_json.has_value());
    ASSERT_TRUE(parsed.params_json->find("\"diagnostics\":[]") != std::string::npos);
}

/* Phase 6.1 Navigation tests */

TEST(lsp, document_symbols_extraction)
{
    std::string source =
      "fn add(a: i32, b: i32) -> i32 {\n"
      "    let sum: i32 = a + b;\n"
      "    return sum;\n"
      "}\n"
      "struct Point { x: i32, y: i32 };\n"
      "const MAX_COUNT: i32 = 999;\n";

    auto symbols = slang::lsp::compute_document_symbols(source);
    ASSERT_EQ(symbols.size(), 3);

    // 1. Function 'add'
    EXPECT_EQ(symbols[0].name, "add");
    EXPECT_EQ(symbols[0].kind, slang::lsp::symbol_kind::function);
    ASSERT_EQ(symbols[0].children.size(), 3);
    EXPECT_EQ(symbols[0].children[0].name, "a");
    EXPECT_EQ(symbols[0].children[0].kind, slang::lsp::symbol_kind::variable);
    EXPECT_EQ(symbols[0].children[1].name, "b");
    EXPECT_EQ(symbols[0].children[1].kind, slang::lsp::symbol_kind::variable);
    EXPECT_EQ(symbols[0].children[2].name, "sum");
    EXPECT_EQ(symbols[0].children[2].kind, slang::lsp::symbol_kind::variable);

    // 2. Struct 'Point'
    EXPECT_EQ(symbols[1].name, "Point");
    EXPECT_EQ(symbols[1].kind, slang::lsp::symbol_kind::struct_);
    ASSERT_EQ(symbols[1].children.size(), 2);
    EXPECT_EQ(symbols[1].children[0].name, "x");
    EXPECT_EQ(symbols[1].children[0].kind, slang::lsp::symbol_kind::field);
    EXPECT_EQ(symbols[1].children[1].name, "y");
    EXPECT_EQ(symbols[1].children[1].kind, slang::lsp::symbol_kind::field);

    // 3. Constant 'MAX_COUNT'
    EXPECT_EQ(symbols[2].name, "MAX_COUNT");
    EXPECT_EQ(symbols[2].kind, slang::lsp::symbol_kind::constant);

    // Verify JSON serialization
    std::string json = slang::lsp::format_document_symbols_result(symbols);
    EXPECT_TRUE(json.find("\"name\":\"add\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"name\":\"Point\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"name\":\"sum\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"kind\":23") != std::string::npos);    // struct
    EXPECT_TRUE(json.find("\"kind\":12") != std::string::npos);    // function
}

TEST(lsp, go_to_definition)
{
    std::string source =
      "fn helper(x: i32) -> i32 { return x * 2; }\n"
      "fn main() -> i32 { return helper(42); }\n";

    const std::string uri = "file:///workspace/test.sl";

    // Position on 'helper' call in main (Line 1, character 28)
    slang::lsp::lsp_position call_pos{.line = 1, .character = 28};
    auto def_loc = slang::lsp::find_definition(uri, source, call_pos);

    ASSERT_TRUE(def_loc.has_value());
    EXPECT_EQ(def_loc->uri, uri);
    // Definition is 'fn helper' on Line 0
    EXPECT_EQ(def_loc->range.start.line, 0);
    EXPECT_EQ(def_loc->range.start.character, 3);    // 'helper' starts at index 3

    // Position on whitespace
    slang::lsp::lsp_position whitespace_pos{.line = 1, .character = 0};
    auto none_loc = slang::lsp::find_definition(uri, source, whitespace_pos);
    EXPECT_FALSE(none_loc.has_value());
}

TEST(lsp, find_references)
{
    std::string source =
      "fn compute(n: i32) -> i32 { return n + 1; }\n"
      "fn main() -> i32 {\n"
      "    let a: i32 = compute(10);\n"
      "    let b: i32 = compute(20);\n"
      "    return a + b;\n"
      "}\n";

    const std::string uri = "file:///workspace/test.sl";

    // Position on 'compute' declaration (Line 0, character 4)
    slang::lsp::lsp_position decl_pos{.line = 0, .character = 4};

    // References including declaration (1 def + 2 calls = 3)
    auto refs_with_decl = slang::lsp::find_references(uri, source, decl_pos, true);
    EXPECT_EQ(refs_with_decl.size(), 3);

    // References excluding declaration (2 calls)
    auto refs_without_decl = slang::lsp::find_references(uri, source, decl_pos, false);
    EXPECT_EQ(refs_without_decl.size(), 2);
    for(const auto& r: refs_without_decl)
    {
        EXPECT_NE(r.range.start.line, 0);    // None on line 0 (declaration)
    }

    // Verify JSON serialization
    std::string json = slang::lsp::format_references_result(refs_with_decl);
    EXPECT_TRUE(json.find("\"uri\":\"file:///workspace/test.sl\"") != std::string::npos);
}

}    // namespace
