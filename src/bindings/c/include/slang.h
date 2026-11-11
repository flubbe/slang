/**
 * slang - a simple scripting language.
 *
 * C bindings.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Assert float layout.
 */

static_assert(sizeof(float) * CHAR_BIT == 32, "float must be 32 bits");
static_assert(FLT_RADIX == 2, "Expected binary floating point");
static_assert(FLT_MANT_DIG == 24, "Expected IEEE 754 binary32 precision");
static_assert(FLT_MAX_EXP == 128, "Expected IEEE 754 binary32 exponent range");

/*
 * Helpers.
 */

#ifdef __cplusplus
#    define SLANG_COMPOUND_LITERAL(type, ...) \
        type                                  \
        {                                     \
            __VA_ARGS__                       \
        }
#else
#    define SLANG_COMPOUND_LITERAL(type, ...) \
        (type)                                \
        {                                     \
            __VA_ARGS__                       \
        }
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    /*
     * Type definitions.
     */

    /** Opaque context handle. */
    typedef struct slang_context slang_context;

    /** Opaque module handle. */
    typedef struct slang_module_impl* slang_module;

    /** Opaque operand stack. */
    typedef struct slang_operand_stack_impl* slang_operand_stack;

    /** Opaque function handle. */
    typedef struct slang_function_impl* slang_function;

    /** Native function pointer. */
    typedef void (*slang_native_function_t)(
      slang_context* context,
      slang_operand_stack stack,
      void* user_data);

    /** Opaque string handle. */
    typedef struct slang_string_impl* slang_string;

    /*
     * Errors.
     */

    /** Error codes. */
    typedef enum slang_status
    {
        SLANG_STATUS_OK = 0,                /**< Success. */
        SLANG_STATUS_ERROR = -1,            /**< Generic error. */
        SLANG_STATUS_INVALID_ARGUMENT = -2, /**< Passed arguments were invalid. */
        SLANG_STATUS_FILESYSTEM_ERROR = -3, /**< A filesystem operation failed. */
        SLANG_STATUS_GC_ERROR = -4,         /**< A garbage collector error. */
    } slang_status;

    /*
     * Values.
     */

    /** Values kinds supported by the API. */
    typedef enum slang_value_kind
    {
        SLANG_VALUE_VOID, /**< `void` */
        SLANG_VALUE_I32,  /**< `int32_t` */
        SLANG_VALUE_F32,  /**< `float`. */
        SLANG_VALUE_STR,  /**< String.  */
        SLANG_VALUE_TYPE  /**< Custom type. */
    } slang_value_kind;

    /**
     * A value to be passed across the native/interpreter boundary.
     *
     * Strings and references are borrowed, i.e., managed by the user.
     * Their underlying memory must remain valid for the duration of
     * any API call that uses the value.
     */
    typedef struct slang_value
    {
        /** Value kind. */
        slang_value_kind kind;

        union
        {
            int32_t i32;
            float f32;

            struct
            {
                size_t length;    /**< String length (excluding null-terminator). */
                const char* data; /**< String data. */
            } str;

            struct
            {
                size_t layout_id; /**< Type layout id. */
                const void* data; /**< Raw type data. */
            } type;
        };
    } slang_value;

    /*
     * Errors.
     */

    /**
     * Return the last error, as a string.
     * Returns an empty string if the context was invalid.
     *
     * The string is valid until the next API call on the context.
     *
     * @param context The context.
     * @returns The error string.
     */
    const char* slang_context_error_string(
      slang_context* context);

    /*
     * Context.
     */

    /** Create a context. Must be destroyed with `slang_context_destroy`. */
    slang_context* slang_context_create(void);

    /**
     * Destroy a context.
     *
     * @note No-op if `nullptr` is passed in.
     * @param context The context to destroy.
     */
    void slang_context_destroy(
      slang_context* context);

    /**
     * Add a search path for module resolution.
     *
     * @param context Slang context.
     * @param path Path to add.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_add_search_path(
      slang_context* context,
      const char* path);

    /**
     * Get the layout id of a type.
     *
     * @param context Slang context.
     * @param name Fully qualified type name.
     * @param out_layout_id Output for the layout id.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_get_type_layout_id(
      slang_context* context,
      const char* name,
      size_t* out_layout_id);

    /*
     * Modules.
     */

    /**
     * Resolve a module given its import name. This loads the module if it is not already loaded.
     *
     * @param context Slang context.
     * @param name Module (import) name.
     * @param out_module If non-null, output for the resolved module.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_resolve_module(
      slang_context* context,
      const char* name,
      slang_module* out_module);

    /**
     * Checks whether the module contains a function.
     *
     * @param context The context.
     * @param module The module.
     * @param name Name of the function.
     * @param out_result Receives whether the function exists.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_module_has_function(
      slang_context* context,
      slang_module module,
      const char* name,
      bool* out_result);

    /**
     * Get a function from a module.
     *
     * @param context The context.
     * @param module The module.
     * @param name Name of the function.
     * @param out_function The function.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_module_get_function(
      slang_context* context,
      slang_module module,
      const char* name,
      slang_function* out_function);

    /*
     * Values.
     */

    /** Create a value from an `i32`. */
    static inline slang_value slang_value_from_i32(
      int32_t value)
    {
        return SLANG_COMPOUND_LITERAL(
          slang_value,
          .kind = SLANG_VALUE_I32,
          .i32 = value);
    }

    /** Create a value from a `float`. */
    static inline slang_value slang_value_from_f32(
      float value)
    {
        return SLANG_COMPOUND_LITERAL(
          slang_value,
          .kind = SLANG_VALUE_F32,
          .f32 = value);
    }

    /**
     * Create a value from a (not necessarily null-terminated) string.
     *
     * @note The memory `data` is referring to has to stay valid until script execution returns.
     * @param length The string length.
     * @param data The string data.
     * @returns Returns the string as a `slang_value`.
     */
    static inline slang_value slang_value_from_str(
      size_t length,
      const char* data)
    {
        return SLANG_COMPOUND_LITERAL(
          slang_value,
          .kind = SLANG_VALUE_STR,
          .str = {
            .length = length,
            .data = data});
    }

    /**
     * Create a value from a null-terminated string.
     *
     * @note The memory `str` is referring to has to stay valid until script execution returns.
     * @param str The string data.
     * @returns Returns the string as a `slang_value`.
     */
    static inline slang_value slang_value_from_cstr(
      const char* str)
    {
        if(str == nullptr)
        {
            return SLANG_COMPOUND_LITERAL(
              slang_value,
              .kind = SLANG_VALUE_STR,
              .str = {
                .length = 0,
                .data = nullptr});
        }

        return slang_value_from_str(
          strlen(str),
          str);
    }

    /**
     * Create a value from a type.
     *
     * @note The memory `data` is referring to has to stay valid until script execution returns.
     * @param layout_id The type layout id.
     * @param data The raw data.
     * @returns Returns the type value as a `slang_value`.
     */
    static inline slang_value slang_value_from_type(
      size_t layout_id,
      const void* data)
    {
        return SLANG_COMPOUND_LITERAL(
          slang_value,
          .kind = SLANG_VALUE_TYPE,
          .type = {
            .layout_id = layout_id,
            .data = data});
    }

    /**
     * Destroy a `slang_value`. Releases the underlying memory (if any).
     *
     * @param value The value to destroy.
     */
    slang_status slang_value_destroy(
      slang_value* value);

    /*
     * Strings.
     */

    /**
     * Create a new string compatible with the interpreter's internal string
     * representation. The string data is copied into internal memory.
     *
     * This is useful when initializing string fields in runtime structures,
     * for example:
     *
     * ```c
     * slang_string str = slang_string_create(1, "S");
     * struct s_native s = {.s = str};
     *
     * // Pass native to the interpreter...
     *
     * slang_string_destroy(str);
     * ```
     *
     * The resulting string must remain valid until script execution returns.
     * Destroy after use with `slang_string_destroy`.
     *
     * @param length The string length.
     * @param data The string data.
     * @returns Returns a string in the internal representation.
     */
    slang_string slang_string_create(
      size_t length,
      const char* data);

    /** Destroy a string created with `slang_string_create`. */
    void slang_string_destroy(
      slang_string string);

    /**
     * Pop a string from the stack.
     *
     * This is useful when getting parameters from the stack in native functions.
     *
     * @note The string (referred to by `out_data`) is only valid until the next GC run.
     * @param context The context.
     * @param stack The function's stack.
     * @param out_length If non-null, the string length is written here.
     * @param out_data If non-null, a reference to the string data is written here.
     * @returns `SLANG_STATUS_OK` on success, or `SLANG_STATUS_INVALID_ARGUMENT` if `context` was `nullptr´.
     */
    slang_status slang_stack_pop_string(
      slang_context* context,
      slang_operand_stack stack,
      size_t* out_length,
      const char** out_data);

    /**
     * Get a read-only view of a string.
     *
     * The returned data is owned by the string and remains valid
     * until the string is destroyed.
     *
     * @param string The string.
     * @param out_length If non-null, the string length is written here.
     * @param out_data If non-null, a reference to the string data is written here.
     */
    void slang_string_view(
      slang_string string,
      size_t* out_length,
      const char** out_data);

    /*
     * Types.
     */

    /**
     * Register a type layout.
     *
     * @param context The context.
     * @param name The name, as `module_name.type_name`.
     * @param offsets The type layout, given as a list of offsets of the contained pointers.
     * @param offset_count Number of offsets.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_type_register_layout(
      slang_context* context,
      const char* name,
      const size_t* offsets,
      size_t offset_count);

    /*
     * Functions.
     */

    /**
     * Register a native function.
     *
     * @param context The context.
     * @param module_name Name of the module containing the function.
     * @param function_name The function's name.
     * @param callback Callback that is invoked when the function is called.
     * @param user_data User data passed to the callback.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_function_register_native(
      slang_context* context,
      const char* module_name,
      const char* function_name,
      slang_native_function_t callback,
      void* user_data);

    /**
     * Invoke a function.
     *
     * @param context The context.
     * @param function The function to invoke.
     * @param args Function arguments array.
     * @param arg_count Argument count.
     * @param out_result Result of the function call.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_function_invoke(
      slang_context* context,
      slang_function function,
      const slang_value* args,
      size_t arg_count,
      slang_value* out_result);

    /*
     * Garbage collector.
     */

    /**
     * Remove a temporary object (or decrease it's reference count).
     *
     * @note This needs to be called on string and array types, that are:
     *     1. passed to native functions, or
     *     2. returned from invoke to native code.
     * @note If `nullptr` is passed in `object`, the function is a no-op.
     *
     * @param context The context.
     * @param object The object.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_gc_remove_temporary(
      slang_context* context,
      void* object);

    /**
     * Run garbage collection.
     *
     * @param context The context.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_gc_run(
      slang_context* context);

    /**
     * Get allocated object count.
     *
     * @param context The context.
     * @param out_count Output for the object count.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_gc_object_count(
      slang_context* context,
      size_t* out_count);

    /**
     * Get the root set size.
     *
     * @param context The context.
     * @param out_count Output for the root set size.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_gc_root_set_size(
      slang_context* context,
      size_t* out_count);

    /**
     * Get allocated bytes.
     *
     * @param context The context.
     * @param out_count Output for the allocated bytes.
     * @returns `SLANG_STATUS_OK` on success, and an error status on failure.
     */
    slang_status slang_context_gc_byte_size(
      slang_context* context,
      size_t* out_size);

#ifdef __cplusplus
}
#endif
