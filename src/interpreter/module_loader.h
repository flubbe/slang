/**
 * slang - a simple scripting language.
 *
 * module loader.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <filesystem>
#include <string>
#include <unordered_map>

#include "shared/module.h"
#include "types.h"

namespace slang::interpreter
{

namespace fs = std::filesystem;

/*
 * Forward declarations.
 */
class context; /* from interpreter/interpreter.h */

/** An instruction recorder, e.g. for disassembling a module. */
struct instruction_recorder
{
    /** Default destructor. */
    virtual ~instruction_recorder() = default;

    /**
     * Begin recording a new section.
     *
     * @param name Name of the section.
     */
    virtual void section([[maybe_unused]] const std::string& name)
    {
    }

    /**
     * Record function information.
     *
     * @param name Name of the function.
     * @param details Details of the function.
     */
    virtual void function(
      [[maybe_unused]] const std::string& name,
      [[maybe_unused]] const module_::function_details& details)
    {
    }

    /**
     * Record type information.
     *
     * @param name The type name.
     * @param desc The type descriptor.
     */
    virtual void type(
      [[maybe_unused]] const std::string& name,
      [[maybe_unused]] const module_::struct_descriptor& desc)
    {
    }

    /**
     * Record a constant table entry.
     *
     * @param c The constant.
     */
    virtual void constant(
      [[maybe_unused]] const module_::constant_table_entry& c)
    {
    }

    /**
     * Record an exported symbol.
     *
     * @param s The exported symbol.
     */
    virtual void record(
      [[maybe_unused]] const module_::exported_symbol& s)
    {
    }

    /**
     * Record an imported symbol.
     *
     * @param s The imported symbol.
     */
    virtual void record(
      [[maybe_unused]] const module_::imported_symbol& s)
    {
    }

    /**
     * Add a label.
     *
     * @param index The label's index.
     */
    virtual void label([[maybe_unused]] std::int64_t index)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     */
    virtual void record([[maybe_unused]] opcode instr)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param i An integer parameter.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] std::int64_t i)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param i1 The first integer parameter.
     * @param i2 The second integer parameter.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] std::int64_t i1,
      [[maybe_unused]] std::int64_t i2)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param f A floating-point parameter.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] float f)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param d A double parameter.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] double f)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param i A table index.
     * @param s The table entry.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] std::int64_t i,
      [[maybe_unused]] std::string s)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param i A table index.
     * @param s The table entry.
     * @param field_index A field index.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] std::int64_t i,
      [[maybe_unused]] std::string s,
      [[maybe_unused]] std::int64_t field_index)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param s1 The first string parameter.
     * @param s2 The second string parameter.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] std::string s1,
      [[maybe_unused]] std::string s2)
    {
    }

    /**
     * Record an instruction.
     *
     * @param instr The instruction's opcode.
     * @param s1 The first string parameter.
     * @param s2 The second string parameter.
     * @param s3 The third string parameter.
     */
    virtual void record(
      [[maybe_unused]] opcode instr,
      [[maybe_unused]] std::string s1,
      [[maybe_unused]] std::string s2,
      [[maybe_unused]] std::string s3)
    {
    }
};

/** A module loader. Represents a loaded module, and is associated to an interpreter context. */
class module_loader
{
    /** The associated interpreter context. */
    context& ctx;

    /** The module's import name. */
    std::string import_name;

    /** The module's path. */
    fs::path path;

    /** Module. */
    module_::language_module mod;

    /** Decoded types, ordered by name. */
    std::unordered_map<std::string, module_::struct_descriptor> struct_map;

    /** Decoded functions, ordered by name. */
    std::unordered_map<std::string, function> function_map;

    /** An instruction recorder, for disassembly. */
    std::shared_ptr<instruction_recorder> recorder;

    /**
     * Get the byte size and alignment of a type.
     *
     * @param type The type.
     * @return Returns the type properties.
     * @throws Throws an `interpreter_error` if the type is not known.
     */
    type_properties get_type_properties(const module_::variable_type& type) const;

    /**
     * Get the byte size and offset of a field.
     *
     * @param type_name The base type name.
     * @param field_index The field index.
     * @return Returns the field properties.
     * @throws Throws an `interpreter_error` if the type is not known or the field index outside the type's field array.
     */
    field_properties get_field_properties(const std::string& type_name, std::size_t field_index) const;

    /**
     * Decode the structs. Set types sizes, alignments and offsets.
     */
    void decode_structs();

    /**
     * Decode a module.
     */
    void decode();

    /**
     * Decode an instruction.
     *
     * @param ar The archive to read from.
     * @param instr The instruction to decode.
     * @param details The function's details.
     * @param code Buffer to write the decoded bytes into.
     * @return Delta (in bytes) by which the instruction changes the stack size.
     */
    std::int32_t decode_instruction(
      archive& ar,
      std::byte instr,
      const module_::function_details& details,
      std::vector<std::byte>& code);

    /**
     * Resolve a type. For custom types, that means resolving or validating
     * its layout id. For built-in types, this validates the given type.
     *
     * @param type The type to resolve.
     */
    void resolve_type(module_::variable_type& type) const;

public:
    /** Defaulted and deleted constructors. */
    module_loader() = delete;
    module_loader(const module_loader&) = default;
    module_loader(module_loader&&) = default;

    /** Default assignments. */
    module_loader& operator=(const module_loader&) = delete;
    module_loader& operator=(module_loader&&) = delete;

    /**
     * Create a new module loader.
     *
     * @param ctx The associated interpreter context.
     * @param import_name The module's import name.
     * @param path The module's path.
     * @param recorder An optional instruction recorder.
     */
    module_loader(
      context& ctx,
      std::string import_name,
      fs::path path,
      std::shared_ptr<instruction_recorder> recorder = std::make_shared<instruction_recorder>());

    /**
     * Check if the module contains a function.
     *
     * @param name Name of the function.
     * @returns Returns whether the function exists in the module.
     */
    bool has_function(const std::string& name) const;

    /**
     * Get a function from the module.
     *
     * @param name The function's name.
     * @returns Returns a reference to the function.
     * @throws Throws an `interpreter_error` if the function is not found.
     */
    function& get_function(const std::string& name);

    /** Get the module's path. */
    fs::path get_path() const
    {
        return path;
    }

    /** Get the module data. */
    const module_::language_module& get_module() const
    {
        return mod;
    }

    /**
     * Resolve an entry point to a function name.
     *
     * @param entry_point The entry point.
     * @returns Returns the name of the function at the entry point, or `std::nullopt` if the entry
     *          point could be resolved.
     */
    std::optional<std::string> resolve_entry_point(std::size_t entry_point) const;
};

}    // namespace slang::interpreter
