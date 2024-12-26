/**
 * slang - a simple scripting language.
 *
 * module loader.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <filesystem>
#include <string>
#include <unordered_map>

#include "interpreter/types.h"
#include "module.h"

namespace slang::interpreter
{

namespace fs = std::filesystem;

/*
 * Forward declarations.
 */
class context; /* from interpreter/interpreter.h */

/** A module loader. Represents a loaded module, and is associated to an interpreter context. */
class module_loader
{
    /** The associated interpreter context. */
    context& ctx;

    /** The module's path. */
    fs::path path;

    /** Module. */
    module_::language_module mod;

    /** Decoded types, ordered by name. */
    std::unordered_map<std::string, module_::struct_descriptor> struct_map;

    /** Decoded functions, ordered by name. */
    std::unordered_map<std::string, function> function_map;

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
     * Calculate the stack size delta from a function's signature.
     *
     * @param s The signature.
     * @returns The stack size delta.
     */
    std::int32_t get_stack_delta(const module_::function_signature& s) const;

    /**
     * Decode the structs. Set types sizes, alignments and offsets.
     */
    void decode_structs();

    /**
     * Decode a module.
     */
    void decode();

    /**
     * Decode the function's arguments and locals.
     *
     * @param desc The function descriptor.
     */
    void decode_locals(module_::function_descriptor& desc);

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
     * @param path The module's path.
     */
    module_loader(context& ctx, fs::path path);

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
