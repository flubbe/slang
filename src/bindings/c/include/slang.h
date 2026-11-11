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

#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

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
        SLANG_STATUS_OK = 0,
        SLANG_STATUS_ERROR = -1,
        SLANG_STATUS_INVALID_ARGUMENT = -2,
        SLANG_STATUS_FILESYSTEM_ERROR = -3
    } slang_status;

    /** Return the last error, as a string. Returns an empty string if the context was invalid. */
    const char* slang_context_error_string(
      slang_context* context);

    /*
     * Context.
     */

    /** Create a context. */
    slang_context* slang_context_create(void);

    /** Delete a context. */
    void slang_context_destroy(
      slang_context* context);

    /**
     * Add a search path for module resolution.
     *
     * @param context Slang context.
     * @param path Path to add.
     * @returns `SLANG_STATUS_OK` on success, and non-null as specified in `slang_status` on error.
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
     * @returns `SLANG_STATUS_OK` on success, and non-null as specified in `slang_status` on error.
     */
    slang_status slang_context_get_type_layout_id(
      slang_context* context,
      const char* name,
      size_t* out_layout_id);

    /*
     * Modules.
     */

    /**
     * Resolve a module given it's import name. This loads the module if it is not already loaded.
     *
     * @param context Slang context.
     * @param name Module (import) name.
     * @param out_module If non-null, output for the resolved module.
     * @returns `SLANG_STATUS_OK` on success, and non-null as specified in `slang_status` on error.
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
     * @returns `SLANG_STATUS_OK` on success, or an error status.
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
     * @returns `SLANG_STATUS_OK` on success, and non-null as specified in `slang_status` on error.
     */
    slang_status slang_module_get_function(
      slang_context* context,
      slang_module module,
      const char* name,
      slang_function* out_function);

    /*
     * Values.
     */

    typedef enum slang_value_kind
    {
        SLANG_VALUE_VOID,
        SLANG_VALUE_I32,
        SLANG_VALUE_F32,
        SLANG_VALUE_STR,
        SLANG_VALUE_TYPE
    } slang_value_kind;

    typedef struct slang_value
    {
        slang_value_kind kind;

        union
        {
            int32_t i32;
            float f32;

            struct
            {
                size_t length;
                const char* data;
            } str;

            struct
            {
                size_t layout_id;
                const void* data;
            } type;
        };
    } slang_value;

    static inline slang_value slang_value_i32(
      int32_t value)
    {
        return (slang_value){
          .kind = SLANG_VALUE_I32,
          .i32 = value};
    }

    static inline slang_value slang_value_f32(
      float value)
    {
        return (slang_value){
          .kind = SLANG_VALUE_F32,
          .f32 = value};
    }

    inline slang_value slang_value_str(
      size_t length,
      const char* value)
    {
        return (slang_value){
          .kind = SLANG_VALUE_STR,
          .str = {
            .length = length,
            .data = value}};
    }

    inline slang_value slang_value_cstr(
      const char* str)
    {
        if(str == nullptr)
        {
            return (slang_value){
              .kind = SLANG_VALUE_STR,
              .str = {
                .length = 0,
                .data = nullptr}};
        }

        return slang_value_str(
          strlen(str),
          str);
    }

    static inline slang_value slang_value_type(
      size_t layout_id,
      const void* data)
    {
        return (slang_value){
          .kind = SLANG_VALUE_TYPE,
          .type = {
            .layout_id = layout_id,
            .data = data}};
    }

    slang_status slang_value_destroy(
      slang_value* value);

    /*
     * Strings.
     */

    slang_string slang_string_new(
      size_t length,
      const char* data);

    void slang_string_destroy(
      slang_string string);

    /**
     * Pop a string from the stack.
     *
     * @note The string is only valid until the next GC run.
     */
    slang_status slang_stack_pop_string(
      slang_context* context,
      slang_operand_stack stack,
      size_t* out_length,
      const char** out_data);

    void slang_string_view(
      slang_string string,
      size_t* out_length,
      const char** out_data);

    /*
     * Types.
     */

    slang_status slang_context_register_type_layout(
      slang_context* context,
      const char* name,
      const size_t* offsets,
      size_t offset_count);

    /*
     * Functions.
     */

    slang_status slang_context_register_native_function(
      slang_context* context,
      const char* module_name,
      const char* function_name,
      slang_native_function_t callback,
      void* user_data);

    slang_status slang_function_invoke(
      slang_context* context,
      slang_function function,
      const slang_value* args,
      size_t arg_count,
      slang_value* out_result);

    /*
     * Garbage collector.
     */

    slang_status slang_context_gc_remove_temporary(
      slang_context* context,
      void* object);

    slang_status slang_context_gc_run(
      slang_context* context);

    slang_status slang_context_gc_object_count(
      slang_context* context,
      size_t* out_count);

    slang_status slang_context_gc_root_set_size(
      slang_context* context,
      size_t* out_count);

    slang_status slang_context_gc_byte_size(
      slang_context* context,
      size_t* out_size);

#ifdef __cplusplus
}
#endif
