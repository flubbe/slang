/**
 * slang - a simple scripting language.
 *
 * `lsp` command implementation (Language Server).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstdio>
#include <iostream>
#include <print>
#include <string>

#include "commandline.h"

#include <simdjson.h>

namespace slang::commandline
{

namespace
{

class lsp_server
{
    bool running{true};

    /**
     * Read one LSP message from stdin.
     *
     * Messages consist of headers terminated by CRLF, followed by
     * a blank line and a body of Content-Length bytes.
     */
    bool read_message(std::string& message)
    {
        std::string header;
        std::size_t content_length = 0;

        while(std::getline(std::cin, header))
        {
            // std::getline removes '\n', but leaves '\r'.
            if(header == "\r" || header.empty())
            {
                break;
            }

            constexpr std::string_view content_length_header =
              "Content-Length:";

            if(header.starts_with(content_length_header))
            {
                auto value = header.substr(content_length_header.size());

                content_length = std::stoull(value);
            }
        }

        if(std::cin.eof() && header.empty())
        {
            return false;
        }

        if(content_length == 0)
        {
            std::println(stderr, "[lsp] Invalid or missing Content-Length.");
            return false;
        }

        message.resize(content_length);

        std::cin.read(message.data(),
                      static_cast<std::streamsize>(content_length));

        if(std::cin.gcount() != static_cast<std::streamsize>(content_length))
        {
            std::println(stderr, "[lsp] Incomplete message body.");
            return false;
        }

        return true;
    }

    /** Write one JSON-RPC response to stdout. */
    void send_response(std::string_view response)
    {
        std::println(stdout,
                     "Content-Length: {}\r",
                     response.size());

        std::print(stdout, "\r\n");
        std::print(stdout, "{}", response);

        std::fflush(stdout);
    }

    /** Parse and handle an incoming JSON-RPC message. */
    void handle_message(const std::string& message)
    {
        simdjson::dom::parser parser;
        auto result = parser.parse(message);

        if(result.error())
        {
            std::println(stderr,
                         "[lsp] JSON parse error: {}",
                         simdjson::error_message(result.error()));
            return;
        }

        auto doc = result.value();

        auto method = doc["method"].get_string();
        if(method.error())
        {
            return;
        }

        const std::string_view method_name = method.value();

        // Requests have an ID and require a response.
        auto id = doc["id"];

        if(method_name == "initialize")
        {
            std::println(stderr, "[lsp] Initializing.");

            send_response(
              R"({"jsonrpc":"2.0","id":)" + std::to_string(id.get_int64().value()) + R"(,"result":{"capabilities":{}}})");
        }
        else if(method_name == "shutdown")
        {
            std::println(stderr, "[lsp] Shutting down.");

            send_response(
              R"({"jsonrpc":"2.0","id":)" + std::to_string(id.get_int64().value()) + R"(,"result":null})");

            running = false;
        }
        else if(method_name == "exit")
        {
            running = false;
        }
    }

public:
    void serve()
    {
        std::println(stderr, "[lsp] Server starting.");

        while(running)
        {
            std::string message;

            if(!read_message(message))
            {
                break;
            }

            std::println(stderr, "[lsp] Received message:");
            std::println(stderr, "{}", message);

            handle_message(message);
        }

        std::println(stderr, "[lsp] Server stopped.");
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
      "s,server",
      "Run as LSP server.");

    auto result = parse_args(options, args);

    if(result.count("server") > 0)
    {
        lsp_server server;
        server.serve();
    }
    else
    {
        std::print("{}", options.help());
        std::print("\nUsage: slang lsp --server\n");
    }
}

std::string lsp::get_description() const
{
    return "Language Server.";
}

}    // namespace slang::commandline
