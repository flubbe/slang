/**
 * slang - a simple scripting language.
 *
 * instruction emitter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <set>

#include "archives/memory.h"
#include "shared/module.h"

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
    using std::runtime_error::runtime_error;
};

/** Export table builder. */
class export_table_builder
{
    /** Export table. */
    std::vector<module_::exported_symbol> export_table;

public:
    /** Clear the export table. */
    void clear()
    {
        export_table.clear();
    }

    /** Return the export table size. */
    std::size_t size() const
    {
        return export_table.size();
    }

    /**
     * Add a function to the export table.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The argument types.
     * @throws Throws an `emitter_error` if the function already exists.
     */
    void add_function(
      const std::string& name,
      module_::variable_type return_type,
      std::vector<module_::variable_type> arg_types);

    /**
     * Set the function's details (`size`, `offset` and `locals`).
     *
     * @param name Name of the function to set the details for.
     * @param size The function's bytecode size.
     * @param offset The function's offset / entry point.
     * @param locals The function's locals.
     */
    void update_function(
      const std::string& name,
      std::size_t size,
      std::size_t offset,
      std::vector<module_::variable_descriptor> locals);

    /**
     * Add a native function to the export table.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_types The argument types.
     * @param import_library Name of the library this function is implemented in.
     * @throws Throws an `emitter_error` if the function already exists.
     */
    void add_native_function(
      const std::string& name,
      module_::variable_type return_type,
      std::vector<module_::variable_type> arg_types,
      std::string import_library);

    /**
     * Add a type to the export table.
     *
     * @param ctx The context used for type resolution.
     * @param type The type definition.
     */
    void add_type(
      const cg::context& ctx,
      const std::unique_ptr<cg::struct_>& type);

    /**
     * Add a constant to the export table.
     *
     * @param name The constant's name.
     * @param i Index into the constant table.
     */
    void add_constant(std::string name, std::size_t i);

    /**
     * Add a macro to the export table.
     *
     * @param name The macro's name.
     * @param desc Macro descriptor.
     */
    void add_macro(
      std::string name,
      module_::macro_descriptor desc);

    /**
     * Get the index of a symbol.
     *
     * @param t The symbol type.
     * @param name The symbol name.
     * @return Returns the index of the symbol in the export table.
     * @throw Throws an `emitter_error` if the symbol is not found.
     */
    std::size_t get_index(module_::symbol_type t, const std::string& name) const;

    /**
     * Write the export table to a module.
     *
     * @param mod The module to write to.
     */
    void write(module_::language_module& mod) const;
};

class instruction_emitter
{
    /** The associated codegen context. */
    cg::context& ctx;

    /** Memory buffer for instruction emission. */
    memory_write_archive instruction_buffer;

    /** Exports. */
    export_table_builder exports;

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
     * Collect imports by inspecting the instructions `invoke` and `new`.
     */
    void collect_imports();

    /**
     * Emit an instruction.
     *
     * @param func The current function.
     * @param instr The instruction to emit.
     */
    void emit_instruction(
      const std::unique_ptr<cg::function>& func,
      const std::unique_ptr<cg::instruction>& instr);

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
    explicit instruction_emitter(cg::context& ctx)
    : ctx{ctx}
    , instruction_buffer{false, std::endian::little}
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
    module_::language_module to_module() const;
};

}    // namespace slang
