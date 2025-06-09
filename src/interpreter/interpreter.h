/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <functional>
#include <unordered_map>
#include <vector>

#include "shared/module.h"
#include "shared/opcodes.h"
#include "module_loader.h"
#include "filemanager.h"
#include "gc.h"
#include "utils.h"

namespace slang::interpreter
{

/**
 * Helper to create a unique type name.
 *
 * @param package_import_name Import name of the package containing the type.
 * @param type_name The type name in the package.
 */
inline std::string make_type_name(
  const std::string& package_import_name,
  const std::string& type_name)
{
    return std::format("{}.{}", package_import_name, type_name);
}

/**
 * Check if a type is garbage collected.
 *
 * @param t The type string.
 * @returns Return whether a type is garbage collected.
 */
bool is_garbage_collected(const module_::variable_type& t) noexcept;

/** Interpreter context. */
class context
{
    friend class module_loader;

    /** Module loaders, indexed by module. */
    std::unordered_map<std::string, std::unique_ptr<module_loader>> loaders;

    /** Native functions, indexed by module and name. */
    std::unordered_map<std::string, std::unordered_map<std::string, std::function<void(operand_stack&)>>> native_function_map;

    /** File manager reference. */
    file_manager& file_mgr;

    /** Maximum call stack depth. */
    unsigned int max_call_stack_depth;

    /** The current call stack level. */
    unsigned int call_stack_level = 0;

    /** Garbage collector. */
    gc::garbage_collector gc;

    /**
     * Resolve a native function.
     *
     * @param name The function's name.
     * @param library_name The function's library name.
     * @returns Returns the resolved function.
     * @throws Throws an `interpreter_error` if the resolution failed.
     */
    std::function<void(operand_stack&)> resolve_native_function(
      const std::string& name,
      const std::string& library_name) const;

    /**
     * Execute a function.
     *
     * @param loader The module loader.
     * @param f The function to execute.
     * @param args The function's arguments.
     * @return The function's return value.
     */
    value exec(
      const module_loader& loader,
      const function& f,
      const std::vector<value>& args);

    /**
     * Execute a function.
     *
     * @param loader The module loader.
     * @param entry_point The function's entry point/offset in the binary buffer.
     * @param size The function's bytecode size.
     * @param locals The locals for the function.
     * @param frame The stack frame for the function.
     * @return The function's return opcode.
     */
    opcode exec(
      const module_loader& loader,
      std::size_t entry_point,
      std::size_t size,
      const std::vector<module_::variable_descriptor>& locals,
      stack_frame& frame);

    /**
     * Stack trace handler for `interpreter_error`s.
     *
     * @param err The interpreter error.
     * @param loader Reference to loader of the module where the error occured.
     * @param entry_point Entry point of the function where the error occured.
     * @param offset Offset into the decoded binary where the error occured.
     */
    void stack_trace_handler(
      interpreter_error& err,
      const module_loader& loader,
      std::size_t entry_point,
      std::size_t offset);

    /**
     * Create a readable message from a stack trace by resolving the entry points
     * to function names.
     *
     * @param stack_trace The stack trace.
     * @returns Returns the stack trace as a readable string.
     */
    std::string stack_trace_to_string(const std::vector<stack_trace_entry>& stack_trace);

public:
    /** Default constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Construct an interpreter context.
     *
     * @param file_mgr The file manager to use for module resolution.
     * @param max_call_stack_depth The maximum allowed function call stack depth.
     */
    context(file_manager& file_mgr, unsigned int max_call_stack_depth = 500)
    : file_mgr{file_mgr}
    , max_call_stack_depth{max_call_stack_depth}
    {
    }

    /** Default destructor. */
    ~context() = default;

    /**
     * Register a native function to a module.
     *
     * @param mod_name The name of the module to bind to.
     * @param fn_name The function's name.
     * @param func The function.
     * @throws Throws a `interpreter_error` if the function given by `mod_name` and `fn_name` is already registered,
     *         or if the function `func` is `nullptr`.
     */
    void register_native_function(const std::string& mod_name, std::string fn_name, std::function<void(operand_stack&)> func);

    /**
     * Resolve a module given it's import name. This loads the module if it is not
     * already loaded.
     *
     * @param import_name The module's import name in the form `a::b::mod_name`.
     * @param recorder An optional instruction recorder for disassembling the module on load.
     * @returns Returns the module loader.
     */
    module_loader& resolve_module(
      const std::string& import_name,
      std::shared_ptr<instruction_recorder> recorder = std::make_shared<instruction_recorder>());

    /**
     * Get the import name for a module loader.
     *
     * @param loader The loader.
     * @returns Returns the import name.
     * @throws Throws an `interpreter_error` if the loader does not exist.
     */
    std::string get_import_name(const module_loader& loader) const;

    /**
     * Invoke a function from a module by name.
     *
     * @param module_name Name of the module of the function.
     * @param function_name The function's name.
     * @param args The function's arguments.
     * @returns The function's return value.
     */
    value invoke(const std::string& module_name, const std::string& function_name, const std::vector<value>& args);

    /**
     * Invoke a function from a module.
     *
     * @param loader The module loader.
     * @param fn The function to invoke.
     * @param args The function's arguments.
     * @returns The function's return value.
     */
    value invoke(const module_loader& loader, const function& fn, const std::vector<value>& args);

    /** Reset the interpreter. Needs to be called when an exception was thrown and caught. */
    void reset()
    {
        call_stack_level = 0;
        gc.reset();
    }

    /** Garbage collector access. */
    gc::garbage_collector& get_gc()
    {
        return gc;
    }
};

}    // namespace slang::interpreter
