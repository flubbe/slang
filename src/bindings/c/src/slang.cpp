/**
 * slang - a simple scripting language.
 *
 * C bindings.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter/interpreter.h"
#include "runtime/utils.h"
#include "slang.h"

namespace si = slang::interpreter;
namespace rt = slang::runtime;

/** Context capturing all interpreter runtime objects. */
struct slang_context
{
    /** File manager. */
    slang::file_manager file_mgr;

    /** Interpreter context. */
    si::context context;

    /**
     * Last error string.
     *
     * @note Cleared on every API invokation.
     */
    std::string last_error;

    /** Create a context. */
    slang_context()
    : context{file_mgr}
    {
    }
};

/*
 * Errors.
 */

extern "C" const char* slang_context_error_string(
  slang_context* context)
{
    if(context == nullptr)
    {
        return "Invalid context";
    }

    return context->last_error.c_str();
}

/*
 * Context.
 */

extern "C" slang_context* slang_context_create(void)
{
    try
    {
        return new slang_context{};
    }
    catch(...)
    {
        return nullptr;
    }
}

extern "C" void slang_context_destroy(
  slang_context* context)
{
    delete context;
}

extern "C" slang_status slang_context_add_search_path(
  slang_context* context,
  const char* path)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(path == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    try
    {
        context->file_mgr.add_search_path(path);
        return SLANG_STATUS_OK;
    }
    catch(const std::filesystem::filesystem_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_FILESYSTEM_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

extern "C" slang_status slang_context_get_type_layout_id(
  slang_context* context,
  const char* name,
  size_t* out_layout_id)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(name == nullptr
       || out_layout_id == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    try
    {
        *out_layout_id = context->context.get_type_layout_id(
          name);

        return SLANG_STATUS_OK;
    }
    catch(const slang::gc::gc_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_GC_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

/*
 * Modules.
 */

extern "C" slang_status slang_context_resolve_module(
  slang_context* context,
  const char* name,
  slang_module* module)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(name == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    if(module != nullptr)
    {
        *module = nullptr;
    }

    try
    {
        auto& loader = context->context.resolve_module(name);

        if(module != nullptr)
        {
            *module = reinterpret_cast<slang_module>(&loader);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        }

        return SLANG_STATUS_OK;
    }
    catch(const slang::file_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_FILESYSTEM_ERROR;
    }
    catch(const slang::gc::gc_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_GC_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

extern "C" slang_status slang_module_has_function(
  slang_context* context,
  slang_module module,
  const char* name,
  bool* out_result)
{
    if(out_result == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    *out_result = false;

    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(module == nullptr
       || name == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    auto* loader = reinterpret_cast<slang::interpreter::module_loader*>(module);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    try
    {
        *out_result = loader->has_function(
          name);
        return SLANG_STATUS_OK;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

extern "C" slang_status slang_module_get_function(
  slang_context* context,
  slang_module module,
  const char* name,
  slang_function* out_function)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(module == nullptr
       || name == nullptr
       || out_function == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();
    *out_function = nullptr;

    auto* loader = reinterpret_cast<slang::interpreter::module_loader*>(module);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    try
    {
        *out_function = reinterpret_cast<slang_function>(&loader->get_function(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          name));
        return SLANG_STATUS_OK;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

/*
 * Values.
 */

extern "C" slang_status slang_value_destroy(
  slang_context* context,
  slang_value* value)
{
    if(context == nullptr
       || value == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    if(value->kind == SLANG_VALUE_STR)
    {
        std::free(              // NOLINT(cppcoreguidelines-no-malloc)
          const_cast<void*>(    // NOLINT(cppcoreguidelines-pro-type-const-cast)
            static_cast<const void*>(
              value->data.str.data)));     // NOLINT(cppcoreguidelines-pro-type-union-access)
        value->data.str.data = nullptr;    // NOLINT(cppcoreguidelines-pro-type-union-access)
        value->data.str.length = 0;        // NOLINT(cppcoreguidelines-pro-type-union-access)
    }
    else if(value->kind == SLANG_VALUE_TYPE)
    {
        auto* object = const_cast<void*>(value->data.type.data);
        context->context.get_gc().remove_temporary(object);
        value->data.type.data = nullptr;
    }

    return SLANG_STATUS_OK;
}

/*
 * Strings.
 */

extern "C" slang_string slang_string_create(
  size_t length,
  const char* data)
{
    return reinterpret_cast<slang_string>(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      new std::string{data, length});
}

extern "C" void slang_string_destroy(
  slang_string string)
{
    delete reinterpret_cast<std::string*>(string);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

extern "C" slang_status slang_stack_push_i32(
  slang_context* context,
  slang_operand_stack stack,
  int32_t value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        reinterpret_cast<si::operand_stack*>(stack)->push_cat1(value);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    catch(const slang::interpreter::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_push_f32(
  slang_context* context,
  slang_operand_stack stack,
  float value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        reinterpret_cast<si::operand_stack*>(stack)->push_cat1(value);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    catch(const slang::interpreter::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_push_i64(
  slang_context* context,
  slang_operand_stack stack,
  int64_t value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        reinterpret_cast<si::operand_stack*>(stack)->push_cat2(value);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    catch(const slang::interpreter::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_push_f64(
  slang_context* context,
  slang_operand_stack stack,
  double value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        reinterpret_cast<si::operand_stack*>(stack)->push_cat2(value);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    catch(const slang::interpreter::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_push_string(
  slang_context* context,
  slang_operand_stack stack,
  size_t length,
  const char* data)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        auto* gc_string = context->context.get_gc().gc_new<std::string>(
          slang::gc::gc_object::of_temporary);
        gc_string->assign({data, length});

        reinterpret_cast<si::operand_stack*>(stack)->push_addr(gc_string);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    catch(const slang::gc::gc_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_GC_ERROR;
    }
    catch(const slang::interpreter::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_pop_i32(
  slang_context* context,
  slang_operand_stack stack,
  int32_t* out_value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    auto value = reinterpret_cast<si::operand_stack*>(stack)->pop_cat1<std::int32_t>();    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if(out_value != nullptr)
    {
        *out_value = value;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_pop_f32(
  slang_context* context,
  slang_operand_stack stack,
  float* out_value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    auto value = reinterpret_cast<si::operand_stack*>(stack)->pop_cat1<float>();    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if(out_value != nullptr)
    {
        *out_value = value;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_pop_i64(
  slang_context* context,
  slang_operand_stack stack,
  int64_t* out_value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    auto value = reinterpret_cast<si::operand_stack*>(stack)->pop_cat2<std::int64_t>();    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if(out_value != nullptr)
    {
        *out_value = value;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_pop_f64(
  slang_context* context,
  slang_operand_stack stack,
  double* out_value)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    auto value = reinterpret_cast<si::operand_stack*>(stack)->pop_cat2<double>();    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if(out_value != nullptr)
    {
        *out_value = value;
    }

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_stack_pop_string(
  slang_context* context,
  slang_operand_stack stack,
  size_t* out_length,
  const char** out_data)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    auto [gc_container] = rt::get_args<rt::gc_object<std::string>>(
      context->context,
      *reinterpret_cast<si::operand_stack*>(stack));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if(out_length != nullptr)
    {
        *out_length = gc_container.get()->length();
    }

    if(out_data != nullptr)
    {
        *out_data = gc_container.get()->c_str();
    }

    return SLANG_STATUS_OK;
}

extern "C" void slang_string_view(
  slang_string string,
  size_t* out_length,
  const char** out_data)
{
    const auto* s = reinterpret_cast<std::string*>(string);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if(out_length != nullptr)
    {
        *out_length = s->length();
    }

    if(out_data != nullptr)
    {
        *out_data = s->c_str();
    }
}

/*
 * Types.
 */

extern "C" slang_status slang_type_register_layout(
  slang_context* context,
  const char* name,
  const size_t* offsets,
  size_t offset_count)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(name == nullptr
       || (offsets == nullptr && offset_count != 0))
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    try
    {
        if(offsets != nullptr)
        {
            context->context.register_type_layout(
              name,
              {offsets, offsets + offset_count});    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        else
        {
            context->context.register_type_layout(
              name,
              {});
        }

        return SLANG_STATUS_OK;
    }
    catch(const slang::gc::gc_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_GC_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

/*
 * Functions.
 */

extern "C" slang_status slang_function_register_native(
  slang_context* context,
  const char* module_name,
  const char* function_name,
  slang_native_function_t callback,
  void* user_data)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(module_name == nullptr
       || function_name == nullptr
       || callback == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    try
    {
        context->context.register_native_function(
          module_name,
          function_name,
          [context, callback, user_data](si::operand_stack& stack)
          {
              callback(
                context,
                reinterpret_cast<slang_operand_stack>(&stack),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                user_data);
          });

        return SLANG_STATUS_OK;
    }
    catch(const si::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

extern "C" slang_status slang_function_invoke(
  slang_context* context,
  slang_function function,
  const slang_value* args,
  size_t arg_count,
  slang_value* out_result)
{
    if(out_result != nullptr)
    {
        *out_result = {
          .kind = slang_value_kind::SLANG_VALUE_VOID,
        };
    }

    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(function == nullptr
       || (args == nullptr && arg_count != 0))
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    try
    {
        auto* f = reinterpret_cast<si::function*>(function);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        // Convert arguments.
        std::vector<si::value> values;
        values.reserve(arg_count);

        for(size_t i = 0; i < arg_count; ++i)
        {
            switch(args[i].kind)    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            {
            case SLANG_VALUE_I32:
                values.emplace_back(args[i].data.i32);    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-union-access)
                break;

            case SLANG_VALUE_F32:
                values.emplace_back(args[i].data.f32);    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-union-access)
                break;

            case SLANG_VALUE_STR:
                values.emplace_back(std::string{
                  args[i].data.str.data,        // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-union-access)
                  args[i].data.str.length});    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-union-access)
                break;

            case SLANG_VALUE_TYPE:
                values.emplace_back(
                  args[i].data.type.layout_id,                   // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-union-access)
                  const_cast<void*>(args[i].data.type.data));    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-union-access)
                break;

            default:
                context->last_error = "Unsupported argument type.";
                return SLANG_STATUS_INVALID_ARGUMENT;
            }
        }

        // Invoke the function.
        si::value result = f->invoke(values);

        // Convert the result.
        const auto& type = result.get_type().base_type();

        if(type == "i32")
        {
            if(out_result != nullptr)
            {
                out_result->kind = SLANG_VALUE_I32;
                out_result->data.i32 = *result.get<int>();    // NOLINT(cppcoreguidelines-pro-type-union-access)
            }
        }
        else if(type == "f32")
        {
            if(out_result != nullptr)
            {
                out_result->kind = SLANG_VALUE_F32;
                out_result->data.f32 = *result.get<float>();    // NOLINT(cppcoreguidelines-pro-type-union-access)
            }
        }
        else if(type == "str")
        {
            const auto& str = *result.get<std::string>();

            if(out_result != nullptr)
            {
                auto* string_copy = static_cast<char*>(    // NOLINT(cppcoreguidelines-owning-memory)
                  std::malloc(str.length() + 1));          // NOLINT(cppcoreguidelines-no-malloc)

                if(string_copy == nullptr)
                {
                    context->last_error = "Failed to allocate string result.";
                    return SLANG_STATUS_ERROR;
                }

                std::memcpy(
                  string_copy,
                  str.data(),
                  str.length());
                string_copy[str.length()] = '\0';    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

                out_result->kind = SLANG_VALUE_STR;
                out_result->data.str =    // NOLINT(cppcoreguidelines-pro-type-union-access)
                  {.length = str.length(),
                   .data = string_copy};
            }
        }
        else if(type == "void")
        {
            if(out_result != nullptr)
            {
                out_result->kind = SLANG_VALUE_VOID;
            }
        }
        else
        {
            // Custom object. Stays GC managed.
            const auto& layout_id = result.get_type().get_layout_id();
            if(!layout_id.has_value())
            {
                context->last_error = "Result type has no layout id.";
                return SLANG_STATUS_ERROR;
            }

            void* const* object = result.get<void*>();
            if(object == nullptr)
            {
                context->last_error = "Invalid custom type result.";
                return SLANG_STATUS_ERROR;
            }

            if(out_result != nullptr)
            {
                out_result->kind = SLANG_VALUE_TYPE;
                out_result->data.type = {
                  .layout_id = layout_id.value(),
                  .data = *object,    // TODO Test this.
                };
            }
        }

        return SLANG_STATUS_OK;
    }
    catch(const si::interpreter_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

/*
 * Garbage collector.
 */

extern "C" slang_status slang_context_gc_remove_temporary(
  slang_context* context,
  void* object)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        context->context.get_gc().remove_temporary(object);
        return SLANG_STATUS_OK;
    }
    catch(const slang::gc::gc_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_GC_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

extern "C" slang_status slang_context_gc_run(
  slang_context* context)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    try
    {
        context->context.get_gc().run();
        return SLANG_STATUS_OK;
    }
    catch(const slang::gc::gc_error& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_GC_ERROR;
    }
    catch(const std::exception& e)
    {
        context->last_error = e.what();
        return SLANG_STATUS_ERROR;
    }
    catch(...)
    {
        context->last_error = "Unknown error.";
        return SLANG_STATUS_ERROR;
    }
}

extern "C" slang_status slang_context_gc_object_count(
  slang_context* context,
  size_t* out_count)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(out_count == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    *out_count = context->context.get_gc().object_count();

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_context_gc_root_set_size(
  slang_context* context,
  size_t* out_count)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(out_count == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    *out_count = context->context.get_gc().root_set_size();

    return SLANG_STATUS_OK;
}

extern "C" slang_status slang_context_gc_byte_size(
  slang_context* context,
  size_t* out_size)
{
    if(context == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    context->last_error.clear();

    if(out_size == nullptr)
    {
        return SLANG_STATUS_INVALID_ARGUMENT;
    }

    *out_size = context->context.get_gc().byte_size();

    return SLANG_STATUS_OK;
}