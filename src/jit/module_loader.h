/**
 * slang - a simple scripting language.
 *
 * Shared Just In Time module loader.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "jit/common.h"

namespace slang
{
class file_manager;
}    // namespace slang

namespace slang::jit
{

class module_loader;

/** An entry in the import table. */
struct imported_symbol
{
    /** Symbol type. */
    module_::symbol_type type;

    /** Symbol name. */
    std::string name;

    /** Index into the package import table. Unused for package imports. */
    std::uint32_t package_index;

    /** Resolved module or export reference. Not serialized. */
    std::variant<
      const module_loader*,
      const module_::exported_symbol*>
      export_reference;
};

/** Runtime header state owned by the JIT loader. */
struct module_header
{
    /** Import table. */
    std::vector<imported_symbol> imports;

    /** Export table. */
    std::vector<module_::exported_symbol> exports;

    /** Constant table. */
    std::vector<module_::constant_table_entry> constants;
};

/** A module loader. Represents a loaded module with JITted bytecode. */
class module_loader
{
    /** Shared file manager for resolving imported modules. */
    slang::file_manager& file_mgr;

    /** Runtime context shared with this module and its imports. */
    interpreter::context* runtime_context{nullptr};

    /** The module's import name. */
    std::string import_name;

    /** The module's path. */
    fs::path path;

    /** JIT-owned runtime header. */
    module_header header;

    /** JIT-owned bytecode. */
    std::vector<std::byte> binary;

    /** Decoded types, ordered by name. */
    std::unordered_map<std::string, module_::struct_descriptor> struct_map;

    /** Compiled functions, ordered by name. */
    std::unordered_map<std::string, jit_function> function_map;

    /** Stable call targets, indexed by module export index. */
    std::vector<std::unique_ptr<jit_call_target>> call_targets;

    /** Imported modules. Owns imported call targets. */
    std::unordered_map<std::string, std::unique_ptr<module_loader>> imported_modules;

    /** Non-owning call targets indexed by module import index. */
    std::vector<jit_call_target*> import_call_targets;

    /** Resolve and retain an imported module. */
    module_loader& resolve_module(const std::string& module_name);

    /** Resolve exports into stable call target records for module exports. */
    void resolve_exports();

    /** Resolve imported symbols and populate the import-indexed call target table. */
    void resolve_imports();

    /** Compile all non-native functions in the module. */
    void compile_functions(
      const std::vector<std::unique_ptr<jit_call_target>>& call_targets);

    /** Bind matching native exports in this module and its dependencies. */
    std::size_t register_native_function_recursive(
      const std::string& library_name,
      const std::string& function_name,
      const std::function<void(interpreter::operand_stack&)>& function);

    /** Decode structs, setting their sizes, alignments, and offsets. */
    void decode_structs();

public:
    module_loader() = delete;
    module_loader(const module_loader&) = delete;
    module_loader(module_loader&& other) noexcept;
    module_loader& operator=(const module_loader&) = delete;
    module_loader& operator=(module_loader&&) = delete;

    /**
     * Create a module loader.
     *
     * @param file_mgr File manager.
     * @param import_name The module's import name.
     * @param path The module's path.
     * @param runtime_context Runtime context shared with this module and its imports.
     */
    module_loader(
      slang::file_manager& file_mgr,
      std::string import_name,
      fs::path path,
      interpreter::context* runtime_context);

    /** Get the module constant table. */
    const std::vector<module_::constant_table_entry>& get_constant_table() const
    {
        return header.constants;
    }

    /** Get a compiled function by its exported name. */
    jit_function& get_function(const std::string& name);

    /** Bind a native export to a host callback. */
    void register_native_function(
      const std::string& library_name,
      const std::string& function_name,
      const std::function<void(interpreter::operand_stack&)>& function);

    /** Resolve all native exports using a caller-provided runtime callback. */
    void register_native_functions(
      const std::function<native_function_type(const std::string&, const std::string&)>& resolver);
};

}    // namespace slang::jit
