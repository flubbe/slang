/**
 * slang - a simple scripting language.
 *
 * Integration example.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstddef>
#include <print>

#include "interpreter/interpreter.h"
#include "runtime/utils.h"

namespace si = slang::interpreter;
namespace rt = slang::runtime;

/** A struct that is mirrored in the script. */
struct S
{
    std::string* s{nullptr};
    std::int32_t i{0};
};

/**
 * Register native functions and types.
 *
 * @param ctx The context to register the functions in.
 */
static void register_native(si::context& ctx)
{
    // Statically validate that our struct is compatible.
    static_assert(std::is_standard_layout_v<S>);

    // Register a struct.
    std::vector<std::size_t> layout = {
      offsetof(S, s) /* offset of the `std::string`*/
    };
    ctx.get_gc().register_type_layout(si::make_type_name("native_integration", "S"), layout);

    // Register a native function.
    ctx.register_native_function(
      "slang",
      "print",
      [&ctx](si::operand_stack& stack)
      {
          auto [gc_container] = rt::get_args<rt::gc_object<std::string>>(ctx, stack);
          std::print("{}", *gc_container.get());
      });
}

/** Entry point. */
// NOLINTNEXTLINE(bugprone-exception-escape)
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Set up file manager and search paths. These are used for module imports.
    // We set it up so that we can run from the repository base folder, and from
    // within the examples folder.
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("examples");

    // Interpreter context.
    si::context ctx{file_mgr};

    // Register functions and types to be used by the interpreter.
    register_native(ctx);

    // Get the layout id of the struct we want to use.
    std::size_t layout_id = ctx.get_gc().get_type_layout_id(si::make_type_name("native_integration", "S"));

    try
    {
        // This will load the module when invoked first.
        ctx.resolve_module("native_integration");
    }
    catch(const slang::file_error& err)
    {
        std::print("Error: {}\n", err.what());
        return EXIT_FAILURE;
    }

    // The module is loaded here. This call just gets a reference.
    si::module_loader& loader = ctx.resolve_module("native_integration");

    // Find the function to invoke.
    if(!loader.has_function("test"))
    {
        std::print("Cannot find function 'test' in module.\n");
        return EXIT_FAILURE;
    }
    si::function& function = loader.get_function("test");

    // Set up argument for function call.
    std::string str = "Hello from native code!";
    S s = S{&str, 123};    // NOLINT(readability-magic-numbers)

    // Invoke the function.
    si::value res;
    try
    {
        res = function(si::value{layout_id, &s}, 3.141f);    // NOLINT(readability-magic-numbers)
    }
    catch(const si::interpreter_error& err)
    {
        std::print("Error: {}\n", err.what());
        return EXIT_FAILURE;
    }

    // Print the result.
    S* ret_s = static_cast<S*>(*res.get<void*>());
    if(ret_s == nullptr)
    {
        // `res` can be `nullptr` if the return type was not an object.
        std::print("Got unexpected return type.\n");
        return EXIT_FAILURE;
    }

    if(ret_s->s == nullptr)
    {
        std::print("Received null instead of string.\n");
    }
    else
    {
        std::print("String: {}\n", *ret_s->s);
    }
    std::print("Value: {}\n", ret_s->i);

    // Clean up return values.
    ctx.get_gc().remove_temporary(ret_s);
    ctx.get_gc().run();

    // Check that the memory was cleaned up.
    if(ctx.get_gc().object_count() != 0)
    {
        std::print("GC: There are objects left.\n");
    }
    if(ctx.get_gc().root_set_size() != 0)
    {
        std::print("GC: There are roots left.\n");
    }
    if(ctx.get_gc().byte_size() != 0)
    {
        std::print("GC: There is memory left.\n");
    }
}
