/**
 * slang - a simple scripting language.
 *
 * instruction emitter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "archives/memory.h"
#include "module.h"

/*
 * Forward declarations.
 */

namespace slang::codegen
{
class context;
class instruction;
class function;
}    // namespace slang::codegen

namespace slang
{

namespace cg = slang::codegen;

/** An error during instruction emission. */
class emitter_error : public std::runtime_error
{
public:
    /**
     * Construct a `emitter_error`.
     *
     * @param message The error message.
     */
    emitter_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

class instruction_emitter
{
    /** The associated codegen context. */
    cg::context& ctx;

    /** Memory buffer for instruction emission. */
    memory_write_archive instruction_buffer;

    /** Function details. */
    std::unordered_map<std::string, function_details> func_details;

    /** Referenced jump targets. */
    std::set<std::string> jump_targets;

protected:
    /**
     * Collect all jump targets.
     *
     * @return The jump target labels.
     */
    std::set<std::string> collect_jump_targets() const;

    /**
     * Emit an instruction.
     *
     * @param func The current function.
     * @param instr The instruction to emit.
     */
    void emit_instruction(const std::unique_ptr<cg::function>& func, const std::unique_ptr<cg::instruction>& instr);

public:
    /** Delete constructors. */
    instruction_emitter() = delete;
    instruction_emitter(const instruction_emitter&) = delete;
    instruction_emitter(instruction_emitter&&) = delete;

    /** Delete assignments. */
    instruction_emitter& operator=(const instruction_emitter&) = delete;
    instruction_emitter& operator=(instruction_emitter&&) = delete;

    /**
     * Construct an instruction emitter.
     *
     * @param ctx A codegen context.
     */
    instruction_emitter(cg::context& ctx)
    : ctx{ctx}
    , instruction_buffer{false, slang::endian::little}
    {
    }

    /**
     * Run the instruction emitter.
     *
     * @throws Throws an `emitter_error` if the instruction buffer was not empty.
     */
    void run();

    /**
     * Generate a module from the current context state.
     *
     * @returns A module representing the context state.
     */
    language_module to_module() const;
};

}    // namespace slang