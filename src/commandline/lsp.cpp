/**
 * slang - a simple scripting language.
 *
 * `lsp` command implementation — Language server.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstdio>
#include <iostream>
#include <format>
#include <print>
#include <string>

#include "lsp/lsp_transport.h"
#include "lsp/json_rpc.h"
#include "lsp/document_store.h"
#include "lsp/diagnostics.h"
#include "lsp/navigation.h"
#include "commandline.h"

namespace slang::commandline
{

namespace
{

/** Server lifecycle states. */
enum class server_state
{
    uninitialized,
    initialized,
    shutting_down
};

/** LSP server. */
class lsp_server
{
    bool running{true};
    server_state state{server_state::uninitialized};
    int exit_code{0};

    slang::lsp::transport transport{std::cin, std::cout};
    slang::lsp::message_parser parser;
    slang::lsp::document_store docs;

    /**
     * Handle a single JSON-RPC message body.
     */
    void handle_message(std::string_view body)
    {
        auto msg = parser.parse(body);

        if(!msg.is_valid())
        {
            std::println(
              stderr,
              "[lsp] Invalid JSON-RPC message: {}",
              msg.error.message);

            if(msg.error.code == slang::lsp::error_code::parse_error)
            {
                transport.send_message(
                  slang::lsp::make_error_response(
                    std::nullopt,
                    slang::lsp::error_code::parse_error,
                    msg.error.message));
            }
            else if(msg.id.has_value())
            {
                transport.send_message(
                  slang::lsp::make_error_response(
                    msg.id,
                    slang::lsp::error_code::invalid_request,
                    msg.error.message));
            }
            return;
        }

        if(msg.is_request())
        {
            handle_request(msg);
        }
        else if(msg.is_notification())
        {
            handle_notification(msg);
        }
    }

    void handle_request(const slang::lsp::incoming_message& msg)
    {
        if(state == server_state::shutting_down)
        {
            // Reject any request received after shutdown with InvalidRequest.
            transport.send_message(
              slang::lsp::make_error_response(
                msg.id,
                slang::lsp::error_code::invalid_request,
                "Server is shutting down"));
            return;
        }

        if(msg.method == "initialize")
        {
            std::println(stderr, "[lsp] Handling initialize request.");

            // Advertise supported capabilities.
            std::string result =
              R"({"capabilities":{"textDocumentSync":1,"documentSymbolProvider":true,"definitionProvider":true,"referencesProvider":true},"serverInfo":{"name":"slang-lsp","version":"0.1.0"}})";

            transport.send_message(
              slang::lsp::make_response(*msg.id, result));
        }
        else if(msg.method == "shutdown")
        {
            std::println(stderr, "[lsp] Handling shutdown request.");
            state = server_state::shutting_down;

            transport.send_message(
              slang::lsp::make_response(*msg.id, "null"));
        }
        else if(msg.method == "textDocument/documentSymbol")
        {
            handle_document_symbol(msg);
        }
        else if(msg.method == "textDocument/definition")
        {
            handle_definition(msg);
        }
        else if(msg.method == "textDocument/references")
        {
            handle_references(msg);
        }
        else
        {
            std::println(
              stderr,
              "[lsp] Unknown request method: {}",
              msg.method);

            transport.send_message(
              slang::lsp::make_error_response(
                msg.id,
                slang::lsp::error_code::method_not_found,
                std::format("Method not found: {}", msg.method)));
        }
    }

    void handle_document_symbol(const slang::lsp::incoming_message& msg)
    {
        if(!msg.params_json.has_value())
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        simdjson::dom::parser doc_parser;
        auto result = doc_parser.parse(*msg.params_json);
        if(result.error() != simdjson::error_code::SUCCESS)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        auto uri = result["textDocument"]["uri"].get_string();
        if(uri.error() != simdjson::error_code::SUCCESS)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        std::string uri_str{uri.value()};
        const auto* doc = docs.get(uri_str);
        if(!doc)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        auto symbols = slang::lsp::compute_document_symbols(doc->text);
        transport.send_message(
          slang::lsp::make_response(*msg.id, slang::lsp::format_document_symbols_result(symbols)));
    }

    void handle_definition(const slang::lsp::incoming_message& msg)
    {
        if(!msg.params_json.has_value())
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "null"));
            return;
        }

        simdjson::dom::parser doc_parser;
        auto result = doc_parser.parse(*msg.params_json);
        if(result.error() != simdjson::error_code::SUCCESS)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "null"));
            return;
        }

        auto uri = result["textDocument"]["uri"].get_string();
        auto line = result["position"]["line"].get_int64();
        auto character = result["position"]["character"].get_int64();

        if(uri.error() != simdjson::error_code::SUCCESS || line.error() != simdjson::error_code::SUCCESS
           || character.error() != simdjson::error_code::SUCCESS)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "null"));
            return;
        }

        std::string uri_str{uri.value()};
        const auto* doc = docs.get(uri_str);
        if(!doc)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "null"));
            return;
        }

        slang::lsp::lsp_position pos{
          .line = static_cast<std::size_t>(line.value()),
          .character = static_cast<std::size_t>(character.value())};

        auto loc = slang::lsp::find_definition(uri_str, doc->text, pos);
        transport.send_message(
          slang::lsp::make_response(*msg.id, slang::lsp::format_definition_result(loc)));
    }

    void handle_references(const slang::lsp::incoming_message& msg)
    {
        if(!msg.params_json.has_value())
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        simdjson::dom::parser doc_parser;
        auto result = doc_parser.parse(*msg.params_json);
        if(result.error() != simdjson::error_code::SUCCESS)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        auto uri = result["textDocument"]["uri"].get_string();
        auto line = result["position"]["line"].get_int64();
        auto character = result["position"]["character"].get_int64();
        bool include_decl = true;

        auto inc = result["context"]["includeDeclaration"].get_bool();
        if(inc.error() == simdjson::error_code::SUCCESS)
        {
            include_decl = inc.value();
        }

        if(uri.error() != simdjson::error_code::SUCCESS || line.error() != simdjson::error_code::SUCCESS
           || character.error() != simdjson::error_code::SUCCESS)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        std::string uri_str{uri.value()};
        const auto* doc = docs.get(uri_str);
        if(!doc)
        {
            transport.send_message(slang::lsp::make_response(*msg.id, "[]"));
            return;
        }

        slang::lsp::lsp_position pos{
          .line = static_cast<std::size_t>(line.value()),
          .character = static_cast<std::size_t>(character.value())};

        auto refs = slang::lsp::find_references(uri_str, doc->text, pos, include_decl);
        transport.send_message(
          slang::lsp::make_response(*msg.id, slang::lsp::format_references_result(refs)));
    }

    void handle_notification(const slang::lsp::incoming_message& msg)
    {
        if(msg.method == "initialized")
        {
            std::println(stderr, "[lsp] Client initialized.");
            state = server_state::initialized;
        }
        else if(msg.method == "exit")
        {
            std::println(stderr, "[lsp] Exiting.");
            running = false;
            exit_code = (state == server_state::shutting_down) ? 0 : 1;
        }
        else if(msg.method == "textDocument/didOpen")
        {
            handle_did_open(msg);
        }
        else if(msg.method == "textDocument/didChange")
        {
            handle_did_change(msg);
        }
        else if(msg.method == "textDocument/didClose")
        {
            handle_did_close(msg);
        }
        else if(msg.method == "textDocument/didSave" || msg.method == "$/setTrace")
        {
            // Acknowledged notifications without additional action.
        }
        else
        {
            std::println(
              stderr,
              "[lsp] Unknown notification method ignored: {}",
              msg.method);
        }
    }

    void handle_did_open(const slang::lsp::incoming_message& msg)
    {
        if(!msg.params_json.has_value())
            return;

        simdjson::dom::parser doc_parser;
        auto result = doc_parser.parse(*msg.params_json);
        if(result.error() != simdjson::error_code::SUCCESS)
            return;

        auto td = result["textDocument"];
        auto uri = td["uri"].get_string();
        auto version = td["version"].get_int64();
        auto text = td["text"].get_string();

        if(uri.error() != simdjson::error_code::SUCCESS || text.error() != simdjson::error_code::SUCCESS)
            return;

        std::string uri_str{uri.value()};
        std::int64_t ver_val = (version.error() == simdjson::error_code::SUCCESS) ? version.value() : 0;
        std::string text_str{text.value()};

        docs.did_open(uri_str, ver_val, text_str);

        auto diagnostics = slang::lsp::compute_diagnostics(uri_str, text_str);
        transport.send_message(
          slang::lsp::format_publish_diagnostics(uri_str, diagnostics));
    }

    void handle_did_change(const slang::lsp::incoming_message& msg)
    {
        if(!msg.params_json.has_value())
            return;

        simdjson::dom::parser doc_parser;
        auto result = doc_parser.parse(*msg.params_json);
        if(result.error() != simdjson::error_code::SUCCESS)
            return;

        auto td = result["textDocument"];
        auto uri = td["uri"].get_string();
        auto version = td["version"].get_int64();
        auto changes = result["contentChanges"].get_array();

        if(uri.error() != simdjson::error_code::SUCCESS || changes.error() != simdjson::error_code::SUCCESS)
            return;

        std::string uri_str{uri.value()};
        std::int64_t ver_val = (version.error() == simdjson::error_code::SUCCESS) ? version.value() : 0;

        // With Full sync, the entire file content is in the content change.
        for(auto change: changes.value())
        {
            auto text = change["text"].get_string();
            if(text.error() == simdjson::error_code::SUCCESS)
            {
                std::string text_str{text.value()};
                docs.did_change(uri_str, ver_val, text_str);

                auto diagnostics = slang::lsp::compute_diagnostics(uri_str, text_str);
                transport.send_message(
                  slang::lsp::format_publish_diagnostics(uri_str, diagnostics));
                break;
            }
        }
    }

    void handle_did_close(const slang::lsp::incoming_message& msg)
    {
        if(!msg.params_json.has_value())
            return;

        simdjson::dom::parser doc_parser;
        auto result = doc_parser.parse(*msg.params_json);
        if(result.error() != simdjson::error_code::SUCCESS)
            return;

        auto uri = result["textDocument"]["uri"].get_string();
        if(uri.error() != simdjson::error_code::SUCCESS)
            return;

        std::string uri_str{uri.value()};
        docs.did_close(uri_str);

        // Clear diagnostics upon closure.
        transport.send_message(
          slang::lsp::format_publish_diagnostics(uri_str, {}));
    }

public:
    /**
     * Enter the LSP read-loop.
     */
    int serve()
    {
        std::println(stderr,
                     "[lsp] Server starting");

        while(running)
        {
            std::string body;
            if(!transport.read_message(body))
                break;

            handle_message(body);
        }

        std::println(stderr,
                     "[lsp] Server stopped");
        return exit_code;
    }
};

}    // namespace

lsp::lsp()
: command{"lsp"}
{
}

void lsp::invoke(const std::vector<std::string>& args)
{
    cxxopts::Options options = make_cxxopts_options();

    options.add_options()(
      "s,server", "Run as LSP server.");

    auto result = parse_args(options, args);

    if(result.count("server") > 0)
    {
        lsp_server server;
        server.serve();
    }
    else
    {
        std::println(stderr, "{}", options.help());
        std::println(stderr, "Usage: slang lsp --server");
    }
}

std::string lsp::get_description() const
{
    return "Language Server.";
}

}    // namespace slang::commandline
