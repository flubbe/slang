/**
 * slang - a simple scripting language.
 *
 * macro collection / expansion environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>

#include "compiler/macro.h"
#include "sema.h"
#include "syntax_context.h"

namespace
{

using slang::syntax_context_id;
using slang::token;
using slang::token_stream;

/**
 * Helper to retag tokens with a given context id.
 *
 * @param ts The input token stream.
 * @param ctx Current syntax context id to use.
 * @returns Returns the token stream tagged with the syntax context id.
 */
token_stream retag(
  const token_stream& ts,
  syntax_context_id ctx)
{
    std::vector<token> new_tokens;
    new_tokens.reserve(ts.size());
    for(const auto& t: ts.data())
    {
        token copy = t;
        copy.context = ctx;
        new_tokens.push_back(std::move(copy));
    }
    return token_stream{std::move(new_tokens)};
}

}    // namespace

namespace slang::macro
{

token_stream macro::expand_macro(
  const std::vector<token_stream>& args,
  syntax_context_id call_site,
  syntax_context_table& contexts) const
{
    // Don't expand empty macros.
    if(desc.branches.empty())
    {
        return token_stream{};
    }

    // Select the same kind of best matching branch as the legacy AST
    // implementation: exact non-variadic, exact variadic, then a variadic
    // branch with additional arguments.
    const module_::macro_branch_descriptor* branch{nullptr};
    std::size_t best_score{0};
    bool ambiguous{false};
    for(const auto& candidate: desc.branches)
    {
        std::optional<std::size_t> score;
        if(args.size() == candidate.params.size())
        {
            score = candidate.args_end_with_list ? 2 : 3;
        }
        else if(candidate.args_end_with_list
                && args.size() > candidate.params.size())
        {
            score = 1;
        }

        if(!score.has_value())
        {
            continue;
        }

        if(branch == nullptr || score.value() > best_score)
        {
            branch = &candidate;
            best_score = score.value();
            ambiguous = false;
        }
        else if(score.value() == best_score)
        {
            ambiguous = true;
        }
    }

    if(branch == nullptr)
    {
        throw macro_error{
          std::format(
            "Could not match a branch of macro '{}'.",
            name)};
    }
    if(ambiguous)
    {
        throw macro_error{
          std::format(
            "Macro '{}' has ambiguous branches.",
            name)};
    }

    // A selected branch without a token body is still valid for the legacy
    // migration stage, but has nothing for the token expander to emit.
    if(!branch->body.has_value())
    {
        return token_stream{};
    }

    /*
     * Expand a macro hygienically:
     *
     * 1. Create a fresh syntax context for this expansion, parented at the
     *    macro's definition context. This preserves the definition-site
     *    ancestry of identifiers originating in the macro body.
     * 2. Walk the macro body token stream.
     * 3. If an identifier matches a macro parameter, substitute the
     *    corresponding argument token stream, retagging its tokens with
     *    the call-site context so that they retain the caller's name-resolution
     *    environment.
     * 4. Otherwise, keep the token but retag it with the fresh expansion
     *    context, so that identifiers originating in the macro body remain
     *    distinct from identifiers originating at the call site.
     * 5. Concatenate the resulting streams and return the expanded token stream.
     */

    token_stream result;

    /*
     * Fresh expansion context.
     *
     * The expansion happens at the call site, but identifiers originating
     * in the macro body retain the macro's definition-site ancestry.
     */
    syntax_context_id expansion_ctx = contexts.allocate(definition_context);

    // Process each token.
    for(const auto& tok: branch->body.value().data())
    {
        // Check if token is an identifier that matches a macro parameter.
        bool replaced = false;
        if(tok.type == token_type::macro_identifier)
        {
            // Search parameters by name.
            for(std::size_t i = 0; i < branch->params.size(); ++i)
            {
                const auto& param = branch->params[i];    // bounds are check in loop condition. // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                if(tok.s == param.name)
                {
                    // Parameter match → splice argument tokens, retagged.
                    if(i < args.size())
                    {
                        result = result.concat(
                          retag(args[i], call_site));    // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                    }
                    replaced = true;
                    break;
                }
            }
        }

        if(!replaced)
        {
            // Keep definition-originating tokens in this expansion's context.
            token copy = tok;
            copy.context = expansion_ctx;
            result = result.append(copy);
        }
    }

    return result;
}

void env::add_macro(
  std::string name,
  module_::macro_descriptor desc,
  std::optional<std::string> import_path,
  syntax_context_id definition_context)
{
    if(std::ranges::find_if(
         macros,
         [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
         {
             return m->get_name() == name
                    && m->get_import_path() == import_path;
         })
       != macros.end())
    {
        throw macro_error(
          std::format(
            "Macro '{}' already defined.",
            name));
    }

    macros.emplace_back(
      std::make_unique<macro>(
        std::move(name),
        std::move(desc),
        std::move(import_path),
        definition_context));
}

macro* env::get_macro(
  const std::string& name,
  std::optional<std::string> import_path)
{
    auto it = std::ranges::find_if(
      macros,
      [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
      {
          return m->get_name() == name
                 && m->get_import_path() == import_path;
      });

    if(it != macros.end())
    {
        return it->get();
    }

    // macro was not found.
    if(import_path.has_value())
    {
        throw macro_error(
          std::format(
            "Macro '{}::{}' not found.",
            import_path.value(),
            name));
    }
    throw macro_error(
      std::format(
        "Macro '{}' not found.",
        name));
}

}    // namespace slang::macro
