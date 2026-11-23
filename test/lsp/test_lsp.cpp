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

/* Read message tests. */

TEST(lsp, read_single_message)
{
    std::string body;
    ASSERT_TRUE(read_from_wire(
      "Content-Length: 5\r\n\r\nhello",
      body));
    ASSERT_EQ(body, "hello");
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

}    // namespace
