/**
 * slang - a simple scripting language.
 *
 * JSON-RPC 2.0 message parsing and types for LSP.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <simdjson.h>

namespace slang::lsp
{

/** JSON-RPC 2.0 request identifier (integer or string). */
using request_id = std::variant<std::int64_t, std::string>;

/** Kind of incoming JSON-RPC message. */
enum class message_kind
{
    request,
    notification,
    response,
    invalid
};

/** Standard JSON-RPC 2.0 and LSP error codes. */
enum class error_code : int
{
    parse_error = -32700,
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_params = -32602,
    internal_error = -32603,
    server_not_initialized = -32002,
    unknown_error_code = -32001,
    request_cancelled = -32800,
    content_modified = -32801,
};

/** Detailed error information for invalid messages. */
struct parse_error_info
{
    error_code code{error_code::invalid_request};
    std::string message;
};

/** Parsed incoming JSON-RPC message. */
struct incoming_message
{
    message_kind kind{message_kind::invalid};

    /** Request identifier, if present. */
    std::optional<request_id> id;

    /** Method name for requests and notifications. */
    std::string method;

    /** Raw JSON string of `params` (object or array), if present. */
    std::optional<std::string> params_json;

    /** Raw JSON string of `result` for responses, if present. */
    std::optional<std::string> result_json;

    /** Raw JSON string of `error` for responses, if present. */
    std::optional<std::string> error_json;

    /** Error details if kind == message_kind::invalid. */
    parse_error_info error;

    [[nodiscard]] bool is_request() const noexcept
    {
        return kind == message_kind::request;
    }

    [[nodiscard]] bool is_notification() const noexcept
    {
        return kind == message_kind::notification;
    }

    [[nodiscard]] bool is_response() const noexcept
    {
        return kind == message_kind::response;
    }

    [[nodiscard]] bool is_valid() const noexcept
    {
        return kind != message_kind::invalid;
    }
};

/**
 * JSON-RPC message parser reusing an internal simdjson DOM parser.
 */
class message_parser
{
public:
    message_parser() = default;

    /**
     * Parse a JSON-RPC 2.0 message from a string.
     *
     * @param json_str The raw JSON string.
     * @return The parsed incoming message or an invalid message descriptor on error.
     */
    incoming_message parse(std::string_view json_str);

private:
    simdjson::dom::parser parser;
};

/**
 * Convenience function to parse a single JSON-RPC message.
 */
incoming_message parse_message(std::string_view json_str);

/** Escape a string for safe embedding inside a JSON string literal. */
std::string escape_json_string(std::string_view str);

/** Format a successful JSON-RPC 2.0 response. */
std::string make_response(const request_id& id, std::string_view result_json);

/** Format an error JSON-RPC 2.0 response. */
std::string make_error_response(
  const std::optional<request_id>& id,
  error_code code,
  std::string_view message);

/** Format a JSON-RPC 2.0 notification from server to client. */
std::string make_notification(std::string_view method, std::string_view params_json);

}    // namespace slang::lsp
