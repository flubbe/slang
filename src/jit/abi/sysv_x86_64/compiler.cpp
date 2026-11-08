/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for x86_64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <bit>
#include <cassert>
#include <cstring>
#include <limits>
#include <ranges>
#include <type_traits>
#include <unordered_map>

#include "archives/memory.h"
#include "jit/cfg.h"
#include "jit/abi/sysv_x86_64/sysv_x86_64.h"
#include "shared/opcodes.h"
#include "shared/type_class.h"

namespace slang::jit::abi::sysv_x86_64
{

using namespace x86_64;

namespace
{

using slang::interpreter::stack_frame;

void load_string_constant(
  stack_frame* frame,
  std::size_t constant_index,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr
           || constant_index >= frame->constants.size())
        {
            throw jit_error{
              "Invalid JIT string constant."};
        }

        const auto& constant = frame->constants.at(constant_index);
        if(constant.type != module_::constant_type::str)
        {
            throw jit_error{
              "JIT string constant table entry is not a string."};
        }

        auto* string = frame->gc->gc_new<std::string>(gc::gc_object::of_temporary);
        *string = std::get<std::string>(constant.data);
        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &string,
          sizeof(string));
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void execute_reference_local(
  stack_frame* frame,
  opcode operation,
  std::size_t local_offset,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Reference local operation requires a runtime context."};
        }

        void* reference{nullptr};
        if(operation == opcode::aload)
        {
            std::memcpy(
              &reference,
              frame->locals.data() + local_offset,
              sizeof(reference));

            frame->gc->add_temporary(reference);
            std::memcpy(
              frame->stack.span().data() + stack_offset,
              &reference,
              sizeof(reference));

            return;
        }

        std::memcpy(
          &reference,
          frame->stack.span().data() + stack_offset,
          sizeof(reference));

        void* previous{nullptr};
        std::memcpy(
          &previous,
          frame->locals.data() + local_offset,
          sizeof(previous));

        frame->gc->remove_temporary(reference);
        if(reference != previous)
        {
            frame->update_gc_local_root(local_offset, reference);
        }

        std::memcpy(
          frame->locals.data() + local_offset,
          &reference,
          sizeof(reference));
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void execute_reference_stack(
  stack_frame* frame,
  opcode operation,
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Reference stack operation requires a runtime context."};
        }

        void* reference{nullptr};
        std::memcpy(
          &reference,
          frame->stack.span().data() + stack_offset,
          sizeof(reference));

        if(operation == opcode::adup)
        {
            frame->gc->add_temporary(reference);
            std::memcpy(
              frame->stack.span().data() + stack_offset + stack_slot_size,
              &reference,
              sizeof(reference));
        }
        else
        {
            frame->gc->remove_temporary(reference);
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void duplicate_stack_block(
  stack_frame* frame,
  opcode operation,
  std::uint32_t reference_flags,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t old_size) noexcept
{
    try
    {
        if(frame == nullptr
           || old_size > frame->stack.span().size())
        {
            throw jit_error{
              "Invalid current JIT operand stack size."};
        }

        if(reference_flags != 0
           && frame->gc == nullptr)
        {
            throw jit_error{
              "Reference stack duplication requires a runtime context."};
        }

        const auto copy_reference = [&](std::size_t offset)
        {
            void* reference{nullptr};
            std::memcpy(
              &reference,
              frame->stack.span().data() + offset,
              sizeof(reference));

            frame->gc->add_temporary(reference);
        };

        auto stack = frame->stack.span();
        if(operation == opcode::dup2_x0)
        {
            constexpr auto copied_size = 2 * stack_slot_size;
            if(old_size < copied_size
               || copied_size > stack.size() - old_size)
            {
                throw jit_error{
                  "Invalid dup2_x0 operand stack."};
            }

            const auto source_offset = old_size - copied_size;
            const std::vector<std::byte> copy{
              stack.begin() + static_cast<std::ptrdiff_t>(source_offset),
              stack.begin() + static_cast<std::ptrdiff_t>(old_size),
            };
            std::memcpy(
              stack.data() + old_size,
              copy.data(),
              copied_size);

            if((reference_flags & 0x1u) != 0)
            {
                copy_reference(source_offset);
            }

            if((reference_flags & 0x2u) != 0)
            {
                copy_reference(source_offset + stack_slot_size);
            }

            return;
        }

        const auto below_size =
          operation == opcode::dup_x1
            ? stack_slot_size
            : 2 * stack_slot_size;

        constexpr auto slot_size = static_cast<std::size_t>(stack_slot_size);
        if(old_size < slot_size + below_size
           || slot_size > stack.size() - old_size)
        {
            throw jit_error{
              "Invalid dup_x operand stack."};
        }

        const auto source_offset = old_size - stack_slot_size - below_size;
        const std::vector<std::byte> copy{
          stack.begin() + static_cast<std::ptrdiff_t>(source_offset + below_size),
          stack.begin() + static_cast<std::ptrdiff_t>(old_size),
        };
        std::memmove(
          stack.data() + source_offset + stack_slot_size,
          stack.data() + source_offset,
          below_size);

        std::memcpy(
          stack.data() + source_offset,
          copy.data(),
          stack_slot_size);
        std::memcpy(
          stack.data() + old_size,
          copy.data(),
          stack_slot_size);

        if((reference_flags & 0x1u) != 0)
        {
            copy_reference(source_offset);
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void create_array(
  stack_frame* frame,
  module_::array_type type,
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Array allocation requires a runtime context."};
        }

        std::int32_t length{0};
        std::memcpy(
          &length,
          frame->stack.span().data() + stack_offset,
          sizeof(length));

        if(length < 0)
        {
            throw jit_error{
              std::format(
                "Invalid array size '{}'.",
                length)};
        }

        void* array{nullptr};
        auto& collector = *frame->gc;

        switch(type)
        {
        case module_::array_type::i8:
            array = collector.gc_new_array<std::int8_t>(
              length,
              gc::gc_object::of_temporary);
            break;
        case module_::array_type::i16:
            array = collector.gc_new_array<std::int16_t>(
              length,
              gc::gc_object::of_temporary);
            break;
        case module_::array_type::i32:
            array = collector.gc_new_array<std::int32_t>(
              length,
              gc::gc_object::of_temporary);
            break;
        case module_::array_type::i64:
            array = collector.gc_new_array<std::int64_t>(
              length,
              gc::gc_object::of_temporary);
            break;
        case module_::array_type::f32:
            array = collector.gc_new_array<float>(
              length,
              gc::gc_object::of_temporary);
            break;
        case module_::array_type::f64:
            array = collector.gc_new_array<double>(
              length,
              gc::gc_object::of_temporary);
            break;
        case module_::array_type::str:
        {
            auto* strings = collector.gc_new_array<std::string*>(
              length,
              gc::gc_object::of_temporary);

            for(auto& string: *strings)
            {
                string = collector.gc_new<std::string>(
                  gc::gc_object::of_none,
                  false);
            }

            array = strings;
            break;
        }
        case module_::array_type::ref:
            array = collector.gc_new_array<void*>(
              length,
              gc::gc_object::of_temporary);
            break;
        }

        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &array,
          sizeof(array));
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

template<typename Element>
void load_array_element(
  stack_frame* frame,
  void* array,
  std::int32_t index,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset,
  bool reference_element)
{
    auto* values = static_cast<slang::interpreter::fixed_vector<Element>*>(array);

    if(index < 0
       || static_cast<std::size_t>(index) >= values->size())
    {
        throw jit_error{
          "Out of bounds array access."};
    }

    const auto value = (*values)[index];
    if constexpr(std::is_pointer_v<Element>)
    {
        if(reference_element)
        {
            frame->gc->add_temporary(value);
        }
    }

    if constexpr(
      std::is_same_v<Element, std::int8_t>
      || std::is_same_v<Element, std::int16_t>)
    {
        const auto widened = static_cast<std::int32_t>(value);
        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &widened,
          sizeof(widened));
    }
    else
    {
        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &value,
          sizeof(value));
    }
}

void execute_array_load(
  stack_frame* frame,
  opcode operation,
  std::size_t stack_offset) noexcept
{
    try
    {
        void* array{nullptr};
        std::int32_t index{0};
        auto stack = frame->stack.span();

        std::memcpy(
          &array,
          stack.data() + stack_offset,
          sizeof(array));
        std::memcpy(
          &index,
          stack.data() + stack_offset + stack_slot_size,
          sizeof(index));

        if(array == nullptr)
        {
            throw jit_error{
              std::format(
                "Null pointer access during {}.",
                to_string(operation))};
        }

        if(frame->gc == nullptr)
        {
            throw jit_error{
              "Array access requires a runtime context."};
        }

        const auto array_type = frame->gc->get_object_type(array);
        frame->gc->remove_temporary(array);

        const auto expected_type = [&]
        {
            switch(operation)
            {
            case opcode::caload:
                return gc::gc_object_type::array_i8;
            case opcode::saload:
                return gc::gc_object_type::array_i16;
            case opcode::iaload:
                return gc::gc_object_type::array_i32;
            case opcode::laload:
                return gc::gc_object_type::array_i64;
            case opcode::faload:
                return gc::gc_object_type::array_f32;
            case opcode::daload:
                return gc::gc_object_type::array_f64;
            case opcode::aaload:
                return array_type;
            default:
                throw jit_error{
                  "Invalid array load opcode."};
            }
        }();

        if(array_type != expected_type
           || (operation == opcode::aaload
               && array_type != gc::gc_object_type::array_str
               && array_type != gc::gc_object_type::array_aref))
        {
            throw jit_error{
              "Array load opcode does not match the array element type."};
        }

        switch(operation)
        {
        case opcode::caload:
            load_array_element<std::int8_t>(
              frame,
              array,
              index,
              stack_offset,
              false);
            break;
        case opcode::saload:
            load_array_element<std::int16_t>(
              frame,
              array,
              index,
              stack_offset,
              false);
            break;
        case opcode::iaload:
            load_array_element<std::int32_t>(
              frame,
              array,
              index,
              stack_offset,
              false);
            break;
        case opcode::laload:
            load_array_element<std::int64_t>(
              frame,
              array,
              index,
              stack_offset,
              false);
            break;
        case opcode::faload:
            load_array_element<float>(
              frame,
              array,
              index,
              stack_offset,
              false);
            break;
        case opcode::daload:
            load_array_element<double>(
              frame,
              array,
              index,
              stack_offset,
              false);
            break;
        case opcode::aaload:
            if(frame->gc->get_object_type(array) == gc::gc_object_type::array_str)
            {
                load_array_element<std::string*>(
                  frame,
                  array,
                  index,
                  stack_offset,
                  true);
            }
            else
            {
                load_array_element<void*>(
                  frame,
                  array,
                  index,
                  stack_offset,
                  true);
            }

            break;
        default:
            throw jit_error{
              "Invalid array load opcode."};
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

template<typename Element>
void store_array_element(
  stack_frame* frame,
  void* array,
  std::int32_t index,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t value_offset,
  bool reference_element)
{
    auto* values = static_cast<slang::interpreter::fixed_vector<Element>*>(array);

    Element value{};
    std::memcpy(
      &value,
      frame->stack.span().data() + value_offset,
      sizeof(value));

    if constexpr(std::is_pointer_v<Element>)
    {
        if(reference_element)
        {
            frame->gc->remove_temporary(value);
        }
    }

    if(index < 0
       || static_cast<std::size_t>(index) >= values->size())
    {
        throw jit_error{
          "Out of bounds array access."};
    }

    (*values)[index] = value;
}

void execute_array_store(
  stack_frame* frame,
  opcode operation,
  std::size_t stack_offset) noexcept
{
    try
    {
        void* array{nullptr};
        auto stack = frame->stack.span();
        std::memcpy(
          &array,
          stack.data() + stack_offset,
          sizeof(array));

        std::int32_t index{0};
        std::memcpy(
          &index,
          stack.data() + stack_offset + stack_slot_size,
          sizeof(index));

        if(array == nullptr)
        {
            throw jit_error{
              std::format(
                "Null pointer access during {}.",
                to_string(operation))};
        }

        if(frame->gc == nullptr)
        {
            throw jit_error{
              "Array access requires a runtime context."};
        }

        const auto array_type = frame->gc->get_object_type(array);
        frame->gc->remove_temporary(array);

        const auto expected_type = [&]
        {
            switch(operation)
            {
            case opcode::castore:
                return gc::gc_object_type::array_i8;
            case opcode::sastore:
                return gc::gc_object_type::array_i16;
            case opcode::iastore:
                return gc::gc_object_type::array_i32;
            case opcode::lastore:
                return gc::gc_object_type::array_i64;
            case opcode::fastore:
                return gc::gc_object_type::array_f32;
            case opcode::dastore:
                return gc::gc_object_type::array_f64;
            case opcode::aastore:
                return array_type;
            default:
                throw jit_error{
                  "Invalid array store opcode."};
            }
        }();

        if(array_type != expected_type
           || (operation == opcode::aastore
               && array_type != gc::gc_object_type::array_str
               && array_type != gc::gc_object_type::array_aref))
        {
            throw jit_error{
              "Array store opcode does not match the array element type."};
        }

        const auto value_offset = stack_offset + (2uz * stack_slot_size);
        switch(operation)
        {
        case opcode::castore:
            store_array_element<std::int8_t>(
              frame,
              array,
              index,
              value_offset,
              false);
            break;
        case opcode::sastore:
            store_array_element<std::int16_t>(
              frame,
              array,
              index,
              value_offset,
              false);
            break;
        case opcode::iastore:
            store_array_element<std::int32_t>(
              frame,
              array,
              index,
              value_offset,
              false);
            break;
        case opcode::lastore:
            store_array_element<std::int64_t>(
              frame,
              array,
              index,
              value_offset,
              false);
            break;
        case opcode::fastore:
            store_array_element<float>(
              frame,
              array,
              index,
              value_offset,
              false);
            break;
        case opcode::dastore:
            store_array_element<double>(
              frame,
              array,
              index,
              value_offset,
              false);
            break;
        case opcode::aastore:
            if(frame->gc->get_object_type(array) == gc::gc_object_type::array_str)
            {
                store_array_element<std::string*>(
                  frame,
                  array,
                  index,
                  value_offset,
                  true);
            }
            else
            {
                store_array_element<void*>(
                  frame,
                  array,
                  index,
                  value_offset,
                  true);
            }
            break;
        default:
            throw jit_error{
              "Invalid array store opcode."};
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void array_length(
  stack_frame* frame,
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Array length requires a runtime context."};
        }

        void* array{nullptr};
        std::memcpy(
          &array,
          frame->stack.span().data() + stack_offset,
          sizeof(array));
        if(array == nullptr)
        {
            throw jit_error{
              "Null pointer access during arraylength."};
        }

        const auto type = frame->gc->get_object_type(array);
        frame->gc->remove_temporary(array);

        std::size_t length{0};
        switch(type)
        {
        case gc::gc_object_type::array_i8:
            length = static_cast<slang::interpreter::fixed_vector<std::int8_t>*>(array)->size();
            break;
        case gc::gc_object_type::array_i16:
            length = static_cast<slang::interpreter::fixed_vector<std::int16_t>*>(array)->size();
            break;
        case gc::gc_object_type::array_i32:
            length = static_cast<slang::interpreter::fixed_vector<std::int32_t>*>(array)->size();
            break;
        case gc::gc_object_type::array_i64:
            length = static_cast<slang::interpreter::fixed_vector<std::int64_t>*>(array)->size();
            break;
        case gc::gc_object_type::array_f32:
            length = static_cast<slang::interpreter::fixed_vector<float>*>(array)->size();
            break;
        case gc::gc_object_type::array_f64:
            length = static_cast<slang::interpreter::fixed_vector<double>*>(array)->size();
            break;
        case gc::gc_object_type::array_str:
            length = static_cast<slang::interpreter::fixed_vector<std::string*>*>(array)->size();
            break;
        case gc::gc_object_type::array_aref:
            length = static_cast<slang::interpreter::fixed_vector<void*>*>(array)->size();
            break;
        default:
            throw jit_error{
              "arraylength: argument is not an array."};
        }

        const auto value = static_cast<std::int32_t>(length);
        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &value,
          sizeof(value));
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void create_struct(
  stack_frame* frame,
  std::size_t size,
  std::size_t alignment,
  std::size_t layout_id,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Struct allocation requires a runtime context."};
        }

        auto* object = frame->gc->gc_new(
          layout_id,
          size,
          alignment,
          gc::gc_object::of_temporary);
        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &object,
          sizeof(object));
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void create_struct_array(
  stack_frame* frame,
  std::size_t layout_id,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Struct array allocation requires a runtime context."};
        }

        std::int32_t length{0};
        std::memcpy(
          &length,
          frame->stack.span().data() + stack_offset,
          sizeof(length));

        if(length < 0)
        {
            throw jit_error{
              std::format(
                "Invalid array length '{}'.",
                length)};
        }

        auto* array = frame->gc->gc_new_array(
          layout_id,
          static_cast<std::size_t>(length),
          gc::gc_object::of_temporary);
        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &array,
          sizeof(array));    // We really want to store the pointer here. // NOLINT(bugprone-sizeof-expression)
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void access_field(
  stack_frame* frame,
  bool store,
  std::size_t field_size,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t field_offset,
  bool field_needs_gc,
  std::size_t stack_offset) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Struct field access requires a runtime context."};
        }

        void* object{nullptr};
        std::memcpy(
          &object,
          frame->stack.span().data() + stack_offset,
          sizeof(object));

        if(object == nullptr)
        {
            throw jit_error{
              std::format(
                "Null pointer access during {}.",
                store
                  ? "setfield"
                  : "getfield")};
        }

        if(field_size != 1
           && field_size != 2
           && field_size != 4
           && field_size != 8)    // NOLINT(readability-magic-numbers)
        {
            throw jit_error{
              std::format(
                "Invalid field size {}.",
                field_size)};
        }

        frame->gc->remove_temporary(object);
        auto* location = static_cast<std::byte*>(object) + field_offset;    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        if(store)
        {
            std::int64_t value{0};
            std::memcpy(
              &value,
              frame->stack.span().data() + stack_offset + stack_slot_size,
              sizeof(value));

            if(field_needs_gc
               && field_size == sizeof(void*))
            {
                void* reference{nullptr};
                std::memcpy(
                  static_cast<void*>(&reference),
                  &value,
                  sizeof(reference));
                frame->gc->remove_temporary(reference);
            }

            std::memcpy(
              location,
              &value,
              field_size);
            return;
        }

        std::int64_t value{0};
        switch(field_size)
        {
        case 1:
        {
            std::int8_t field{0};
            std::memcpy(
              &field,
              location,
              sizeof(field));
            value = field;    // NOLINT(bugprone-signed-char-misuse)
            break;
        }
        case 2:
        {
            std::int16_t field{0};
            std::memcpy(
              &field,
              location,
              sizeof(field));
            value = field;
            break;
        }
        case 4:
        {
            std::int32_t field{0};
            std::memcpy(
              &field,
              location,
              sizeof(field));
            value = field;
            break;
        }
        case 8:    // NOLINT(readability-magic-numbers)
            std::memcpy(
              &value,
              location,
              sizeof(value));
            break;
        default:
            std::unreachable();
        }

        std::memcpy(
          frame->stack.span().data() + stack_offset,
          &value,
          sizeof(value));

        if(field_needs_gc
           && field_size == sizeof(void*))
        {
            void* reference{nullptr};
            std::memcpy(
              static_cast<void*>(&reference),
              &value,
              sizeof(reference));
            frame->gc->add_temporary(reference);
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

void check_cast(
  stack_frame* frame,
  std::size_t stack_offset,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t target_layout_id) noexcept
{
    try
    {
        if(frame == nullptr
           || frame->gc == nullptr)
        {
            throw jit_error{
              "Type cast requires a runtime context."};
        }

        void* object{nullptr};
        std::memcpy(
          &object,
          frame->stack.span().data() + stack_offset,
          sizeof(object));
        if(object == nullptr)
        {
            throw jit_error{
              "Null pointer access during checkcast."};
        }

        const auto source_layout_id = frame->gc->get_type_layout_id(object);
        if(source_layout_id != target_layout_id)
        {
            throw jit_error{std::format(
              "Type cast from '{}' to '{}' failed.",
              frame->gc->layout_to_string(source_layout_id),
              frame->gc->layout_to_string(target_layout_id))};
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

template<typename T>
    requires std::is_integral_v<T> && std::is_signed_v<T>
void store_and_extend(
  std::span<std::byte> destination,    // NOLINT(bugprone-easily-swappable-parameters)
  std::span<const std::byte> source)
{
    T value{};
    std::memcpy(
      &value,
      source.data(),
      sizeof(value));
    const auto extended = static_cast<std::int64_t>(value);
    std::memcpy(
      destination.data(),
      &extended,
      sizeof(extended));
}

void invoke_call_target(
  const jit_call_target* target,
  stack_frame* caller,
  std::size_t argument_offset) noexcept
{
    try
    {
        if(target == nullptr
           || caller == nullptr)
        {
            throw jit_error{
              "Invalid JIT call target or caller frame."};
        }

        if(target->safepoint)
        {
            target->safepoint();
        }

        std::span<std::byte> caller_stack;
        if(!caller->stack.empty())
        {
            caller_stack = caller->stack.span();
        }

        if(argument_offset > caller_stack.size()
           || target->argument_size > caller_stack.size() - argument_offset
           || target->return_size > caller_stack.size() - argument_offset)
        {
            throw jit_error{
              "JIT call arguments or return value exceed caller stack storage."};
        }

        const auto arguments = std::span<std::byte>{caller_stack}
                                 .subspan(
                                   argument_offset,
                                   target->argument_size);
        const auto return_slot = caller_stack.subspan(
          argument_offset,
          target->return_size);

        std::visit(
          [target, caller, arguments, return_slot](const auto& function)
          {
              using T = std::decay_t<decltype(function)>;
              if constexpr(std::is_same_v<T, std::monostate>)
              {
                  throw jit_error{
                    "Attempted to invoke an empty function target."};
              }
              else if constexpr(std::is_same_v<T, jit_function_pointer>)
              {
                  if(function == nullptr
                     || target->constants == nullptr)
                  {
                      throw jit_error{
                        "Attempted to invoke an incomplete JIT function target."};
                  }

                  if(target->argument_size > target->locals_size)
                  {
                      throw jit_error{
                        "JIT function arguments exceed its local storage."};
                  }

                  auto callee = slang::interpreter::stack_frame::with_size(
                    *target->constants,
                    target->locals_size,
                    target->stack_size,
                    caller->gc);
                  std::ranges::copy(arguments, callee.locals.begin());

                  if(caller->gc != nullptr)
                  {
                      for(const auto local_offset: target->gc_local_offsets)
                      {
                          if(local_offset + sizeof(void*) > callee.locals.size())
                          {
                              continue;
                          }

                          void* reference{nullptr};
                          std::memcpy(
                            &reference,
                            callee.locals.data() + local_offset,
                            sizeof(reference));

                          if(reference != nullptr)
                          {
                              if(caller->gc->is_temporary(reference))
                              {
                                  caller->gc->remove_temporary(reference);
                              }
                              callee.update_gc_local_root(local_offset, reference);
                          }
                      }
                  }

                  function(&callee);

                  if(callee.pending_exception != nullptr)
                  {
                      caller->pending_exception = callee.pending_exception;
                      callee.clear_gc_local_roots();
                      return;
                  }

                  if(target->return_size > callee.stack.size())
                  {
                      throw jit_error{
                        "JIT function return value exceeds its operand stack."};
                  }

                  if(target->return_size != 0)
                  {
                      std::ranges::copy(
                        callee.stack.span().first(target->return_size),
                        return_slot.begin());
                  }

                  callee.clear_gc_local_roots();
              }
              else if constexpr(std::is_same_v<T, native_function_type>)
              {
                  if(function == nullptr)
                  {
                      throw jit_error{
                        "Attempted to invoke an incomplete native function target."};
                  }

                  auto native_stack = slang::interpreter::operand_stack::with_capacity(
                    target->argument_size + target->return_size);
                  std::size_t jit_offset{0};

                  for(const auto parameter_size: target->param_sizes)
                  {
                      native_stack.push_bytes(
                        arguments.subspan(jit_offset, parameter_size));
                      jit_offset += static_cast<std::size_t>(stack_slot_size);
                  }

                  function(native_stack);

                  if(target->return_size != 0)
                  {
                      if(return_slot.size() != target->return_size)
                      {
                          throw jit_error{
                            std::format(
                              "Native function returned {} bytes, expected {}.",
                              native_stack.size(),
                              return_slot.size())};
                      }

                      std::size_t native_return_size{0};
                      switch(target->return_type_kind)
                      {
                      case slang::codegen::type_kind::i8: [[fallthrough]];
                      case slang::codegen::type_kind::i16: [[fallthrough]];
                      case slang::codegen::type_kind::i32: [[fallthrough]];
                      case slang::codegen::type_kind::f32:
                          native_return_size = sizeof(std::int32_t);
                          break;
                      case slang::codegen::type_kind::i64: [[fallthrough]];
                      case slang::codegen::type_kind::f64: [[fallthrough]];
                      case slang::codegen::type_kind::ref:
                          native_return_size = sizeof(std::int64_t);
                          break;
                      default:
                          throw jit_error{
                            "Native function has unexpected return type kind."};
                      }

                      if(native_stack.size() != native_return_size)
                      {
                          throw jit_error{
                            std::format(
                              "Native function returned {} bytes, expected {}.",
                              native_stack.size(),
                              native_return_size)};
                      }

                      std::ranges::fill(return_slot, std::byte{0});
                      const auto native_return = native_stack.span();
                      switch(target->return_type_kind)
                      {
                      case slang::codegen::type_kind::i8:
                          store_and_extend<std::int8_t>(return_slot, native_return);
                          break;
                      case slang::codegen::type_kind::i16:
                          store_and_extend<std::int16_t>(return_slot, native_return);
                          break;
                      case slang::codegen::type_kind::i32:
                          store_and_extend<std::int32_t>(return_slot, native_return);
                          break;
                      case slang::codegen::type_kind::f32:
                          std::ranges::copy(native_return, return_slot.begin());
                          break;
                      case slang::codegen::type_kind::i64: [[fallthrough]];
                      case slang::codegen::type_kind::f64: [[fallthrough]];
                      case slang::codegen::type_kind::ref:
                          std::ranges::copy(native_return, return_slot.begin());
                          break;
                      default:
                          throw jit_error{
                            "Native function has unexpected return type kind."};
                      }
                  }
              }
              else
              {
                  static_assert(utils::false_type<T>::value, "Unsupported JIT callable function type.");
              }
          },
          target->function);
    }
    catch(...)
    {
        if(caller != nullptr
           && caller->pending_exception == nullptr)
        {
            caller->pending_exception = std::current_exception();
        }
    }
}

/** Register use for generated functions. */
constexpr auto frame_register = cpu_registers::R12;
constexpr auto stack_register = cpu_registers::R13;
constexpr auto locals_register = cpu_registers::R14;
constexpr auto lhs_register = cpu_registers::RAX;
constexpr auto rhs_register = cpu_registers::RCX;

struct stack_frame_offsets
{
    std::size_t locals;
    std::size_t stack;
};

struct branch_fixup
{
    std::size_t instruction_index;
    std::int64_t label_id;
    bool conditional;
};

void record_negative_shift(
  stack_frame* frame) noexcept
{
    if(frame == nullptr
       || frame->pending_exception != nullptr)
    {
        return;
    }

    try
    {
        throw slang::interpreter::interpreter_error{
          "Negative shift."};
    }
    catch(...)
    {
        frame->pending_exception = std::current_exception();
    }
}

template<typename T>
bool compare_values(
  T lhs,
  T rhs,
  opcode operation)
{
    switch(operation)
    {
    case opcode::icmpl: [[fallthrough]];
    case opcode::lcmpl: [[fallthrough]];
    case opcode::fcmpl: [[fallthrough]];
    case opcode::dcmpl:
        return lhs < rhs;
    case opcode::icmple: [[fallthrough]];
    case opcode::lcmple: [[fallthrough]];
    case opcode::fcmple: [[fallthrough]];
    case opcode::dcmple:
        return lhs <= rhs;
    case opcode::icmpg: [[fallthrough]];
    case opcode::lcmpg: [[fallthrough]];
    case opcode::fcmpg: [[fallthrough]];
    case opcode::dcmpg:
        return lhs > rhs;
    case opcode::icmpge: [[fallthrough]];
    case opcode::lcmpge: [[fallthrough]];
    case opcode::fcmpge: [[fallthrough]];
    case opcode::dcmpge:
        return lhs >= rhs;
    case opcode::icmpeq: [[fallthrough]];
    case opcode::lcmpeq: [[fallthrough]];
    case opcode::fcmpeq: [[fallthrough]];
    case opcode::dcmpeq: [[fallthrough]];
    case opcode::acmpeq:
        return lhs == rhs;
    case opcode::icmpne: [[fallthrough]];
    case opcode::lcmpne: [[fallthrough]];
    case opcode::fcmpne: [[fallthrough]];
    case opcode::dcmpne: [[fallthrough]];
    case opcode::acmpne:
        return lhs != rhs;
    default:
        throw jit_error{
          "Invalid x86_64 comparison opcode."};
    }
}

void execute_comparison(
  stack_frame* frame,
  opcode operation,
  std::size_t offset) noexcept
{
    try
    {
        if(frame == nullptr)
        {
            throw jit_error{
              "Invalid JIT stack frame."};
        }

        const auto stack = frame->stack.span();
        if(offset > stack.size()
           || 2 * stack_slot_size > stack.size() - offset)
        {
            throw jit_error{
              "Invalid JIT comparison stack offset."};
        }

        bool result{false};
        switch(operation)
        {
        case opcode::icmpl: [[fallthrough]];
        case opcode::icmple: [[fallthrough]];
        case opcode::icmpg: [[fallthrough]];
        case opcode::icmpge: [[fallthrough]];
        case opcode::icmpeq: [[fallthrough]];
        case opcode::icmpne:
        {
            std::int32_t lhs{0};
            std::int32_t rhs{0};
            std::memcpy(
              &lhs,
              stack.data() + offset,
              sizeof(lhs));
            std::memcpy(
              &rhs,
              stack.data() + offset + stack_slot_size,
              sizeof(rhs));
            result = compare_values(
              lhs,
              rhs,
              operation);
            break;
        }
        case opcode::lcmpl: [[fallthrough]];
        case opcode::lcmple: [[fallthrough]];
        case opcode::lcmpg: [[fallthrough]];
        case opcode::lcmpge: [[fallthrough]];
        case opcode::lcmpeq: [[fallthrough]];
        case opcode::lcmpne:
        {
            std::int64_t lhs{0};
            std::int64_t rhs{0};
            std::memcpy(
              &lhs,
              stack.data() + offset,
              sizeof(lhs));
            std::memcpy(
              &rhs,
              stack.data() + offset + stack_slot_size,
              sizeof(rhs));
            result = compare_values(
              lhs,
              rhs,
              operation);
            break;
        }
        case opcode::fcmpl: [[fallthrough]];
        case opcode::fcmple: [[fallthrough]];
        case opcode::fcmpg: [[fallthrough]];
        case opcode::fcmpge: [[fallthrough]];
        case opcode::fcmpeq: [[fallthrough]];
        case opcode::fcmpne:
        {
            float lhs{0};
            float rhs{0};
            std::memcpy(
              &lhs,
              stack.data() + offset,
              sizeof(lhs));
            std::memcpy(
              &rhs,
              stack.data() + offset + stack_slot_size,
              sizeof(rhs));
            result = compare_values(
              lhs,
              rhs,
              operation);
            break;
        }
        case opcode::dcmpl: [[fallthrough]];
        case opcode::dcmple: [[fallthrough]];
        case opcode::dcmpg: [[fallthrough]];
        case opcode::dcmpge: [[fallthrough]];
        case opcode::dcmpeq: [[fallthrough]];
        case opcode::dcmpne:
        {
            double lhs{0};
            double rhs{0};
            std::memcpy(
              &lhs,
              stack.data() + offset,
              sizeof(lhs));
            std::memcpy(
              &rhs,
              stack.data() + offset + stack_slot_size,
              sizeof(rhs));
            result = compare_values(
              lhs,
              rhs,
              operation);
            break;
        }
        case opcode::acmpeq: [[fallthrough]];
        case opcode::acmpne:
        {
            void* lhs{nullptr};
            void* rhs{nullptr};
            std::memcpy(
              &lhs,
              stack.data() + offset,
              sizeof(lhs));
            std::memcpy(
              &rhs,
              stack.data() + offset + stack_slot_size,
              sizeof(rhs));
            result = compare_values(
              lhs,
              rhs,
              operation);

            if(frame->gc != nullptr)
            {
                frame->gc->remove_temporary(lhs);
                frame->gc->remove_temporary(rhs);
            }

            break;
        }
        default:
            throw jit_error{
              "Invalid x86_64 comparison opcode."};
        }

        const auto value = static_cast<std::int32_t>(result);
        std::memcpy(
          stack.data() + offset,
          &value,
          sizeof(value));
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

template<
  typename Input,
  typename Output>
void convert_value(
  stack_frame* frame,
  std::size_t offset)
{
    Input input{};
    std::memcpy(
      &input,
      frame->stack.span().data() + offset,
      sizeof(input));
    const auto output = static_cast<Output>(input);
    std::memcpy(
      frame->stack.span().data() + offset,
      &output,
      sizeof(output));
}

template<typename Narrow>
void narrow_i32(
  stack_frame* frame,
  std::size_t offset)
{
    std::int32_t input{0};
    std::memcpy(
      &input,
      frame->stack.span().data() + offset,
      sizeof(input));
    const auto output = static_cast<std::int32_t>(static_cast<Narrow>(input));
    std::memcpy(
      frame->stack.span().data() + offset,
      &output,
      sizeof(output));
}

void execute_numeric_conversion(
  stack_frame* frame,
  opcode operation,
  std::size_t offset) noexcept
{
    try
    {
        if(frame == nullptr
           || offset > frame->stack.span().size()
           || stack_slot_size > frame->stack.span().size() - offset)
        {
            throw jit_error{
              "Invalid JIT numeric conversion stack offset."};
        }

        switch(operation)
        {
        case opcode::i2c:
            narrow_i32<std::int8_t>(
              frame,
              offset);
            return;
        case opcode::i2s:
            narrow_i32<std::int16_t>(
              frame,
              offset);
            return;
        case opcode::i2l:
            convert_value<std::int32_t, std::int64_t>(
              frame,
              offset);
            return;
        case opcode::i2f:
            convert_value<std::int32_t, float>(
              frame,
              offset);
            return;
        case opcode::i2d:
            convert_value<std::int32_t, double>(
              frame,
              offset);
            return;
        case opcode::l2i:
            convert_value<std::int64_t, std::int32_t>(
              frame,
              offset);
            return;
        case opcode::l2f:
            convert_value<std::int64_t, float>(
              frame,
              offset);
            return;
        case opcode::l2d:
            convert_value<std::int64_t, double>(
              frame,
              offset);
            return;
        case opcode::f2i:
            convert_value<float, std::int32_t>(
              frame,
              offset);
            return;
        case opcode::f2l:
            convert_value<float, std::int64_t>(
              frame,
              offset);
            return;
        case opcode::f2d:
            convert_value<float, double>(
              frame,
              offset);
            return;
        case opcode::d2i:
            convert_value<double, std::int32_t>(
              frame,
              offset);
            return;
        case opcode::d2l:
            convert_value<double, std::int64_t>(
              frame,
              offset);
            return;
        case opcode::d2f:
            convert_value<double, float>(
              frame,
              offset);
            return;
        default:
            throw jit_error{
              "Invalid x86_64 numeric conversion opcode."};
        }
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }
    }
}

bool has_pending_exception(
  stack_frame* frame) noexcept
{
    return frame != nullptr
           && frame->pending_exception != nullptr;
}

template<typename T>
T divide_or_remainder(
  T dividend,
  T divisor,
  bool remainder)
{
    if(divisor == 0)
    {
        throw slang::interpreter::interpreter_error{
          "Division by zero."};
    }

    if(dividend == std::numeric_limits<T>::min()
       && divisor == -1)
    {
        return remainder ? 0 : dividend;
    }

    return remainder
             ? dividend % divisor
             : dividend / divisor;
}

std::int32_t execute_integer_division(
  stack_frame* frame,
  std::size_t offset,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t flags) noexcept
{
    try
    {
        if(frame == nullptr)
        {
            throw slang::interpreter::interpreter_error{
              "Invalid JIT stack frame."};
        }

        const auto stack = frame->stack.span();
        if(offset > stack.size()
           || 2 * stack_slot_size > stack.size() - offset)
        {
            throw slang::interpreter::interpreter_error{
              "Invalid JIT division stack offset."};
        }

        const bool is_64_bit = (flags & 0x1u) != 0;
        const bool remainder = (flags & 0x2u) != 0;

        if(is_64_bit)
        {
            std::int64_t dividend{0};
            std::int64_t divisor{0};
            std::memcpy(
              &dividend,
              stack.data() + offset,
              sizeof(dividend));
            std::memcpy(
              &divisor,
              stack.data() + offset + stack_slot_size,
              sizeof(divisor));
            const auto result = divide_or_remainder(
              dividend,
              divisor,
              remainder);
            std::memcpy(
              stack.data() + offset,
              &result,
              sizeof(result));
        }
        else
        {
            std::int32_t dividend{0};
            std::int32_t divisor{0};
            std::memcpy(
              &dividend,
              stack.data() + offset,
              sizeof(dividend));
            std::memcpy(
              &divisor,
              stack.data() + offset + stack_slot_size,
              sizeof(divisor));
            const auto result = divide_or_remainder(
              dividend,
              divisor,
              remainder);
            std::memcpy(
              stack.data() + offset,
              &result,
              sizeof(result));
        }

        return 0;
    }
    catch(...)
    {
        if(frame != nullptr
           && frame->pending_exception == nullptr)
        {
            frame->pending_exception = std::current_exception();
        }

        return 1;
    }
}

stack_frame_offsets calculate_stack_frame_offsets()
{
    const std::vector<module_::constant_table_entry> constants;
    const auto frame = stack_frame::with_capacity(constants, 0, 0, nullptr);
    const auto base = reinterpret_cast<std::uintptr_t>(&frame);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto locals_offset =
      reinterpret_cast<std::uintptr_t>(&frame.locals) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto stack_offset =
      reinterpret_cast<std::uintptr_t>(&frame.stack) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    return {.locals = locals_offset, .stack = stack_offset};
}

std::int32_t checked_displacement(std::size_t offset)
{
    if(offset > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw jit_error{
          "x86_64 frame offset is out of range."};
    }
    return static_cast<std::int32_t>(offset);
}

void emit_helper_call(
  instruction_emitter& emitter,
  std::uintptr_t function,
  opcode operation,
  std::size_t stack_offset)
{
    emitter.mov_r64(
      cpu_registers::RDI,
      frame_register);
    emitter.mov_imm32(
      cpu_registers::RSI,
      std::to_underlying(operation));
    emitter.mov_imm64(
      cpu_registers::RDX,
      stack_offset);
    emitter.mov_imm64(
      lhs_register,
      function);
    emitter.call_reg(lhs_register);
}

std::size_t get_local_offset(
  const vle_int& index,
  const std::vector<std::size_t>& local_offsets,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t locals_size,
  std::size_t local_size)
{
    if(index.i < 0
       || static_cast<std::uint64_t>(index.i) >= local_offsets.size())
    {
        throw jit_error{
          std::format(
            "Local variable index {} is out of range.",
            index.i)};
    }

    const auto offset = local_offsets.at(static_cast<std::size_t>(index.i));
    if(offset > locals_size
       || local_size > locals_size - offset)
    {
        throw jit_error{
          std::format(
            "Local variable at index {} exceeds local storage.",
            index.i)};
    }

    return offset;
}

void update_stack_size(
  opcode operation,
  std::uint32_t& current_size,
  std::uint32_t& max_size,
  std::int32_t delta)
{
    const auto next_size = static_cast<std::int64_t>(current_size) + delta;
    if(next_size < 0
       || next_size > std::numeric_limits<std::int32_t>::max())
    {
        throw jit_error{
          std::format(
            "Invalid operand stack size after '{}'.",
            to_string(operation))};
    }

    current_size = static_cast<std::uint32_t>(next_size);
    max_size = std::max(max_size, current_size);
}

void emit_binary(
  instruction_emitter& emitter,
  opcode operation)
{
    const bool is_32_bit =
      operation == opcode::iadd
      || operation == opcode::isub
      || operation == opcode::imul
      || operation == opcode::iand
      || operation == opcode::ior
      || operation == opcode::ixor;

    switch(operation)
    {
    case opcode::iadd: [[fallthrough]];
    case opcode::ladd:
        if(is_32_bit)
        {
            emitter.add_r32(lhs_register, rhs_register);
            emitter.movsxd_r64_r32(lhs_register, lhs_register);
        }
        else
        {
            emitter.add_r64(lhs_register, rhs_register);
        }
        break;
    case opcode::isub: [[fallthrough]];
    case opcode::lsub:
        if(is_32_bit)
        {
            emitter.sub_r32(lhs_register, rhs_register);
            emitter.movsxd_r64_r32(lhs_register, lhs_register);
        }
        else
        {
            emitter.sub_r64(lhs_register, rhs_register);
        }
        break;
    case opcode::imul: [[fallthrough]];
    case opcode::lmul:
        if(is_32_bit)
        {
            emitter.imul_r32(lhs_register, rhs_register);
            emitter.movsxd_r64_r32(lhs_register, lhs_register);
        }
        else
        {
            emitter.imul_r64(lhs_register, rhs_register);
        }
        break;
    case opcode::iand: [[fallthrough]];
    case opcode::land:
        if(is_32_bit)
        {
            emitter.and_r32(lhs_register, rhs_register);
            emitter.movsxd_r64_r32(lhs_register, lhs_register);
        }
        else
        {
            emitter.and_r64(lhs_register, rhs_register);
        }
        break;
    case opcode::ior: [[fallthrough]];
    case opcode::lor:
        if(is_32_bit)
        {
            emitter.or_r32(lhs_register, rhs_register);
            emitter.movsxd_r64_r32(lhs_register, lhs_register);
        }
        else
        {
            emitter.or_r64(lhs_register, rhs_register);
        }
        break;
    case opcode::ixor: [[fallthrough]];
    case opcode::lxor:
        if(is_32_bit)
        {
            emitter.xor_r32(lhs_register, rhs_register);
            emitter.movsxd_r64_r32(lhs_register, lhs_register);
        }
        else
        {
            emitter.xor_r64(lhs_register, rhs_register);
        }
        break;
    default:
        throw jit_error{"Unsupported x86_64 binary operation."};
    }
}

}    // namespace

jit_function jit_compiler::create_jit_function(
  module_::function_signature signature,
  const std::vector<std::uint8_t>& machine_code,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t locals_size,
  std::size_t stack_size)
{
    executable_memory memory{std::span{machine_code}};

    auto function =
      reinterpret_cast<jit_function_pointer>(memory.data());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return jit_function{
      std::move(signature),
      std::move(memory),
      function,
      locals_size,
      stack_size,
    };
}

jit_function jit_compiler::compile(
  module_::function_signature signature,
  std::span<const std::byte> bytecode,    // NOLINT(bugprone-easily-swappable-parameters)
  const std::vector<std::size_t>& local_offsets,
  std::size_t locals_size,
  const std::vector<std::unique_ptr<jit_call_target>>& call_targets,
  const std::vector<jit_call_target*>& import_call_targets,
  const std::function<module_::struct_descriptor(std::int64_t)>& resolve_type)
{
    if(locals_size > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw jit_error{
          "x86_64 locals storage is too large."};
    }

    instruction_emitter emitter;
    std::vector<std::size_t> return_fixups;
    std::vector<branch_fixup> branch_fixups;
    std::vector<std::size_t> negative_shift_fixups;
    std::vector<std::size_t> exception_fixups;
    std::unordered_map<std::int64_t, std::size_t> labels;

    const auto label_entry_depths = analyze_bytecode_stack_depths(
      bytecode,
      call_targets,
      import_call_targets);

    /*
     * Prologue.
     */

    emitter.push(frame_register);
    emitter.push(stack_register);
    emitter.push(locals_register);
    emitter.mov_r64(frame_register, cpu_registers::RDI);

    const auto [locals_member_offset, stack_member_offset] = calculate_stack_frame_offsets();
    emitter.load_r64(
      locals_register,
      frame_register,
      checked_displacement(locals_member_offset));
    emitter.load_r64(
      stack_register,
      frame_register,
      checked_displacement(stack_member_offset));

    const auto emit_exception_guard = [&]
    {
        emitter.mov_r64(cpu_registers::RDI, frame_register);
        emitter.mov_imm64(
          lhs_register,
          reinterpret_cast<std::uintptr_t>(&has_pending_exception));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        emitter.call_reg(lhs_register);
        emitter.test_r32(lhs_register, lhs_register);
        exception_fixups.push_back(emitter.code.size());
        emitter.jcc_rel32(condition_code::not_equal);
    };

    /*
     * Compile bytecode.
     */

    std::uint32_t current_stack_size{0};
    std::uint32_t max_stack_size{0};
    memory_read_archive input{bytecode, true, std::endian::little};

    while(input.tell() < input.size())
    {
        const auto start_pc = input.tell();

        std::byte instr{0};
        input & instr;

        // Validate instruction range.
        if(std::to_integer<unsigned int>(instr) >= std::to_underlying(opcode::opcode_count))
        {
            throw jit_error{
              std::format(
                "'{:02x}' at PC {} is not an opcode.",
                std::to_integer<unsigned int>(instr),
                start_pc)};
        }

        const auto op = static_cast<opcode>(instr);
        switch(op)
        {
        case opcode::label:
        {
            vle_int label_id;
            input & label_id;

            if(!labels.emplace(label_id.i, emitter.code.size()).second)
            {
                throw jit_error{
                  std::format(
                    "Duplicate x86_64 label {}.",
                    label_id.i)};
            }

            if(const auto it = label_entry_depths.find(label_id.i);
               it != label_entry_depths.end())
            {
                current_stack_size = static_cast<std::uint32_t>(it->second);
                max_stack_size = std::max(max_stack_size, current_stack_size);
            }

            break;
        }
        case opcode::jmp:
        {
            vle_int label_id;
            input & label_id;
            const auto fixup = emitter.code.size();
            emitter.jmp_rel32();
            branch_fixups.push_back(
              {
                .instruction_index = fixup,
                .label_id = label_id.i,
                .conditional = false,
              });
            break;
        }
        case opcode::jnz:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Not enough stack values for 'jnz'."};
            }

            vle_int then_label;
            vle_int else_label;
            input & then_label & else_label;

            const auto condition_offset =
              checked_displacement(
                current_stack_size - stack_slot_size);

            emitter.load_r32(
              lhs_register,
              stack_register,
              condition_offset);
            emitter.test_r32(
              lhs_register,
              lhs_register);

            const auto conditional_fixup = emitter.code.size();
            emitter.jcc_rel32(condition_code::not_equal);

            branch_fixups.push_back(
              {
                .instruction_index = conditional_fixup,
                .label_id = then_label.i,
                .conditional = true,
              });

            const auto else_fixup = emitter.code.size();
            emitter.jmp_rel32();

            branch_fixups.push_back(
              {
                .instruction_index = else_fixup,
                .label_id = else_label.i,
                .conditional = false,
              });

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);

            break;
        }
        case opcode::iconst:
        {
            std::int32_t value{0};
            input & value;

            emitter.mov_imm64(
              lhs_register,
              static_cast<std::uint64_t>(
                static_cast<std::int64_t>(value)));
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::lconst:
        {
            std::int64_t value{0};
            input & value;
            emitter.mov_imm64(
              lhs_register,
              std::bit_cast<std::uint64_t>(value));
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::fconst:
        {
            float value{0.0F};
            input & value;
            emitter.mov_imm32(
              lhs_register,
              std::bit_cast<std::uint32_t>(value));
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::dconst:
        {
            double value{0.0};
            input & value;
            emitter.mov_imm64(
              lhs_register,
              std::bit_cast<std::uint64_t>(value));
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::sconst:
        {
            vle_int constant_index;
            input & constant_index;
            if(constant_index.i < 0)
            {
                throw jit_error{
                  "Invalid JIT string constant index."};
            }
            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm64(
              cpu_registers::RSI,
              static_cast<std::uint64_t>(
                constant_index.i));
            emitter.mov_imm64(
              cpu_registers::RDX,
              current_stack_size);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                &load_string_constant));
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::aconst_null:
        {
            emitter.mov_imm64(lhs_register, 0);
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::iload: [[fallthrough]];
        case opcode::fload: [[fallthrough]];
        case opcode::lload:
        case opcode::dload:
        {
            vle_int index;
            input & index;

            const bool is_32_bit =
              op == opcode::iload
              || op == opcode::fload;
            const auto local_size =
              is_32_bit
                ? sizeof(std::int32_t)
                : sizeof(std::int64_t);
            const auto offset = get_local_offset(
              index,
              local_offsets,
              locals_size,
              local_size);

            if(is_32_bit)
            {
                emitter.load_r32(
                  lhs_register,
                  locals_register,
                  checked_displacement(offset));
                if(op == opcode::iload)
                {
                    emitter.movsxd_r64_r32(
                      lhs_register,
                      lhs_register);
                }
            }
            else
            {
                emitter.load_r64(
                  lhs_register,
                  locals_register,
                  checked_displacement(offset));
            }

            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::aload: [[fallthrough]];
        case opcode::astore:
        {
            vle_int index;
            input & index;

            const auto offset = get_local_offset(
              index,
              local_offsets,
              locals_size,
              sizeof(void*));
            const auto is_load = op == opcode::aload;

            if(!is_load
               && current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Not enough stack values for 'astore'."};
            }

            const auto stack_offset =
              is_load
                ? current_stack_size
                : current_stack_size - stack_slot_size;

            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm32(
              cpu_registers::RSI,
              std::to_underlying(op));
            emitter.mov_imm64(
              cpu_registers::RDX,
              offset);
            emitter.mov_imm64(
              cpu_registers::RCX,
              stack_offset);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&execute_reference_local));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              is_load ? stack_slot_size : -stack_slot_size);
            break;
        }
        case opcode::dup: [[fallthrough]];
        case opcode::dup2:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }
            emitter.load_r64(
              lhs_register,
              stack_register,
              checked_displacement(current_stack_size - stack_slot_size));
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::adup: [[fallthrough]];
        case opcode::apop:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto stack_offset = current_stack_size - stack_slot_size;
            emit_helper_call(
              emitter,
              reinterpret_cast<std::uintptr_t>(&execute_reference_stack),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              op,
              stack_offset);

            if(op == opcode::adup)
            {
                emit_exception_guard();
                update_stack_size(
                  op,
                  current_stack_size,
                  max_stack_size,
                  stack_slot_size);
            }
            else
            {
                emit_exception_guard();
                update_stack_size(
                  op,
                  current_stack_size,
                  max_stack_size,
                  -stack_slot_size);
            }

            break;
        }
        case opcode::dup_x1: [[fallthrough]];
        case opcode::dup_x2: [[fallthrough]];
        case opcode::dup2_x0:
        {
            type_class type1;    // NOLINT(cppcoreguidelines-init-variables)
            type_class type2;    // NOLINT(cppcoreguidelines-init-variables)
            type_class type3{type_class::cat1};
            input & type1 & type2;

            if(op == opcode::dup_x2)
            {
                input & type3;
            }

            const auto required_size =
              op == opcode::dup_x2
                ? 3uz * stack_slot_size
                : 2uz * stack_slot_size;

            if(current_stack_size < required_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto reference_flags =
              (type1 == type_class::ref ? 0x1u : 0u)
              | (op == opcode::dup2_x0 && type2 == type_class::ref
                   ? 0x2u
                   : 0u);

            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm32(
              cpu_registers::RSI,
              std::to_underlying(op));
            emitter.mov_imm32(
              cpu_registers::RDX,
              reference_flags);
            emitter.mov_imm64(
              cpu_registers::RCX,
              current_stack_size);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&duplicate_stack_block));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              op == opcode::dup2_x0
                ? 2 * stack_slot_size
                : stack_slot_size);
            break;
        }
        case opcode::pop: [[fallthrough]];
        case opcode::pop2:
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size, -stack_slot_size);
            break;
        case opcode::istore: [[fallthrough]];
        case opcode::fstore: [[fallthrough]];
        case opcode::lstore:
        case opcode::dstore:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Not enough stack values for local store."};
            }
            vle_int index;
            input & index;

            const bool is_32_bit =
              op == opcode::istore || op == opcode::fstore;
            const auto local_size =
              is_32_bit
                ? sizeof(std::int32_t)
                : sizeof(std::int64_t);
            const auto offset = get_local_offset(
              index,
              local_offsets,
              locals_size,
              local_size);

            emitter.load_r64(
              lhs_register,
              stack_register,
              checked_displacement(current_stack_size - stack_slot_size));

            if(is_32_bit)
            {
                emitter.store_r32(
                  locals_register,
                  checked_displacement(offset),
                  lhs_register);
            }
            else
            {
                emitter.store_r64(
                  locals_register,
                  checked_displacement(offset),
                  lhs_register);
            }

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);
            break;
        }
        case opcode::iadd: [[fallthrough]];
        case opcode::ladd: [[fallthrough]];
        case opcode::isub: [[fallthrough]];
        case opcode::lsub: [[fallthrough]];
        case opcode::imul: [[fallthrough]];
        case opcode::lmul: [[fallthrough]];
        case opcode::iand: [[fallthrough]];
        case opcode::land: [[fallthrough]];
        case opcode::ior: [[fallthrough]];
        case opcode::lor: [[fallthrough]];
        case opcode::ixor: [[fallthrough]];
        case opcode::lxor:
        {
            if(current_stack_size < 2 * stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            emitter.load_r64(
              lhs_register,
              stack_register,
              checked_displacement(current_stack_size - (2 * stack_slot_size)));
            emitter.load_r64(
              rhs_register,
              stack_register,
              checked_displacement(current_stack_size - stack_slot_size));
            emit_binary(emitter, op);
            emitter.store_r64(
              stack_register,
              checked_displacement(current_stack_size - (2 * stack_slot_size)),
              lhs_register);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);
            break;
        }
        case opcode::idiv: [[fallthrough]];
        case opcode::imod: [[fallthrough]];
        case opcode::ldiv:
        case opcode::lmod:
        {
            if(current_stack_size < 2 * stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const bool is_64_bit =
              op == opcode::ldiv || op == opcode::lmod;
            const bool remainder =
              op == opcode::imod || op == opcode::lmod;

            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm64(
              cpu_registers::RSI,
              current_stack_size - (2 * stack_slot_size));
            emitter.mov_imm32(
              cpu_registers::RDX,
              (is_64_bit ? 0x1u : 0u)
                | (remainder ? 0x2u : 0u));
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                &execute_integer_division));
            emitter.call_reg(lhs_register);
            emitter.test_r32(
              lhs_register,
              lhs_register);

            exception_fixups.push_back(emitter.code.size());
            emitter.jcc_rel32(condition_code::not_equal);

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);
            break;
        }
        case opcode::fadd: [[fallthrough]];
        case opcode::fsub: [[fallthrough]];
        case opcode::fmul: [[fallthrough]];
        case opcode::fdiv: [[fallthrough]];
        case opcode::dadd: [[fallthrough]];
        case opcode::dsub: [[fallthrough]];
        case opcode::dmul: [[fallthrough]];
        case opcode::ddiv:
        {
            if(current_stack_size < 2 * stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto left_offset = checked_displacement(current_stack_size - (2 * stack_slot_size));
            const auto right_offset = checked_displacement(current_stack_size - stack_slot_size);
            const bool is_double =
              op == opcode::dadd
              || op == opcode::dsub
              || op == opcode::dmul
              || op == opcode::ddiv;

            if(is_double)
            {
                emitter.load_r64(
                  lhs_register,
                  stack_register,
                  left_offset);
                emitter.movq_xmm_r64(
                  simd_registers::XMM0,
                  lhs_register);
                emitter.load_r64(
                  rhs_register,
                  stack_register,
                  right_offset);
                emitter.movq_xmm_r64(
                  simd_registers::XMM1,
                  rhs_register);
            }
            else
            {
                emitter.load_r32(
                  lhs_register,
                  stack_register,
                  left_offset);
                emitter.movd_xmm_r32(
                  simd_registers::XMM0,
                  lhs_register);
                emitter.load_r32(
                  rhs_register,
                  stack_register,
                  right_offset);
                emitter.movd_xmm_r32(
                  simd_registers::XMM1,
                  rhs_register);
            }

            switch(op)
            {
            case opcode::fadd:
                emitter.addss(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::fsub:
                emitter.subss(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::fmul:
                emitter.mulss(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::fdiv:
                emitter.divss(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::dadd:
                emitter.addsd(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::dsub:
                emitter.subsd(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::dmul:
                emitter.mulsd(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            case opcode::ddiv:
                emitter.divsd(
                  simd_registers::XMM0,
                  simd_registers::XMM1);
                break;
            default:
                throw jit_error{
                  "Invalid floating-point operation."};
            }

            if(is_double)
            {
                emitter.movq_r64_xmm(
                  lhs_register,
                  simd_registers::XMM0);
                emitter.store_r64(
                  stack_register,
                  left_offset,
                  lhs_register);
            }
            else
            {
                emitter.movd_r32_xmm(
                  lhs_register,
                  simd_registers::XMM0);
                emitter.store_r64(
                  stack_register,
                  left_offset,
                  lhs_register);
            }

            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);
            break;
        }
        case opcode::icmpl: [[fallthrough]];
        case opcode::icmple: [[fallthrough]];
        case opcode::icmpg: [[fallthrough]];
        case opcode::icmpge: [[fallthrough]];
        case opcode::icmpeq: [[fallthrough]];
        case opcode::icmpne: [[fallthrough]];
        case opcode::lcmpl: [[fallthrough]];
        case opcode::lcmple: [[fallthrough]];
        case opcode::lcmpg: [[fallthrough]];
        case opcode::lcmpge: [[fallthrough]];
        case opcode::lcmpeq: [[fallthrough]];
        case opcode::lcmpne: [[fallthrough]];
        case opcode::fcmpl: [[fallthrough]];
        case opcode::fcmple: [[fallthrough]];
        case opcode::fcmpg: [[fallthrough]];
        case opcode::fcmpge: [[fallthrough]];
        case opcode::fcmpeq: [[fallthrough]];
        case opcode::fcmpne: [[fallthrough]];
        case opcode::dcmpl: [[fallthrough]];
        case opcode::dcmple: [[fallthrough]];
        case opcode::dcmpg: [[fallthrough]];
        case opcode::dcmpge: [[fallthrough]];
        case opcode::dcmpeq: [[fallthrough]];
        case opcode::dcmpne: [[fallthrough]];
        case opcode::acmpeq: [[fallthrough]];
        case opcode::acmpne:
        {
            if(current_stack_size < 2 * stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            emit_helper_call(
              emitter,
              reinterpret_cast<std::uintptr_t>(&execute_comparison),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              op,
              current_stack_size - (2 * stack_slot_size));

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);
            break;
        }
        case opcode::i2c: [[fallthrough]];
        case opcode::i2s: [[fallthrough]];
        case opcode::i2l: [[fallthrough]];
        case opcode::i2f: [[fallthrough]];
        case opcode::i2d: [[fallthrough]];
        case opcode::l2i: [[fallthrough]];
        case opcode::l2f: [[fallthrough]];
        case opcode::l2d: [[fallthrough]];
        case opcode::f2i: [[fallthrough]];
        case opcode::f2l: [[fallthrough]];
        case opcode::f2d: [[fallthrough]];
        case opcode::d2i: [[fallthrough]];
        case opcode::d2l:
        case opcode::d2f:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }
            emit_helper_call(
              emitter,
              reinterpret_cast<std::uintptr_t>(&execute_numeric_conversion),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              op,
              current_stack_size - stack_slot_size);

            emit_exception_guard();
            break;
        }
        case opcode::newarray:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Not enough stack values for 'newarray'."};
            }

            module_::array_type type;    // NOLINT(cppcoreguidelines-init-variables)
            input & type;

            const auto stack_offset = current_stack_size - stack_slot_size;
            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm32(
              cpu_registers::RSI,
              std::to_underlying(type));
            emitter.mov_imm64(
              cpu_registers::RDX,
              stack_offset);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&create_array));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            break;
        }
        case opcode::new_:
        {
            if(!resolve_type)
            {
                throw jit_error{
                  "Struct allocation requires a JIT type resolver."};
            }
            vle_int type_index;
            input & type_index;

            const auto type = resolve_type(type_index.i);
            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm64(
              cpu_registers::RSI,
              type.size);
            emitter.mov_imm64(
              cpu_registers::RDX,
              type.alignment);
            emitter.mov_imm64(
              cpu_registers::RCX,
              type.layout_id);
            emitter.mov_imm64(
              cpu_registers::R8,
              current_stack_size);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&create_struct));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              stack_slot_size);
            break;
        }
        case opcode::anewarray:
        {
            if(!resolve_type
               || current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Struct array allocation requires a type resolver and a length."};
            }
            vle_int type_index;
            input & type_index;

            const auto type = resolve_type(type_index.i);
            const auto stack_offset = current_stack_size - stack_slot_size;

            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm64(
              cpu_registers::RSI,
              type.layout_id);
            emitter.mov_imm64(
              cpu_registers::RDX,
              stack_offset);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&create_struct_array));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            break;
        }
        case opcode::checkcast:
        {
            if(!resolve_type
               || current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Type cast requires a type resolver and a reference."};
            }

            vle_int type_index;
            input & type_index;

            const auto type = resolve_type(type_index.i);
            if((type.flags & std::to_underlying(module_::struct_flags::allow_cast)) == 0)
            {
                emitter.mov_r64(
                  cpu_registers::RDI,
                  frame_register);
                emitter.mov_imm64(
                  cpu_registers::RSI,
                  current_stack_size - stack_slot_size);
                emitter.mov_imm64(
                  cpu_registers::RDX,
                  type.layout_id);
                emitter.mov_imm64(
                  lhs_register,
                  reinterpret_cast<std::uintptr_t>(&check_cast));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                emitter.call_reg(lhs_register);

                emit_exception_guard();
            }
            break;
        }
        case opcode::setfield: [[fallthrough]];
        case opcode::getfield:
        {
            if(!resolve_type)
            {
                throw jit_error{
                  "Struct field access requires a JIT type resolver."};
            }
            vle_int type_index;
            vle_int field_index;
            input & type_index & field_index;

            const auto type = resolve_type(type_index.i);
            if(field_index.i < 0
               || static_cast<std::size_t>(field_index.i) >= type.member_types.size())
            {
                throw jit_error{
                  "Struct field index is out of range."};
            }

            const auto& field = type.member_types.at(static_cast<std::size_t>(field_index.i)).second;
            const auto& base_type = field.base_type.base_type();
            const bool field_needs_gc =
              field.base_type.is_array()
              || base_type == "str"
              || (base_type != "void"
                  && base_type != "i8"
                  && base_type != "i16"
                  && base_type != "i32"
                  && base_type != "i64"
                  && base_type != "f32"
                  && base_type != "f64");

            const bool store = op == opcode::setfield;
            const auto required_size =
              store
                ? 2uz * stack_slot_size
                : 1uz * stack_slot_size;

            if(current_stack_size < required_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto stack_offset = current_stack_size - required_size;
            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm32(
              cpu_registers::RSI,
              store
                ? 1u
                : 0u);
            emitter.mov_imm64(
              cpu_registers::RDX,
              field.size);
            emitter.mov_imm64(
              cpu_registers::RCX,
              field.offset);
            emitter.mov_imm32(
              cpu_registers::R8,
              field_needs_gc
                ? 1u
                : 0u);
            emitter.mov_imm64(
              cpu_registers::R9,
              stack_offset);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&access_field));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            if(store)
            {
                update_stack_size(
                  op,
                  current_stack_size,
                  max_stack_size,
                  -2 * stack_slot_size);
            }

            break;
        }
        case opcode::caload: [[fallthrough]];
        case opcode::saload: [[fallthrough]];
        case opcode::iaload: [[fallthrough]];
        case opcode::laload: [[fallthrough]];
        case opcode::faload: [[fallthrough]];
        case opcode::daload: [[fallthrough]];
        case opcode::aaload:
        {
            constexpr auto input_size = 2 * stack_slot_size;
            if(current_stack_size < input_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            emit_helper_call(
              emitter,
              reinterpret_cast<std::uintptr_t>(&execute_array_load),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              op,
              current_stack_size - input_size);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);
            break;
        }
        case opcode::castore: [[fallthrough]];
        case opcode::sastore: [[fallthrough]];
        case opcode::iastore: [[fallthrough]];
        case opcode::lastore: [[fallthrough]];
        case opcode::fastore: [[fallthrough]];
        case opcode::dastore: [[fallthrough]];
        case opcode::aastore:
        {
            constexpr auto input_size = 3 * stack_slot_size;
            if(current_stack_size < input_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            emit_helper_call(
              emitter,
              reinterpret_cast<std::uintptr_t>(&execute_array_store),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              op,
              current_stack_size - input_size);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size, -input_size);
            break;
        }
        case opcode::arraylength:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Not enough stack values for 'arraylength'."};
            }

            emitter.mov_r64(
              cpu_registers::RDI,
              frame_register);
            emitter.mov_imm64(
              cpu_registers::RSI,
              current_stack_size - stack_slot_size);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&array_length));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            break;
        }
        case opcode::invoke:
        {
            vle_int function_index;
            input & function_index;

            const jit_call_target* target{nullptr};
            if(function_index.i < 0)
            {
                const auto import_index = static_cast<std::uint64_t>(-(function_index.i + 1));
                if(import_index >= import_call_targets.size())
                {
                    throw jit_error{
                      std::format(
                        "Function import index {} is out of range.",
                        import_index)};
                }
                target = import_call_targets.at(
                  static_cast<std::size_t>(import_index));
            }
            else
            {
                if(static_cast<std::size_t>(function_index.i) >= call_targets.size())
                {
                    throw jit_error{
                      std::format(
                        "Function export index {} is out of range.",
                        function_index.i)};
                }
                target = call_targets.at(
                                       static_cast<std::size_t>(function_index.i))
                           .get();
            }

            if(target == nullptr)
            {
                throw jit_error{
                  std::format(
                    "Call index {} does not refer to a function.",
                    function_index.i)};
            }

            if(current_stack_size < target->argument_size)
            {
                throw jit_error{
                  "Not enough stack values for function call arguments."};
            }

            if(target->argument_size > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
               || target->return_size > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                throw jit_error{
                  "Function call signature exceeds JIT stack limits."};
            }
            const auto argument_offset =
              current_stack_size - static_cast<std::uint32_t>(target->argument_size);

            emitter.mov_imm64(
              cpu_registers::RDI,
              reinterpret_cast<std::uintptr_t>(target));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.mov_r64(
              cpu_registers::RSI,
              frame_register);
            emitter.mov_imm64(
              cpu_registers::RDX,
              argument_offset);
            emitter.mov_imm64(
              lhs_register,
              reinterpret_cast<std::uintptr_t>(&invoke_call_target));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.call_reg(lhs_register);

            emit_exception_guard();
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              static_cast<std::int32_t>(target->return_size)
                - static_cast<std::int32_t>(target->argument_size));

            break;
        }
        case opcode::ishl: [[fallthrough]];
        case opcode::ishr: [[fallthrough]];
        case opcode::lshl: [[fallthrough]];
        case opcode::lshr:
        {
            if(current_stack_size < 2 * stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto value_offset =
              checked_displacement(current_stack_size - (2 * stack_slot_size));
            const auto shift_offset =
              checked_displacement(current_stack_size - stack_slot_size);
            const bool is_64_bit =
              op == opcode::lshl
              || op == opcode::lshr;

            if(is_64_bit)
            {
                emitter.load_r64(
                  lhs_register,
                  stack_register,
                  value_offset);
            }
            else
            {
                emitter.load_r32(
                  lhs_register,
                  stack_register,
                  value_offset);
            }

            emitter.load_r32(
              rhs_register,
              stack_register,
              shift_offset);
            emitter.test_r32(
              rhs_register,
              rhs_register);

            const auto negative_shift = emitter.code.size();
            emitter.jcc_rel32(condition_code::signed_less);
            negative_shift_fixups.push_back(negative_shift);

            if(op == opcode::ishl
               || op == opcode::lshl)
            {
                if(is_64_bit)
                {
                    emitter.shl_r64_cl(lhs_register);
                }
                else
                {
                    emitter.shl_r32_cl(lhs_register);
                    emitter.movsxd_r64_r32(
                      lhs_register,
                      lhs_register);
                }
            }
            else if(is_64_bit)
            {
                emitter.sar_r64_cl(lhs_register);
            }
            else
            {
                emitter.sar_r32_cl(lhs_register);
                emitter.movsxd_r64_r32(
                  lhs_register,
                  lhs_register);
            }

            emitter.store_r64(
              stack_register,
              value_offset,
              lhs_register);
            update_stack_size(
              op,
              current_stack_size,
              max_stack_size,
              -stack_slot_size);

            break;
        }
        case opcode::ineg: [[fallthrough]];
        case opcode::lneg: [[fallthrough]];
        case opcode::fneg: [[fallthrough]];
        case opcode::dneg:
        {
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto offset =
              checked_displacement(current_stack_size - stack_slot_size);

            emitter.load_r64(
              lhs_register,
              stack_register,
              offset);

            switch(op)
            {
            case opcode::ineg:
                emitter.neg_r32(lhs_register);
                emitter.movsxd_r64_r32(
                  lhs_register,
                  lhs_register);
                break;
            case opcode::lneg:
                emitter.neg_r64(lhs_register);
                break;
            case opcode::fneg:
                emitter.mov_imm32(
                  rhs_register,
                  0x80000000u);    // NOLINT(readability-magic-numbers)
                emitter.xor_r32(
                  lhs_register,
                  rhs_register);
                break;
            case opcode::dneg:
                emitter.mov_imm64(
                  rhs_register,
                  0x8000000000000000u);    // NOLINT(readability-magic-numbers)
                emitter.xor_r64(
                  lhs_register,
                  rhs_register);
                break;
            default:
                throw jit_error{
                  "Invalid x86_64 negation operation."};
            }
            emitter.store_r64(
              stack_register,
              offset,
              lhs_register);
            break;
        }
        case opcode::iret: [[fallthrough]];
        case opcode::lret: [[fallthrough]];
        case opcode::fret: [[fallthrough]];
        case opcode::aret: [[fallthrough]];
        case opcode::dret:
            if(current_stack_size < stack_slot_size)
            {
                throw jit_error{
                  "Not enough stack values for return."};
            }
            [[fallthrough]];
        case opcode::ret:
            return_fixups.push_back(emitter.code.size());
            emitter.jmp_rel32();
            break;
        default:
            throw jit_error{
              std::format(
                "Unimplemented x86_64 opcode '{}'.",
                to_string(op))};
        }
    }

    const auto epilogue = emitter.code.size();
    for(const auto& fixup: branch_fixups)
    {
        const auto label = labels.find(fixup.label_id);
        if(label == labels.end())
        {
            throw jit_error{
              std::format(
                "Unknown x86_64 label {}.",
                fixup.label_id)};
        }

        if(fixup.conditional)
        {
            emitter.patch_jcc_rel32(
              fixup.instruction_index,
              label->second);
        }
        else
        {
            emitter.patch_jmp_rel32(
              fixup.instruction_index,
              label->second);
        }
    }

    for(const auto fixup: return_fixups)
    {
        emitter.patch_jmp_rel32(
          fixup,
          epilogue);
    }

    for(const auto fixup: exception_fixups)
    {
        emitter.patch_jcc_rel32(
          fixup,
          epilogue);
    }

    for(const auto fixup: exception_fixups)
    {
        emitter.patch_jcc_rel32(
          fixup,
          epilogue);
    }

    emitter.pop(locals_register);
    emitter.pop(stack_register);
    emitter.pop(frame_register);
    emitter.ret();

    if(!negative_shift_fixups.empty())
    {
        const auto error_handler = emitter.code.size();
        for(const auto fixup: negative_shift_fixups)
        {
            emitter.patch_jcc_rel32(
              fixup,
              error_handler);
        }

        emitter.mov_r64(
          cpu_registers::RDI,
          frame_register);
        emitter.mov_imm64(
          lhs_register,
          reinterpret_cast<std::uintptr_t>(&record_negative_shift));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        emitter.call_reg(lhs_register);

        const auto return_from_error = emitter.code.size();
        emitter.jmp_rel32();
        emitter.patch_jmp_rel32(
          return_from_error,
          epilogue);
    }

    return create_jit_function(
      std::move(signature),
      emitter.code,
      locals_size,
      max_stack_size);
}

}    // namespace slang::jit::abi::sysv_x86_64
