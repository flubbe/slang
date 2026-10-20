/**
 * slang - a simple scripting language.
 *
 * macro collection / expansion environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <stdexcept>
#include <optional>
#include <string>
#include <unordered_map>

#include "shared/module.h"
#include "syntax_context.h"
#include "token_stream.h"

namespace slang::macro
{

/** A generic error during macro processing. */
class macro_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** A macro. */
class macro
{
    /** The macro name. */
    std::string name;

    /** The macro descriptor. */
    module_::macro_descriptor desc;

    /** Import path. */
    std::optional<std::string> import_path;

    /** Syntax context at the macro definition site. */
    syntax_context_id definition_context{root_syntax_context};

public:
    /** Constructors. */
    macro() = delete;
    macro(const macro&) = default;
    macro(macro&&) = default;

    /** Destructor. */
    ~macro() = default;

    /** Assignments. */
    macro& operator=(const macro&) = default;
    macro& operator=(macro&&) = default;

    /**
     * Create a new macro.
     *
     * @param name The macro's name.
     * @param desc Macro descriptor.
     * @param import_path Optional import path.
     * @param definition_context Syntax context at the macro definition site.
     */
    explicit macro(
      std::string name,
      module_::macro_descriptor desc,
      std::optional<std::string> import_path = std::nullopt,
      syntax_context_id definition_context = root_syntax_context)
    : name{std::move(name)}
    , desc{std::move(desc)}
    , import_path{std::move(import_path)}
    , definition_context{definition_context}
    {
    }

    /** Get the macro's name. */
    [[nodiscard]]
    const std::string& get_name() const
    {
        return name;
    }

    /** Get the descriptor. */
    [[nodiscard]]
    const module_::macro_descriptor& get_desc() const
    {
        return desc;
    }

    /** Get the import path. */
    [[nodiscard]]
    const std::optional<std::string> get_import_path() const
    {
        return import_path;
    }

    /** Return whether this is an import. */
    [[nodiscard]]
    bool is_import() const
    {
        return import_path.has_value();
    }

    /** Whether this is a transitive import. */
    [[nodiscard]]
    bool is_transitive_import() const
    {
        return is_import() && name.starts_with("$");
    }

    /** Get the definition-site syntax context. */
    [[nodiscard]]
    syntax_context_id get_definition_context() const
    {
        return definition_context;
    }

    /**
     * Return whether the selected branch is supported by token expansion.
     *
     * TODO(macro-migration): Remove this dispatch gate when all macro shapes
     * use token expansion; branch matching should then be authoritative.
     *
     * @param args Token streams for the macro arguments.
     */
    [[nodiscard]]
    bool supports_token_expansion(
      const std::vector<token_stream>& args) const;

    /** Set transitivity. */
    void set_transitive(bool transitive)
    {
        bool transitive_name = name.starts_with("$");
        if(transitive_name && !transitive)
        {
            name = name.substr(1);
        }
        else if(!transitive_name && transitive)
        {
            name = std::string{"$"} + name;
        }
    }

    /**
     * Expand a macro hygienically using a syntax context table.
     *
     * @param args Token streams for the macro arguments.
     * @param call_site Syntax context id of the invocation site (used to tag
     *     substituted argument tokens).
     * @param contexts Syntax context table for allocating expansion contexts.
     * @return A token_stream representing the expanded macro body.
     */
    token_stream expand_macro(
      const std::vector<token_stream>& args,
      syntax_context_id call_site,
      syntax_context_table& contexts) const;
};

/** Macro collection / expansion environment. */
struct env
{
    /** List of macros. */
    std::vector<std::unique_ptr<macro>> macros;

    /**
     * Add a macro definition.
     *
     * @throws Throws a `macro_error` if the macro already exists.
     * @param name The macro name.
     * @param desc The macro descriptor.
     * @param import_path Import path of the macro, or `std::nullopt`.
     * @param definition_context Syntax context at the macro definition site.
     */
    void add_macro(
      std::string name,
      module_::macro_descriptor desc,
      std::optional<std::string> import_path,
      syntax_context_id definition_context = root_syntax_context);

    /**
     * Get a macro.
     *
     * @throws Throws a `macro_error` if the macro is not found.
     * @param name The macro name.
     * @param import_path Import path of the macro, or `std::nullopt`.
     */
    macro* get_macro(
      const std::string& name,
      std::optional<std::string> import_path);
};

}    // namespace slang::macro
