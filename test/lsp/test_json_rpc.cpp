/**
 * slang - a simple scripting language.
 *
 * LSP JSON-RPC tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <string>

#include "../../src/lsp/json_rpc.h"

namespace
{

/* === Phase 2.1: Message parser === */

TEST(json_rpc, parse_valid_request)
{
    const std::string json =
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":1234}})";
    const auto msg = slang::lsp::parse_message(json);

    EXPECT_TRUE(msg.is_valid());
    EXPECT_TRUE(msg.is_request());
    EXPECT_FALSE(msg.is_notification());
    EXPECT_FALSE(msg.is_response());
    EXPECT_EQ(msg.method, "initialize");
    EXPECT_TRUE(msg.id.has_value());
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
    EXPECT_EQ(std::get<std::int64_t>(*msg.id), 1);
    EXPECT_TRUE(msg.params_json.has_value());
    EXPECT_TRUE(msg.params_json->find("processId") != std::string::npos);
}

TEST(json_rpc, parse_valid_notification)
{
    const std::string json =
      R"({"jsonrpc":"2.0","method":"initialized","params":{}})";
    const auto msg = slang::lsp::parse_message(json);

    EXPECT_TRUE(msg.is_valid());
    EXPECT_FALSE(msg.is_request());
    EXPECT_TRUE(msg.is_notification());
    EXPECT_FALSE(msg.is_response());
    EXPECT_EQ(msg.method, "initialized");
    EXPECT_FALSE(msg.id.has_value());
}

TEST(json_rpc, handle_integer_ids)
{
    // Zero
    {
        const auto msg = slang::lsp::parse_message(
          R"({"jsonrpc":"2.0","id":0,"method":"test"})");
        EXPECT_TRUE(msg.is_valid());
        EXPECT_TRUE(msg.id.has_value());
        EXPECT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
        EXPECT_EQ(std::get<std::int64_t>(*msg.id), 0);
    }
    // INT64_MAX
    {
        const auto msg = slang::lsp::parse_message(
          R"({"jsonrpc":"2.0","id":9223372036854775807,"method":"test"})");
        EXPECT_TRUE(msg.is_valid());
        EXPECT_TRUE(msg.id.has_value());
        EXPECT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
        EXPECT_EQ(std::get<std::int64_t>(*msg.id), 9223372036854775807LL);
    }
    // Negative
    {
        const auto msg = slang::lsp::parse_message(
          R"({"jsonrpc":"2.0","id":-42,"method":"test"})");
        EXPECT_TRUE(msg.is_valid());
        EXPECT_TRUE(msg.id.has_value());
        EXPECT_TRUE(std::holds_alternative<std::int64_t>(*msg.id));
        EXPECT_EQ(std::get<std::int64_t>(*msg.id), -42);
    }
}

TEST(json_rpc, handle_string_ids)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":"request-abc-123","method":"test"})");
    EXPECT_TRUE(msg.is_valid());
    EXPECT_TRUE(msg.is_request());
    EXPECT_TRUE(msg.id.has_value());
    EXPECT_TRUE(std::holds_alternative<std::string>(*msg.id));
    EXPECT_EQ(std::get<std::string>(*msg.id), "request-abc-123");
}

/* === Invalid / malformed requests === */

TEST(json_rpc, handle_missing_jsonrpc_field)
{
    const auto msg = slang::lsp::parse_message(
      R"({"id":1,"method":"test"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_invalid_jsonrpc_version)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"1.0","id":1,"method":"test"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_non_string_jsonrpc_version)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":2.0,"id":1,"method":"test"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_missing_method)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":1})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_non_string_method)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":1,"method":123})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_boolean_id)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":true,"method":"test"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_array_id)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":[1,2],"method":"test"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_object_id)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":{"a":1},"method":"test"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_non_structured_params)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0","id":1,"method":"test","params":"invalid"})");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

TEST(json_rpc, handle_non_object_root)
{
    const auto msg = slang::lsp::parse_message(
      R"(["jsonrpc", "2.0"])");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::invalid_request);
}

/* === Malformed JSON === */

TEST(json_rpc, handle_incomplete_json)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0")");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::parse_error);
}

TEST(json_rpc, handle_trailing_garbage)
{
    const auto msg = slang::lsp::parse_message(
      R"({"jsonrpc":"2.0", "id": })");
    EXPECT_FALSE(msg.is_valid());
    EXPECT_EQ(msg.error.code, slang::lsp::error_code::parse_error);
}

/* === Phase 2.2: Response formatting === */

TEST(json_rpc, parse_response_with_result)
{
    const std::string json =
      R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{}}})";
    const auto msg = slang::lsp::parse_message(json);
    EXPECT_TRUE(msg.is_valid());
    EXPECT_TRUE(msg.is_response());
    EXPECT_FALSE(msg.is_request());
    EXPECT_FALSE(msg.is_notification());
    EXPECT_TRUE(msg.id.has_value());
    EXPECT_TRUE(msg.result_json.has_value());
    EXPECT_FALSE(msg.error_json.has_value());
}

TEST(json_rpc, parse_response_with_error)
{
    const std::string json =
      R"({"jsonrpc":"2.0","id":2,"error":{"code":-32601,"message":"Method not found"}})";
    const auto msg = slang::lsp::parse_message(json);
    EXPECT_TRUE(msg.is_valid());
    EXPECT_TRUE(msg.is_response());
    EXPECT_TRUE(msg.id.has_value());
    EXPECT_FALSE(msg.result_json.has_value());
    EXPECT_TRUE(msg.error_json.has_value());
}

TEST(json_rpc, format_response_integer_id)
{
    const std::string res =
      slang::lsp::make_response(42, R"({"capabilities":{}})");
    const auto parsed = slang::lsp::parse_message(res);
    EXPECT_TRUE(parsed.is_valid());
    EXPECT_TRUE(parsed.is_response());
    EXPECT_TRUE(parsed.id.has_value());
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(*parsed.id));
    EXPECT_EQ(std::get<std::int64_t>(*parsed.id), 42);
    EXPECT_TRUE(parsed.result_json.has_value());
}

TEST(json_rpc, format_response_string_id)
{
    const std::string res = slang::lsp::make_response("req-id-1", "null");
    const auto parsed = slang::lsp::parse_message(res);
    EXPECT_TRUE(parsed.is_valid());
    EXPECT_TRUE(parsed.is_response());
    EXPECT_TRUE(parsed.id.has_value());
    EXPECT_TRUE(std::holds_alternative<std::string>(*parsed.id));
    EXPECT_EQ(std::get<std::string>(*parsed.id), "req-id-1");
}

TEST(json_rpc, format_error_response)
{
    const std::string res =
      slang::lsp::make_error_response(
        10, slang::lsp::error_code::method_not_found,
        "Method not found");
    const auto parsed = slang::lsp::parse_message(res);
    EXPECT_TRUE(parsed.is_valid());
    EXPECT_TRUE(parsed.is_response());
    EXPECT_TRUE(parsed.id.has_value());
    EXPECT_EQ(std::get<std::int64_t>(*parsed.id), 10);
    EXPECT_TRUE(parsed.error_json.has_value());
}

TEST(json_rpc, format_error_response_null_id)
{
    const std::string res =
      slang::lsp::make_error_response(
        std::nullopt, slang::lsp::error_code::parse_error,
        "Parse error");
    EXPECT_TRUE(res.find("\"id\":null") != std::string::npos);
    EXPECT_TRUE(res.find("\"code\":-32700") != std::string::npos);
}

TEST(json_rpc, format_notification)
{
    const std::string notif =
      slang::lsp::make_notification(
        "custom/notification", R"({"status":"ok"})");
    const auto parsed = slang::lsp::parse_message(notif);
    EXPECT_TRUE(parsed.is_valid());
    EXPECT_TRUE(parsed.is_notification());
    EXPECT_EQ(parsed.method, "custom/notification");
    EXPECT_FALSE(parsed.id.has_value());
}

}    // namespace
