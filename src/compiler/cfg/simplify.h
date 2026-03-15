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

namespace slang::cfg
{

namespace cg = slang::codegen;

/** Control flow graph simplificaion context. */
class simplify
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
    simplify() = delete;
    simplify(const simplify&) = delete;
    simplify(simplify&&) = delete;

    /** Deleted assignments. */
    simplify& operator=(const simplify&) = delete;
    simplify& operator=(simplify&&) = delete;

    /**
     * Initialize the CFG simplification context.
     *
     * @param ctx The codegen context.
     */
    explicit simplify(cg::context& ctx)
    : ctx{ctx}
    {
    }

    /** Run the CFG analysis. */
    void run();
};

}    // namespace slang::cfg
