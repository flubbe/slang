/**
 * slang - a simple scripting language.
 *
 * format! macro token-stream expansion.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <optional>
#include <string>
#include <vector>

#include "format_util.h"
#include "macro.h"
#include "macro_format.h"

namespace slang::macro
{

namespace
{

using slang::const_value;
using slang::root_syntax_context;
using slang::token;
using slang::token_type;

/**
 * Emit a `std::<fn>(<body>)` call as a token list, wrapping the argument token
 * stream.  When `cast_to_i32` is set, the body is widened with `as i32` first
 * (used for i8/i16, which have no dedicated `*_to_string`).
 */
std::vector<token>
  to_string_call(
    const char* fn,
    const token_stream& arg,
    bool cast_to_i32,
    source_location call_loc)
{
    std::vector<token> body;
    body.reserve(arg.data().size() + 6);

    body.emplace_back("std", call_loc, token_type::identifier);
    body.emplace_back("::", call_loc, token_type::delimiter);
    body.emplace_back(fn, call_loc, token_type::identifier);
    body.emplace_back("(", call_loc, token_type::delimiter);

    if(cast_to_i32)
    {
        body.emplace_back("(", call_loc, token_type::delimiter);
        for(const auto& t: arg.data())
            body.push_back(t);
        body.emplace_back(")", call_loc, token_type::delimiter);
        body.emplace_back("as", call_loc, token_type::identifier);
        body.emplace_back("i32", call_loc, token_type::identifier);
    }
    else
    {
        body.append_range(arg.data());
    }

    body.emplace_back(")", call_loc, token_type::delimiter);
    return body;
}

/**
 * Append `item` to a growing left‑nested `std::string_concat(...)` chain.
 *
 * If `result` is empty it becomes the item; otherwise it is wrapped as
 * `std::string_concat(result, item)`, mirroring the legacy AST builder.
 */
void append_concat_item(
  std::vector<token>& result,
  const std::vector<token>& item,
  source_location call_loc)
{
    if(item.empty())
    {
        return;
    }

    if(result.empty())
    {
        result = item;
        return;
    }

    std::vector<token> nested;
    nested.reserve(result.size() + item.size() + 5);

    nested.emplace_back("std", call_loc, token_type::identifier);
    nested.emplace_back("::", call_loc, token_type::delimiter);
    nested.emplace_back("string_concat", call_loc, token_type::identifier);
    nested.emplace_back("(", call_loc, token_type::delimiter);

    for(const auto& t: result)
    {
        nested.push_back(t);
    }

    nested.emplace_back(",", call_loc, token_type::delimiter);
    for(const auto& t: item)
    {
        nested.push_back(t);
    }

    nested.emplace_back(")", call_loc, token_type::delimiter);
    result = std::move(nested);
}

/** A string‑literal token carrying `content` as both its text and its value. */
token string_literal_token(const std::string& content, source_location loc)
{
    return {
      content,
      loc,
      token_type::str_literal,
      std::nullopt,
      const_value{content}};
}

}    // namespace

/*
 * expand_format_builtin.
 */

token_stream expand_format_builtin(
  const std::vector<token_stream>& args,
  const std::vector<typing::type_id>& arg_types,
  source_location call_loc,
  const typing::context& type_ctx)
{
    // -----------------------------------------------------------------
    // 1. Extract the format string literal (the first argument).
    // -----------------------------------------------------------------
    if(args.empty() || args[0].data().empty())
    {
        throw macro_error{
          "Macro 'format!': No format string provided."};
    }

    const token& fmt_tok = args[0].data().front();
    if(fmt_tok.type != token_type::str_literal)
    {
        throw macro_error{
          "Macro 'format!': First argument must be a string literal."};
    }

    if(fmt_tok.s.length() < 2)
    {
        throw macro_error{
          "Macro 'format!': Malformed format string."};
    }

    // Strip the surrounding quotes.
    const std::string fmt = fmt_tok.s.substr(1, fmt_tok.s.length() - 2);

    // -----------------------------------------------------------------
    // 2. Parse placeholders.
    // -----------------------------------------------------------------
    const auto placeholders =
      format_util::parse_format_string(fmt, fmt_tok.location);

    // -----------------------------------------------------------------
    // 3. Validate argument types.  arg_types is aligned with args[1..],
    //    i.e. arg_types[i] is the type of args[i + 1].
    // -----------------------------------------------------------------
    if(arg_types.size() != placeholders.size())
    {
        throw macro_error{
          std::format(
            "Macro 'format!': Argument count ({}) does not match placeholder count ({}).",
            arg_types.size(),
            placeholders.size())};
    }

    for(std::size_t i = 0; i < placeholders.size(); ++i)
    {
        format_util::validate_arg_type(
          arg_types[i],
          placeholders[i].specifier,
          type_ctx,
          fmt_tok.location);
    }

    // String only: no placeholders. Emit the format string literal as‑is.
    if(placeholders.empty())
    {
        return token_stream{std::vector<token>{fmt_tok}};
    }

    // -----------------------------------------------------------------
    // 4. Build the left‑nested std::string_concat(...) chain.
    // -----------------------------------------------------------------
    std::vector<token> result;

    // last_fragment_end tracks the offset into `fmt` after the previous
    // placeholder. `fmt` is the *unquoted* format string, so the leading
    // fragment starts at 0.
    std::size_t last_fragment_end = 0;

    for(std::size_t i = 0; i < placeholders.size(); ++i)
    {
        const auto& fph = placeholders[i];
        const std::size_t arg_index = i + 1;
        const int arg_type = arg_types[i];

        // String fragment before this placeholder.
        const std::string fragment = fmt.substr(
          last_fragment_end,
          fph.start - last_fragment_end);
        last_fragment_end = fph.end;

        if(!fragment.empty())
        {
            append_concat_item(
              result,
              {string_literal_token(fragment, call_loc)},
              call_loc);
        }

        // Conversion of the argument, by type.  Structural tokens carry the
        // root context; the spliced argument tokens keep their own context.
        std::vector<token> conversion;
        if(arg_type == type_ctx.get_i8_type()
           || arg_type == type_ctx.get_i16_type()
           || arg_type == type_ctx.get_i32_type())
        {
            conversion = to_string_call(
              "i32_to_string",
              args[arg_index],
              arg_type != type_ctx.get_i32_type(),
              call_loc);
        }
        else if(arg_type == type_ctx.get_i64_type())
        {
            conversion = to_string_call("i64_to_string", args[arg_index], false, call_loc);
        }
        else if(arg_type == type_ctx.get_f32_type())
        {
            conversion = to_string_call("f32_to_string", args[arg_index], false, call_loc);
        }
        else if(arg_type == type_ctx.get_f64_type())
        {
            conversion = to_string_call("f64_to_string", args[arg_index], false, call_loc);
        }
        else if(arg_type == type_ctx.get_str_type())
        {
            // No conversion needed: splice the argument as‑is.
            conversion = args[arg_index].data();
        }
        else
        {
            throw macro_error{
              std::format(
                "Macro 'format!': No formatter for argument {} at {}.",
                arg_index,
                ::slang::to_string(call_loc))};
        }

        append_concat_item(result, conversion, call_loc);
    }

    // Trailing fragment after the last placeholder. `fmt` is unquoted, so a
    // non-empty tail runs from last_fragment_end to the end of the string.
    if(last_fragment_end != fmt.length())
    {
        const std::string fragment = fmt.substr(
          last_fragment_end,
          fmt.length() - last_fragment_end);

        append_concat_item(
          result,
          {string_literal_token(fragment, call_loc)},
          call_loc);
    }

    return token_stream{std::move(result)};
}

}    // namespace slang::macro
