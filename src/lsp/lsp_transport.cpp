/**
 * slang - a simple scripting language.
 *
 * Language Server Protocol message framing.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstring>
#include <format>
#include <print>
#include <string>

#include "lsp/lsp_transport.h"

namespace slang::lsp
{

bool transport::read_message(
  std::string& body)
{
    if(in == nullptr)
    {
        return false;
    }

    std::istream* in_stream = in;

    std::string line;
    std::size_t content_length{0};
    bool content_length_present{false};

    // Read headers.
    constexpr std::string_view keyword =
      "Content-Length:";
    while(std::getline(*in_stream, line))
    {
        // Strip trailing \r (CRLF with LF
        // transparently).
        if(!line.empty()
           && line.back() == '\r')
        {
            line.pop_back();
        }

        // Empty line signals the end of
        // headers.
        if(line.empty())
        {
            break;
        }

        // Look for `Content-Length` anywhere
        // in the header block.
        std::size_t pos = line.find(keyword);
        if(pos != std::string::npos)
        {
            std::string_view value{
              line.data() + pos + keyword.size(),
              line.size() - pos
                - keyword.size()};

            // Skip leading whitespace.
            while(!value.empty()
                  && (value.front() == ' '
                      || value.front() == '\t'))
            {
                value.remove_prefix(1);
            }

            // Parse as a plain integer.
            char* end = nullptr;
            unsigned long long val =
              std::strtoull(
                value.data(), &end,
                10);

            // Accept only if the entire
            // (trimmed) value was consumed.
            if(end == value.data() + value.size())
            {
                content_length_present = true;
                content_length =
                  static_cast<std::size_t>(val);
            }
        }
    }

    // No Content-Length header found before EOF
    // -- transport is done.
    if(in_stream->eof()
       && line.empty()
       && !content_length_present)
    {
        return false;
    }

    // Missing `Content-Length` -- fatal protocol
    // error.
    if(!content_length_present)
    {
        std::println(
          stderr,
          "[lsp] protocol error: "
          "missing Content-Length "
          "header");

        return false;
    }

    // Read exactly `content_length` bytes.
    body.resize(content_length);
    if(content_length == 0)
    {
        return true;
    }

    std::streamsize total = 0;
    while(static_cast<std::size_t>(total)
          < content_length)
    {
        in_stream->read(
          body.data() + total,
          static_cast<
            std::streamsize>(
            content_length)
            - total);

        if(!in_stream->good())
        {
            break;
        }

        total += in_stream->gcount();
    }

    if(static_cast<std::size_t>(total)
       != content_length)
    {
        std::println(
          stderr,
          "[lsp] protocol error: "
          "incomplete body (expected "
          "{} bytes, got {})",
          content_length,
          total);
        body.clear();
        return false;
    }

    return true;
}

void transport::send_message(
  std::string_view body)
{
    if(out == nullptr)
    {
        return;
    }

    std::ostream* out_stream = out;

    std::string header;
    header.reserve(32);
    header += std::format(
      "Content-Length: {}\r\n\r\n",
      body.size());

    *out_stream << header;
    *out_stream << body;
    out_stream->flush();
}

}    // namespace slang::lsp
