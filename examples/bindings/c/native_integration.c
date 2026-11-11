/**
 * slang - a simple scripting language.
 *
 * Integration example (C port of examples/native_integration.cpp).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slang.h"

/** Convenience macro for array element count. */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/**
 * A struct that is mirrored in the script (`native_integration.sl`).
 *
 * @note Must stay layout-compatible with the C++ `struct S`. It relies on the
 *     struct being standard-layout; the reference field `s` is the only
 *     reference, so it is the only entry in the registered type layout. Its
 *     type is the opaque `slang_string` handle (pointer-sized, matching the
 *     C++ `std::string*`).
 */
struct s_native
{
    int8_t a;
    int8_t b;
    int16_t c;
    slang_string s; /* C++: std::string* -- the only reference field. */
    int32_t i;
};

/* Sanity checks */
static_assert(offsetof(struct s_native, a) == 0);
static_assert(offsetof(struct s_native, b) == 1);
static_assert(offsetof(struct s_native, c) == 2);
static_assert(offsetof(struct s_native, i) == offsetof(struct s_native, s) + sizeof(slang_string));

/**  Native print function. */
static void
  slang_print(
    slang_context* context,
    slang_operand_stack stack,
    void* user_data)    // `user_data` is currently unused.
{
    (void)user_data;

    size_t length = 0;
    const char* data = nullptr;

    slang_stack_pop_string(
      context,
      stack,
      &length,
      &data);
    fwrite(data, 1, length, stdout);
}

/**
 * Register native functions and types.
 *
 * @param context The context to register the functions in.
 * @returns `SLANG_STATUS_OK` on success, or a non-OK `slang_status`.
 */
static slang_status
  register_native(
    slang_context* context)
{
    /*
     * Register a struct layout. `offsets` holds the byte offset of every
     * reference field.
     */
    static const size_t s_layout[] = {
      offsetof(struct s_native, s), /* offset of the `std::string` */
    };

    slang_status status =
      slang_type_register_layout(
        context,
        "native_integration.S",
        s_layout,
        ARRAY_SIZE(s_layout));
    if(status != SLANG_STATUS_OK)
    {
        return status;
    }

    /* Register a native function. */
    status =
      slang_function_register_native(
        context,
        "slang",
        "print",
        slang_print,
        nullptr);
    if(status != SLANG_STATUS_OK)
    {
        return status;
    }

    return SLANG_STATUS_OK;
}

/** Entry point. */
int main(int argc, char* argv[])
{
    /* Set up search paths for module imports. */
    slang_context* context = slang_context_create();
    if(context == nullptr)
    {
        fprintf(stderr, "Error: could not create context.\n");
        return EXIT_FAILURE;
    }

    if(slang_context_add_search_path(context, ".") != SLANG_STATUS_OK
       || slang_context_add_search_path(context, "examples") != SLANG_STATUS_OK)
    {
        fprintf(stderr, "Error: %s\n", slang_context_error_string(context));
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    /*
     * Register functions and types to be used by the interpreter.
     *
     * This must happen before `slang_context_resolve_module`, which
     * resolves imported native functions (`print`) while loading.
     */
    if(register_native(context) != SLANG_STATUS_OK)
    {
        fprintf(stderr, "Error: %s\n", slang_context_error_string(context));
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    /* Get the layout id of the struct we want to use. */
    size_t layout_id = 0;
    if(slang_context_get_type_layout_id(
         context,
         "native_integration.S",
         &layout_id)
       != SLANG_STATUS_OK)
    {
        fprintf(stderr, "Error: %s\n", slang_context_error_string(context));
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    /* Resolve the module. This loads it on first call. */
    slang_module module = nullptr;
    slang_status status =
      slang_context_resolve_module(context, "native_integration", &module);
    if(status != SLANG_STATUS_OK)
    {
        fprintf(stderr, "Error: %s\n", slang_context_error_string(context));
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    /* Find the function to invoke. */
    bool has_test = false;
    if(slang_module_has_function(context, module, "test", &has_test) != SLANG_STATUS_OK
       || !has_test)
    {
        fprintf(stderr, "Cannot find function 'test' in module.\n");
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    /*
     * Set up the argument for the function call.
     *
     * The C++ code builds a `std::string str = "Hello from native code!"` and
     * stores `&str` in `S.s`. C cannot name a `std::string`, so we create one
     * through the binding as an opaque handle and store it in `s`. It is not a
     * GC object (mirroring the C++ stack string); we free it ourselves.
     */
    const char* str_literal = "Hello from native code!";
    slang_string str =
      slang_string_create(strlen(str_literal), str_literal);

    struct s_native s = {
      .a = 12,    // NOLINT(readability-magic-numbers)
      .b = 3,
      .c = 99,    // NOLINT(readability-magic-numbers)
      .s = str,
      .i = 123,    // NOLINT(readability-magic-numbers)
    };

    /* Invoke the function by handle. */
    slang_function function = nullptr;
    status =
      slang_module_get_function(context, module, "test", &function);
    if(status != SLANG_STATUS_OK)
    {
        fprintf(stderr, "Error: %s\n", slang_context_error_string(context));
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    slang_value args[2] = {
      slang_value_from_type(layout_id, &s),
      slang_value_from_f32(3.141f),    // NOLINT(readability-magic-numbers)
    };

    slang_value result = {0};
    status =
      slang_function_invoke(
        context,
        function,
        args,
        ARRAY_SIZE(args),
        &result);
    if(status != SLANG_STATUS_OK)
    {
        fprintf(stderr, "Error: %s\n", slang_context_error_string(context));
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    /* Read the result. */
    if(result.kind != SLANG_VALUE_TYPE
       || result.type.data == nullptr)
    {
        fprintf(stderr, "Got unexpected return type.\n");
        slang_context_destroy(context);
        return EXIT_FAILURE;
    }

    struct s_native* const ret_s =
      (struct s_native*)result.type.data;

    /* Read back the (script-created) string field. */
    if(ret_s->s == nullptr)
    {
        fprintf(stderr, "Received null instead of string.\n");
    }
    else
    {
        size_t length = 0;
        const char* data = nullptr;
        slang_string_view(ret_s->s, &length, &data);
        printf("String: %.*s\n", (int)length, data);
    }
    printf("Value a: %d\n", ret_s->a);
    printf("Value b: %d\n", ret_s->b);
    printf("Value c: %d\n", ret_s->c);
    printf("Value i: %d\n", ret_s->i);

    /* Clean up return values. */
    slang_context_gc_remove_temporary(context, ret_s);
    slang_context_gc_run(context);

    /* Check that the memory was cleaned up. */
    size_t object_count = 0;
    size_t root_set_size = 0;
    size_t byte_size = 0;
    if(slang_context_gc_object_count(context, &object_count) == SLANG_STATUS_OK
       && object_count != 0)
    {
        printf("GC: There are objects left.\n");
    }
    if(slang_context_gc_root_set_size(context, &root_set_size) == SLANG_STATUS_OK
       && root_set_size != 0)
    {
        printf("GC: There are roots left.\n");
    }
    if(slang_context_gc_byte_size(context, &byte_size) == SLANG_STATUS_OK
       && byte_size != 0)
    {
        printf("GC: There is memory left.\n");
    }

    /* Free the input string we allocated (the C++ stack string equivalent). */
    slang_string_destroy(str);

    slang_context_destroy(context);
    return EXIT_SUCCESS;
}
