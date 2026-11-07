/**
 * slang - a simple scripting language.
 *
 * format! macro token-stream expansion.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */
#pragma once

#include <vector>

#include "shared/token_stream.h"
#include "typing.h"

namespace slang::macro
{

/**
 * Expand the builtin `format!` macro into a token stream.
 *
 * The produced token stream is a left-nested `std::string_concat(...)` chain
 * interleaving literal fragments of the format string with per-argument
 * `std::<type>_to_string(...)` conversions (or the argument itself, for
 * `str`-typed arguments).
 *
 * @param args Token streams for the macro arguments (`args[0]` = format
 *     string, `args[1..]` = the format arguments).
 * @param arg_types Resolved type of each format argument, aligned with
 *     `args[1..]` (i.e. `arg_types[i]` is the type of `args[i + 1]`).
 * @param call_loc Source location for generated tokens.
 * @param type_ctx Typing context used for argument-type validation.
 * @return Token stream representing the expanded macro body.
 * @throws `macro_error` if the arguments are missing or unsupported.
 * @throws `std::runtime_error` if the format string is invalid or an argument
 *     type does not satisfy its specifier.
 */
token_stream
  expand_format_builtin(
    const std::vector<token_stream>& args,
    const std::vector<typing::type_id>& arg_types,
    source_location call_loc,
    const typing::context& type_ctx);

}    // namespace slang::macro
