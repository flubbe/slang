/**
 * slang - a simple scripting language.
 *
 * JSON-RPC 2.0 message parsing and types for LSP.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <sstream>

#include "lsp/json_rpc.h"

namespace slang::lsp
{

incoming_message message_parser::parse(std::string_view json_str)
{
    incoming_message msg;

    auto result = parser.parse(json_str);
    if(result.error())
    {
        msg.kind = message_kind::invalid;
        msg.error.code = error_code::parse_error;
        msg.error.message = std::string(simdjson::error_message(result.error()));
        return msg;
    }

    auto doc = result.value();
    if(doc.type() != simdjson::dom::element_type::OBJECT)
    {
        msg.kind = message_kind::invalid;
        msg.error.code = error_code::invalid_request;
        msg.error.message = "Message must be a JSON object";
        return msg;
    }

    // Validate "jsonrpc" field.
    auto jsonrpc_field = doc["jsonrpc"];
    if(jsonrpc_field.error() != simdjson::error_code::SUCCESS)
    {
        msg.kind = message_kind::invalid;
        msg.error.code = error_code::invalid_request;
        msg.error.message = "Missing 'jsonrpc' field";
        return msg;
    }

    auto jsonrpc_elem = jsonrpc_field.value();
    if(jsonrpc_elem.type() != simdjson::dom::element_type::STRING)
    {
        msg.kind = message_kind::invalid;
        msg.error.code = error_code::invalid_request;
        msg.error.message = "'jsonrpc' field must be a string";
        return msg;
    }

    if(jsonrpc_elem.get_string().value() != "2.0")
    {
        msg.kind = message_kind::invalid;
        msg.error.code = error_code::invalid_request;
        msg.error.message = "'jsonrpc' version must be '2.0'";
        return msg;
    }

    // Inspect "id" field if present.
    auto id_field = doc["id"];
    bool has_id = (id_field.error() == simdjson::error_code::SUCCESS);
    if(has_id)
    {
        auto id_elem = id_field.value();
        auto id_type = id_elem.type();
        if(id_type == simdjson::dom::element_type::INT64)
        {
            msg.id = id_elem.get_int64().value();
        }
        else if(id_type == simdjson::dom::element_type::UINT64)
        {
            msg.id = static_cast<std::int64_t>(id_elem.get_uint64().value());
        }
        else if(id_type == simdjson::dom::element_type::STRING)
        {
            msg.id = std::string(id_elem.get_string().value());
        }
        else
        {
            // Null, boolean, float, object, or array is not accepted as a valid request ID.
            msg.kind = message_kind::invalid;
            msg.error.code = error_code::invalid_request;
            msg.error.message = "'id' must be an integer or string";
            return msg;
        }
    }

    // Inspect "method" field if present.
    auto method_field = doc["method"];
    bool has_method = (method_field.error() == simdjson::error_code::SUCCESS);
    if(has_method)
    {
        auto method_elem = method_field.value();
        if(method_elem.type() != simdjson::dom::element_type::STRING)
        {
            msg.kind = message_kind::invalid;
            msg.error.code = error_code::invalid_request;
            msg.error.message = "'method' must be a string";
            return msg;
        }
        msg.method = std::string(method_elem.get_string().value());
    }

    // Inspect "params" field if present.
    auto params_field = doc["params"];
    bool has_params = (params_field.error() == simdjson::error_code::SUCCESS);
    if(has_params)
    {
        auto params_elem = params_field.value();
        auto params_type = params_elem.type();
        if(params_type != simdjson::dom::element_type::OBJECT
           && params_type != simdjson::dom::element_type::ARRAY)
        {
            msg.kind = message_kind::invalid;
            msg.error.code = error_code::invalid_request;
            msg.error.message = "'params' must be an object or array";
            return msg;
        }

        std::ostringstream ss;
        ss << params_elem;
        msg.params_json = ss.str();
    }

    // Determine message kind.
    if(has_method)
    {
        // A request or notification cannot also have "result" or "error".
        if(doc["result"].error() == simdjson::error_code::SUCCESS
           || doc["error"].error() == simdjson::error_code::SUCCESS)
        {
            msg.kind = message_kind::invalid;
            msg.error.code = error_code::invalid_request;
            msg.error.message = "Message cannot combine 'method' with 'result' or 'error'";
            return msg;
        }

        if(has_id)
        {
            msg.kind = message_kind::request;
        }
        else
        {
            msg.kind = message_kind::notification;
        }
    }
    else
    {
        // No "method" field. Check if it is a response.
        auto result_field = doc["result"];
        auto error_elem = doc["error"];
        bool has_result = (result_field.error() == simdjson::error_code::SUCCESS);
        bool has_error = (error_elem.error() == simdjson::error_code::SUCCESS);

        if(has_id && (has_result || has_error))
        {
            msg.kind = message_kind::response;
            if(has_result)
            {
                std::ostringstream ss;
                ss << result_field.value();
                msg.result_json = ss.str();
            }
            if(has_error)
            {
                std::ostringstream ss;
                ss << error_elem.value();
                msg.error_json = ss.str();
            }
        }
        else
        {
            msg.kind = message_kind::invalid;
            msg.error.code = error_code::invalid_request;
            msg.error.message = "Missing 'method' field";
            return msg;
        }
    }

    return msg;
}

incoming_message parse_message(std::string_view json_str)
{
    message_parser parser;
    return parser.parse(json_str);
}

}    // namespace slang::lsp
