/**
 * slang - a simple scripting language.
 *
 * `lsp` command implementation — Language Server Protocol transport.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdio>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace slang::lsp
{

/**
 * Stream-agnostic framing for LSP (JSON-RPC 2.0) messages.
 *
 * Provides `Content-Length` header parsing and message body
 * I/O over injected `std::istream` / `std::ostream` pairs.
 */
class transport
{
public:
    /**
     * Construct for a given input and output stream reference.
     */
    transport(std::istream& in, std::ostream& out)
    : in{&in}
    , out{&out}
    {
    }

    /**
     * Backward-compatible constructor accepting raw pointers.
     */
    transport(void* in, void* out)
    : in{static_cast<std::istream*>(in)}
    , out{static_cast<std::ostream*>(out)}
    {
    }

    /**
     * Read one LSP message from the input stream.
     *
     * @param body On return, the message body string.
     * @returns true when a valid message was read;
     *          false on EOF or protocol error.
     *          On error body is cleared.
     */
    bool read_message(std::string& body);

    /**
     * Frame and send a message to the output stream.
     *
     * The header uses `Content-Length` in bytes (UTF-8
     * byte count). The header is terminated by CRLF,
     * followed by a blank line, then the body.
     *
     * @param body The message body string to send.
     */
    void send_message(std::string_view body);

private:
    std::istream* in{nullptr};
    std::ostream* out{nullptr};
};

}    // namespace slang::lsp

namespace slang::commandline::detail
{
using lsp_transport = slang::lsp::transport;
}
