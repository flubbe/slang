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
#include "commandline.h"

namespace slang::commandline
{

namespace
{

/** LSP server. */
class lsp_server
{
    bool running{true};
    slang::lsp::transport transport{std::cin, std::cout};
    slang::lsp::message_parser parser;

    /**
     * Handle a single JSON-RPC message body.
     *
     * Examines the `method` field and dispatches to the
     * appropriate handler. Updates `running` to `false`
     * when the server should shut down (e.g. `exit`,
     * `shutdown`).
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
            return;
        }

        if(msg.is_request())
        {
            if(msg.method == "initialize")
            {
                std::println(stderr, "[lsp] Initializing.");

                std::string response = "{\"jsonrpc\":\"2.0\"";
                if(msg.id.has_value())
                {
                    std::visit(
                      [&response](const auto& id_val)
                      {
                          using T = std::decay_t<decltype(id_val)>;
                          if constexpr(std::is_same_v<T, std::int64_t>)
                          {
                              response += std::format(",\"id\":{}", id_val);
                          }
                          else
                          {
                              response += std::format(",\"id\":\"{}\"", id_val);
                          }
                      },
                      *msg.id);
                }
                else
                {
                    response += R"(,"id":null)";
                }
                response += R"(,"result":{"capabilities":{}}})";

                transport.send_message(response);
            }
            else if(msg.method == "shutdown")
            {
                std::println(stderr, "[lsp] Shutting down.");
                running = false;

                std::string response = "{\"jsonrpc\":\"2.0\"";
                if(msg.id.has_value())
                {
                    std::visit(
                      [&response](const auto& id_val)
                      {
                          using T = std::decay_t<decltype(id_val)>;
                          if constexpr(std::is_same_v<T, std::int64_t>)
                          {
                              response += std::format(",\"id\":{}", id_val);
                          }
                          else
                          {
                              response += std::format(",\"id\":\"{}\"", id_val);
                          }
                      },
                      *msg.id);
                }
                else
                {
                    response += R"(,"id":null)";
                }
                response += R"(,"result":null})";

                transport.send_message(response);
            }
            else
            {
                std::println(
                  stderr,
                  "[lsp] Unknown request method: {}",
                  msg.method);
            }
        }
        else if(msg.is_notification())
        {
            if(msg.method == "initialized")
            {
                std::println(stderr, "[lsp] Initialized.");
            }
            else if(msg.method == "exit")
            {
                std::println(stderr, "[lsp] Exiting.");
                running = false;
            }
            else if(msg.method == "$/setTrace")
            {
                std::println(stderr, "[lsp] Received $/setTrace.");
            }
            else
            {
                std::println(
                  stderr,
                  "[lsp] Unknown notification method: {}",
                  msg.method);
            }
        }
    }

public:
    /**
     * Enter the LSP read-loop.
     *
     * Reads from `std::cin`, writes to `std::cout`. Exits when `exit` is
     * received, when the input is exhausted (EOF), or when a protocol error
     * makes reading impossible.
     */
    void serve()
    {
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
