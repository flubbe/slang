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

namespace slang::commandline
{
namespace detail
{

/**
 * Stream-agnostic framing for LSP (JSON-RPC 2.0) messages.
 *
 * Provides `Content-Length` header parsing and message body
 * I/O over injected `std::istream` / `std::ostream` pairs.
 */
class lsp_transport
{
public:
    /**
     * Construct for a given input and output stream.
     *
     * The `in` and `out` arguments are stored as `void*` and
     * recast to the correct base types in methods.
     * This lets tests use any `std::istringstream*` /
     * `std::ostringstream*` combination.
     */
    lsp_transport(void* in, void* out)
    : in{in}
    , out{out}
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
    void* in = nullptr;
    void* out = nullptr;
};

}    // namespace detail
}    // namespace slang::commandline
