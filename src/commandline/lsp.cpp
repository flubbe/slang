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

#include <simdjson.h>

#include "lsp/lsp_transport.h"
#include "commandline.h"

namespace slang::commandline
{

namespace
{

/** LSP server. */
class lsp_server
{
    bool running{true};

    /**
     * Handle a single JSON-RPC message body.
     *
     * Examines the `method` field and dispatches to the
     * appropriate handler.  Updates `running` to `false`
     * when the server should shut down (e.g. `exit`,
     * `shutdown`).
     */
    void handle_message(std::string_view body)
    {
        simdjson::dom::parser parser;
        auto result = parser.parse(body);

        if(result.error())
        {
            std::println(
              stderr,
              "[lsp] JSON parse error: {}",
              simdjson::error_message(result.error()));
            return;
        }

        auto doc = result.value();

        // Extract the `method` field (notifications lack it; its absence
        // simply means we have no handler for this message).
        auto method = doc["method"].get_string();
        if(method.error())
            return;

        const std::string_view method_name = method.value();

        // `id` is present on requests (requires a response) but absent on
        // notifications (expect an empty result).
        auto id = doc["id"];

        if(method_name == "initialize")
        {
            std::println(stderr, "[lsp] Initializing.");
            auto id_result = id.get_int64();

            // Build the response with the original request ID (if it was an
            // integer ID; otherwise null).
            std::string response;
            response = "{\"jsonrpc\":\"2.0\"";
            if(id_result.error() == simdjson::error_code::SUCCESS)
            {
                response += std::format(
                  ",\"id\":{}",
                  id_result.value());
            }
            else
            {
                response += R"(,"id":null)";
            }
            response += R"(,"result":{"capabilities":{}}})";

            auto header = std::format(
              "Content-Length: {}\r\n\r\n",
              response.size());

            std::fputs(header.c_str(), stdout);
            std::fputs(response.c_str(), stdout);
            std::fflush(stdout);
        }
        else if(method_name == "initialized")
        {
            // Initialization is complete.
            std::println(stderr, "[lsp] Initialized.");
        }
        else if(method_name == "shutdown")
        {
            std::println(stderr, "[lsp] Shutting down.");
            running = false;

            std::string response =
              R"({"jsonrpc":"2.0","id":null,"result":null})";
            std::string header = std::format(
              "Content-Length: {}\r\n\r\n",
              response.size());

            std::fputs(header.c_str(), stdout);
            std::fputs(response.c_str(), stdout);
            std::fflush(stdout);
        }
        else if(method_name == "exit")
        {
            std::println(stderr, "[lsp] Exiting.");
            running = false;
        }
        else if(method_name == "$/setTrace")
        {
            std::println(stderr, "[lsp] Received $/setTrace.");
        }
        else
        {
            std::println(
              stderr,
              "[lsp] Unknown method: {}",
              std::string{method_name});
        }
    }

public:
    /**
     * Enter the LSP read-loop.
     *
     * Reads from `std::cin`, writes to `std::cout`.  Exits when `exit` is
     * received, when the input is exhausted (EOF), or when a protocol error
     * makes reading impossible.
     */
    void serve()
    {
        detail::lsp_transport transport{
          static_cast<void*>(&std::cin),
          static_cast<void*>(&std::cout)};

        std::println(stderr,
                     "[lsp] Server starting");

        while(running)
        {
            std::string body;
            if(!transport.read_message(body))
                break;

            std::println(
              stderr,
              "[lsp] Received message:\n{}\n",
              body);

            handle_message(body);
        }

        std::println(stderr,
                     "[lsp] Server stopped");
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
