/**
 * slang - a simple scripting language.
 *
 * Simple control flow graph analysis.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

/*
 * Forward declarations.
 */
namespace slang::codegen
{
class context;
class function;
}    // namespace slang::codegen

namespace slang::opt::cfg
{

namespace cg = slang::codegen;

/** Control flow graph analysis context. */
class context
{
    /** The associated codegen context. */
    cg::context& ctx;

protected:
    /**
     * Run the CFG analysis on a function.
     *
     * @param func The function to run the analysis on.
     */
    void run_on_function(cg::function& func);

public:
    /** Deleted constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = delete;

    /** Deleted assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Initialize the CFG context.
     *
     * @param ctx The codegen context.
     */
    explicit context(cg::context& ctx)
    : ctx{ctx}
    {
    }

    /** Run the CFG analysis. */
    void run();
};

}    // namespace slang::opt::cfg
