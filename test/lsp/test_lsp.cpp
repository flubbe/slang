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

#include <sstream>

#include "../src/lsp/lsp_transport.h"
#include "../src/lsp/json_rpc.h"

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

}    // namespace
