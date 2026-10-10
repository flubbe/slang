/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <bit>
#include <span>
#include <type_traits>
#include <unordered_map>

#include <sys/mman.h>

#include "archives/memory.h"
#include "jit/aarch64.h"
#include "shared/type_class.h"

namespace si = slang::interpreter;

namespace
{

using namespace slang;
using namespace slang::jit;

void load_string_constant(
  si::stack_frame* frame,
  std::size_t constant_index,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset)
{
    if(frame == nullptr
       || frame->gc == nullptr)
    {
        throw jit_error{
          "JIT string constant requires a runtime context."};
    }
    if(constant_index >= frame->constants.size())
    {
        throw jit_error{
          "Invalid JIT string constant index."};
    }

    const auto& constant = frame->constants[constant_index];
    if(constant.type != module_::constant_type::str)
    {
        throw jit_error{
          "JIT string constant table entry is not a string."};
    }

    auto* string = frame->gc->gc_new<std::string>(
      gc::gc_object::of_temporary);
    *string = std::get<std::string>(constant.data);
    std::memcpy(
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      static_cast<const void*>(&string),
      sizeof(string));    // We really want to store the pointer here (not the string) // NOLINT(bugprone-sizeof-expression)
}

template<typename T>
bool compare_helper(
  si::stack_frame* frame,
  opcode operation,
  std::size_t stack_offset)
{
    T lhs{};
    T rhs{};

    std::memcpy(
      static_cast<void*>(&lhs),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(lhs));
    std::memcpy(
      static_cast<void*>(&rhs),
      frame->stack.span().data() + stack_offset + sizeof(lhs),    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(rhs));

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
    case opcode::dcmpeq:
        return lhs == rhs;
    case opcode::icmpne: [[fallthrough]];
    case opcode::lcmpne: [[fallthrough]];
    case opcode::fcmpne: [[fallthrough]];
    case opcode::dcmpne:
        return lhs != rhs;
    case opcode::acmpeq:
        return lhs == rhs;
    case opcode::acmpne:
        return lhs != rhs;
    default:
        throw jit_error{"Invalid comparison opcode."};
    }
}

void compare_values(
  si::stack_frame* frame,
  opcode operation,
  std::size_t stack_offset)
{
    bool result{false};
    switch(operation)
    {
    case opcode::icmpl: [[fallthrough]];
    case opcode::icmple: [[fallthrough]];
    case opcode::icmpg: [[fallthrough]];
    case opcode::icmpge: [[fallthrough]];
    case opcode::icmpeq: [[fallthrough]];
    case opcode::icmpne:
        result = compare_helper<std::int32_t>(
          frame,
          operation,
          stack_offset);
        break;
    case opcode::lcmpl: [[fallthrough]];
    case opcode::lcmple: [[fallthrough]];
    case opcode::lcmpg: [[fallthrough]];
    case opcode::lcmpge: [[fallthrough]];
    case opcode::lcmpeq: [[fallthrough]];
    case opcode::lcmpne:
        result = compare_helper<std::int64_t>(
          frame,
          operation,
          stack_offset);
        break;
    case opcode::fcmpl: [[fallthrough]];
    case opcode::fcmple: [[fallthrough]];
    case opcode::fcmpg: [[fallthrough]];
    case opcode::fcmpge: [[fallthrough]];
    case opcode::fcmpeq: [[fallthrough]];
    case opcode::fcmpne:
        result = compare_helper<float>(
          frame,
          operation,
          stack_offset);
        break;
    case opcode::dcmpl: [[fallthrough]];
    case opcode::dcmple: [[fallthrough]];
    case opcode::dcmpg: [[fallthrough]];
    case opcode::dcmpge: [[fallthrough]];
    case opcode::dcmpeq: [[fallthrough]];
    case opcode::dcmpne:
        result = compare_helper<double>(
          frame,
          operation,
          stack_offset);
        break;
    case opcode::acmpeq: [[fallthrough]];
    case opcode::acmpne:
    {
        result = compare_helper<void*>(
          frame,
          operation,
          stack_offset);

        auto* values = reinterpret_cast<void**>(         // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          frame->stack.span().data() + stack_offset);    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        frame->gc->remove_temporary(values[0]);          // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        frame->gc->remove_temporary(values[1]);          // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        break;
    }
    default:
        throw jit_error{"Invalid comparison opcode."};
    }

    const auto result_value = static_cast<std::int32_t>(result);
    std::memcpy(
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      &result_value,
      sizeof(result_value));
}

void load_reference_local(
  si::stack_frame* frame,
  std::size_t local_offset,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset)
{
    void* reference{nullptr};
    std::memcpy(
      static_cast<void*>(&reference),
      frame->locals.data() + local_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(reference));

    frame->gc->add_temporary(reference);
    std::memcpy(
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      static_cast<const void*>(&reference),
      sizeof(reference));
}

void store_reference_local(
  si::stack_frame* frame,
  std::size_t local_offset,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset)
{
    void* reference{nullptr};
    std::memcpy(
      static_cast<void*>(&reference),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(reference));

    void* previous{nullptr};
    std::memcpy(
      static_cast<void*>(&previous),
      frame->locals.data() + local_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(previous));

    frame->gc->remove_temporary(reference);
    if(reference != previous)
    {
        frame->update_gc_local_root(
          local_offset,
          reference);
    }

    std::memcpy(
      frame->locals.data() + local_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      static_cast<const void*>(&reference),
      sizeof(reference));
}

void duplicate_reference(
  si::stack_frame* frame,
  std::size_t stack_offset)
{
    void* reference{nullptr};
    std::memcpy(
      static_cast<void*>(&reference),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(reference));

    frame->gc->add_temporary(reference);
    std::memcpy(
      frame->stack.span().data() + stack_offset + sizeof(reference),    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      static_cast<const void*>(&reference),
      sizeof(reference));
}

void discard_reference(
  si::stack_frame* frame,
  std::size_t stack_offset)
{
    void* reference{nullptr};
    std::memcpy(
      static_cast<void*>(&reference),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(reference));
    frame->gc->remove_temporary(reference);
}

void duplicate_stack_block(
  si::stack_frame* frame,
  opcode operation,
  std::size_t size1,
  std::size_t size2,
  std::size_t size3,
  bool needs_gc1,
  bool needs_gc2,
  std::size_t current_stack_size)
{
    auto stack = frame->stack.span();
    const auto old_size = current_stack_size;
    if(old_size > stack.size())
    {
        throw jit_error{
          "Invalid current JIT operand stack size."};
    }

    const auto below_size =
      operation == opcode::dup_x1
        ? size2
        : size2 + size3;
    const auto copied_size =
      operation == opcode::dup2_x0
        ? size1 + size2
        : size1;
    const auto source_offset =
      operation == opcode::dup2_x0
        ? old_size - copied_size
        : old_size - copied_size - below_size;
    const auto copy_offset =
      operation == opcode::dup2_x0
        ? source_offset
        : source_offset + below_size;

    const std::vector<std::byte> copy{
      stack.begin() + static_cast<std::ptrdiff_t>(copy_offset),
      stack.begin() + static_cast<std::ptrdiff_t>(copy_offset + copied_size)};

    if(operation == opcode::dup2_x0)
    {
        std::memcpy(
          stack.data() + old_size,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          copy.data(),
          copied_size);

        if(needs_gc1)
        {
            void* reference{nullptr};
            std::memcpy(
              static_cast<void*>(&reference),
              copy.data(),
              size1);
            frame->gc->add_temporary(reference);
        }

        if(needs_gc2)
        {
            void* reference{nullptr};
            std::memcpy(
              static_cast<void*>(&reference),
              copy.data() + size1,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
              size2);
            frame->gc->add_temporary(reference);
        }

        return;
    }

    std::memmove(
      stack.data() + source_offset + size1,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      stack.data() + source_offset,            // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      below_size);
    std::memcpy(
      stack.data() + source_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      copy.data(),
      size1);
    std::memcpy(
      stack.data() + source_offset + size1 + below_size,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      copy.data(),
      size1);

    if(needs_gc1)
    {
        void* reference{nullptr};
        std::memcpy(
          static_cast<void*>(&reference),
          copy.data(),
          size1);
        frame->gc->add_temporary(reference);
    }
}

void run_gc_safepoint(
  si::stack_frame* frame)
{
    if(frame != nullptr
       && frame->gc != nullptr
       && frame->gc->is_run_requested())
    {
        frame->gc->run();
    }
}

bool has_pending_exception(
  si::stack_frame* frame) noexcept
{
    return frame != nullptr
           && frame->pending_exception != nullptr;
}

void execute_fp_operation(
  si::stack_frame* frame,
  opcode operation,
  std::size_t stack_offset)
{
    // 32-bit floating point operations.

    if(operation == opcode::fadd
       || operation == opcode::fsub
       || operation == opcode::fmul
       || operation == opcode::fdiv)
    {
        float lhs{0};
        float rhs{0};

        std::memcpy(
          &lhs,
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(lhs));
        std::memcpy(
          &rhs,
          frame->stack.span().data() + stack_offset + sizeof(lhs),    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(rhs));

        switch(operation)
        {
        case opcode::fadd:
            lhs += rhs;
            break;
        case opcode::fsub:
            lhs -= rhs;
            break;
        case opcode::fmul:
            lhs *= rhs;
            break;
        case opcode::fdiv:
            lhs /= rhs;
            break;
        default:
            std::unreachable();
        }

        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &lhs,
          sizeof(lhs));

        return;
    }

    // 64-bit floating point operations.

    double lhs{0};
    double rhs{0};

    std::memcpy(
      &lhs,
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(lhs));
    std::memcpy(
      &rhs,
      frame->stack.span().data() + stack_offset + sizeof(lhs),    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(rhs));

    switch(operation)
    {
    case opcode::dadd:
        lhs += rhs;
        break;
    case opcode::dsub:
        lhs -= rhs;
        break;
    case opcode::dmul:
        lhs *= rhs;
        break;
    case opcode::ddiv:
        lhs /= rhs;
        break;
    default:
        std::unreachable();
    }
    std::memcpy(
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      &lhs,
      sizeof(lhs));
}

void numeric_conversion(
  si::stack_frame* frame,
  opcode operation,
  std::size_t stack_offset)
{
    switch(operation)
    {
    case opcode::i2f:
    {
        std::int32_t value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        const auto result = static_cast<float>(value);
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &result,
          sizeof(result));

        return;
    }
    case opcode::d2f:
    {
        double value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        const auto result = static_cast<float>(value);
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &result,
          sizeof(result));

        return;
    }
    case opcode::f2d:
    {
        float value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        const auto result = static_cast<double>(value);
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &result,
          sizeof(result));

        return;
    }
    case opcode::d2i:
    {
        double value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        const auto result = static_cast<std::int32_t>(value);
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &result,
          sizeof(result));

        return;
    }
    default:
        throw jit_error{
          "Unsupported numeric conversion."};
    }
}

void create_array(
  si::stack_frame* frame,
  module_::array_type type,
  std::size_t stack_offset)
{
    std::int32_t length{0};
    std::memcpy(
      &length,
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(length));

    if(length < 0)
    {
        throw jit_error{
          std::format(
            "Invalid array size '{}'.",
            length)};
    }

    void* array = nullptr;
    auto& gc = *frame->gc;

    switch(type)
    {
    case module_::array_type::i8:
        array = gc.gc_new_array<std::int8_t>(
          length,
          gc::gc_object::of_temporary);
        break;
    case module_::array_type::i16:
        array = gc.gc_new_array<std::int16_t>(
          length,
          gc::gc_object::of_temporary);
        break;
    case module_::array_type::i32:
        array = gc.gc_new_array<std::int32_t>(
          length,
          gc::gc_object::of_temporary);
        break;
    case module_::array_type::i64:
        array = gc.gc_new_array<std::int64_t>(
          length,
          gc::gc_object::of_temporary);
        break;
    case module_::array_type::f32:
        array = gc.gc_new_array<float>(
          length,
          gc::gc_object::of_temporary);
        break;
    case module_::array_type::f64:
        array = gc.gc_new_array<double>(
          length,
          gc::gc_object::of_temporary);
        break;
    case module_::array_type::str:
    {
        auto* strings = gc.gc_new_array<std::string*>(
          length,
          gc::gc_object::of_temporary);

        for(auto& string: *strings)
        {
            string = gc.gc_new<std::string>(
              gc::gc_object::of_none,
              false);
        }
        array = strings;

        break;
    }
    case module_::array_type::ref:
        array = gc.gc_new_array<void*>(
          length,
          gc::gc_object::of_temporary);
        break;
    }

    std::memcpy(
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      static_cast<const void*>(&array),
      sizeof(array));
}

void create_struct_array(
  si::stack_frame* frame,
  std::size_t layout_id,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset)
{
    std::int32_t length{0};
    std::memcpy(
      &length,
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

      static_cast<void*>(&array),
      sizeof(array));    // We really want to store the pointer here // NOLINT(bugprone-sizeof-expression)
}

void create_struct(
  si::stack_frame* frame,
  std::size_t size,
  std::size_t alignment,
  std::size_t layout_id,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_offset)
{
    auto* object = frame->gc->gc_new(
      layout_id,
      size,
      alignment,
      gc::gc_object::of_temporary);
    std::memcpy(
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      static_cast<void*>(&object),
      sizeof(object));
}

void set_field(
  si::stack_frame* frame,
  std::size_t field_size,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t field_offset,
  bool field_needs_gc,
  std::size_t stack_offset)
{
    void* object{nullptr};
    std::memcpy(
      static_cast<void*>(&object),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(object));
    if(object == nullptr)
    {
        throw jit_error{
          "Null pointer access during setfield."};
    }

    const auto value_offset = stack_offset + sizeof(object);
    if(field_size == sizeof(void*))
    {
        void* value{nullptr};
        std::memcpy(
          static_cast<void*>(&value),
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        frame->gc->remove_temporary(object);
        if(field_needs_gc)
        {
            frame->gc->remove_temporary(value);
        }

        std::memcpy(
          static_cast<std::byte*>(object) + field_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          static_cast<const void*>(&value),
          sizeof(value));
    }
    else if(field_size == sizeof(std::int32_t))
    {
        std::int32_t value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        frame->gc->remove_temporary(object);

        std::memcpy(
          static_cast<std::byte*>(object) + field_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &value,
          sizeof(value));
    }
    else
    {
        throw jit_error{
          std::format(
            "Invalid field size {} in setfield.",
            field_size)};
    }
}

void get_field(
  si::stack_frame* frame,
  std::size_t field_size,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t field_offset,
  bool field_needs_gc,
  std::size_t stack_offset)
{
    void* object{nullptr};
    std::memcpy(
      static_cast<void*>(&object),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(object));
    if(object == nullptr)
    {
        throw jit_error{
          "Null pointer access during getfield."};
    }

    frame->gc->remove_temporary(object);

    if(field_size == sizeof(void*))
    {
        void* value{nullptr};
        std::memcpy(
          static_cast<void*>(&value),
          static_cast<std::byte*>(object) + field_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          static_cast<const void*>(&value),
          sizeof(value));

        if(field_needs_gc)
        {
            frame->gc->add_temporary(value);
        }
    }
    else if(field_size == sizeof(std::int32_t))
    {
        std::int32_t value{0};
        std::memcpy(
          &value,
          static_cast<std::byte*>(object) + field_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          &value,
          sizeof(value));
    }
    else
    {
        throw jit_error{
          std::format(
            "Invalid field size {} in getfield.",
            field_size)};
    }
}

void check_cast(
  si::stack_frame* frame,
  std::size_t stack_offset,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t target_layout_id)
{
    void* object{nullptr};
    std::memcpy(
      static_cast<void*>(&object),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(object));
    if(object == nullptr)
    {
        throw jit_error{
          "Null pointer access during checkcast."};
    }
    const auto source_layout_id = frame->gc->get_type_layout_id(object);
    if(source_layout_id != target_layout_id)
    {
        throw jit_error{
          std::format(
            "Type cast from '{}' to '{}' failed.",
            frame->gc->layout_to_string(source_layout_id),
            frame->gc->layout_to_string(target_layout_id))};
    }
}

std::size_t get_array_length(
  si::stack_frame* frame,
  void* array)
{
    if(array == nullptr)
    {
        throw jit_error{
          "Null pointer access during arraylength."};
    }

    auto& gc = *frame->gc;
    const auto type = gc.get_object_type(array);
    gc.remove_temporary(array);

    switch(type)
    {
    case gc::gc_object_type::array_i8:
        return static_cast<si::fixed_vector<std::int8_t>*>(array)->size();
    case gc::gc_object_type::array_i16:
        return static_cast<si::fixed_vector<std::int16_t>*>(array)->size();
    case gc::gc_object_type::array_i32:
        return static_cast<si::fixed_vector<std::int32_t>*>(array)->size();
    case gc::gc_object_type::array_i64:
        return static_cast<si::fixed_vector<std::int64_t>*>(array)->size();
    case gc::gc_object_type::array_f32:
        return static_cast<si::fixed_vector<float>*>(array)->size();
    case gc::gc_object_type::array_f64:
        return static_cast<si::fixed_vector<double>*>(array)->size();
    case gc::gc_object_type::array_str:
    case gc::gc_object_type::array_aref:
        return static_cast<si::fixed_vector<void*>*>(array)->size();
    case gc::gc_object_type::str:
    case gc::gc_object_type::obj:
        break;
    }

    throw jit_error{
      "arraylength: argument is not an array."};
}

void load_array_element(
  si::stack_frame* frame,
  opcode operation,
  std::size_t stack_offset)
{
    void* array{nullptr};
    std::int32_t index{0};
    std::memcpy(
      static_cast<void*>(&array),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(array));
    std::memcpy(
      static_cast<void*>(&index),
      frame->stack.span().data() + stack_offset + sizeof(array),    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(index));

    if(array == nullptr)
    {
        throw jit_error{
          std::format(
            "Null pointer access during {}.",
            to_string(operation))};
    }

    auto& gc = *frame->gc;
    gc.remove_temporary(array);

    auto check_index = [index](std::size_t size)
    {
        if(index < 0 || std::cmp_greater_equal(index, size))
        {
            throw jit_error{
              "Out of bounds array access."};
        }
    };

    auto store = [frame, stack_offset](const auto& value)
    {
        std::memcpy(
          frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          static_cast<const void*>(&value),
          sizeof(value));
    };

    switch(operation)
    {
    case opcode::caload:
    {
        auto* values = static_cast<si::fixed_vector<std::int8_t>*>(array);
        check_index(values->size());
        const std::int32_t value = (*values)[index];    // NOLINT(bugprone-signed-char-misuse)
        store(value);
        return;
    }
    case opcode::saload:
    {
        auto* values = static_cast<si::fixed_vector<std::int16_t>*>(array);
        check_index(values->size());
        const std::int32_t value = (*values)[index];
        store(value);
        return;
    }
    case opcode::iaload:
    {
        auto* values = static_cast<si::fixed_vector<std::int32_t>*>(array);
        check_index(values->size());
        store((*values)[index]);
        return;
    }
    case opcode::laload:
    {
        auto* values = static_cast<si::fixed_vector<std::int64_t>*>(array);
        check_index(values->size());
        store((*values)[index]);
        return;
    }
    case opcode::faload:
    {
        auto* values = static_cast<si::fixed_vector<float>*>(array);
        check_index(values->size());
        store((*values)[index]);
        return;
    }
    case opcode::daload:
    {
        auto* values = static_cast<si::fixed_vector<double>*>(array);
        check_index(values->size());
        store((*values)[index]);
        return;
    }
    case opcode::aaload:
    {
        auto* values = static_cast<si::fixed_vector<void*>*>(array);
        check_index(values->size());
        void* value = (*values)[index];
        gc.add_temporary(value);
        store(value);
        return;
    }
    default:
        throw jit_error{
          "Invalid array load opcode."};
    }
}

void store_array_element(
  si::stack_frame* frame,
  opcode operation,
  std::size_t stack_offset)
{
    void* array{nullptr};
    std::int32_t index{0};
    std::memcpy(
      static_cast<void*>(&array),
      frame->stack.span().data() + stack_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(array));
    std::memcpy(
      static_cast<void*>(&index),
      frame->stack.span().data() + stack_offset + sizeof(array),    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sizeof(index));
    if(array == nullptr)
    {
        throw jit_error{
          std::format(
            "Null pointer access during {}.",
            to_string(operation))};
    }

    auto& gc = *frame->gc;
    auto check_index = [index](std::size_t size)
    {
        if(index < 0 || std::cmp_greater_equal(index, size))
        {
            throw jit_error{
              "Out of bounds array access."};
        }
    };

    const auto value_offset = stack_offset + sizeof(array) + sizeof(index);
    switch(operation)
    {
    case opcode::castore:
    {
        std::int32_t value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<std::int8_t>*>(array);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = static_cast<std::int8_t>(value);
        return;
    }
    case opcode::sastore:
    {
        std::int32_t value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<std::int16_t>*>(array);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = static_cast<std::int16_t>(value);
        return;
    }
    case opcode::iastore:
    {
        std::int32_t value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<std::int32_t>*>(array);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = value;
        return;
    }
    case opcode::lastore:
    {
        std::int64_t value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<std::int64_t>*>(array);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = value;
        return;
    }
    case opcode::fastore:
    {
        float value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<float>*>(array);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = value;
        return;
    }
    case opcode::dastore:
    {
        double value{0};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<double>*>(array);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = value;
        return;
    }
    case opcode::aastore:
    {
        void* value{nullptr};
        std::memcpy(
          &value,
          frame->stack.span().data() + value_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          sizeof(value));

        auto* values = static_cast<si::fixed_vector<void*>*>(array);
        gc.remove_temporary(value);
        gc.remove_temporary(array);

        check_index(values->size());
        (*values)[index] = value;
        return;
    }
    default:
        throw jit_error{
          "Invalid array store opcode."};
    }
}

/**
 * Invoke a generated-code target using the VM's stack-frame calling convention.
 *
 * Arguments are read from the caller's stack at the specified offset, and
 * the return value is written back to the same offset.
 *
 * @param target The target to invoke.
 * @param caller The caller's stack frame.
 * @param argument_offset Byte offset of the arguments and return-value slot
 *    in the caller's stack storage.
 */
void invoke_call_target(
  const jit_call_target* target,
  si::stack_frame* caller,
  std::size_t argument_offset)
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

    auto arguments = std::span<std::byte>{caller_stack}.subspan(
      argument_offset,
      target->argument_size);
    auto return_slot = caller_stack.subspan(
      argument_offset,
      target->return_size);

    std::visit(
      [target, caller, arguments, return_slot](const auto& func) -> void
      {
          using T = std::decay_t<decltype(func)>;

          if constexpr(std::is_same_v<T, std::monostate>)
          {
              throw jit_error{
                "Attempted to invoke an empty function target."};
          }
          else if constexpr(std::is_same_v<T, jit_function_pointer>)
          {
              if(func == nullptr
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

              auto callee = si::stack_frame::with_size(
                *target->constants,
                target->locals_size,
                target->stack_size);
              callee.gc = caller->gc;
              std::ranges::copy(
                arguments,
                callee.locals.begin());

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
                        static_cast<void*>(&reference),
                        callee.locals.data() + local_offset,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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

              func(&callee);
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
              auto native_stack = si::operand_stack::with_capacity(
                target->argument_size + target->return_size);
              native_stack.push_bytes(arguments);

              try
              {
                  func(native_stack);
              }
              catch(...)
              {
                  caller->pending_exception = std::current_exception();
                  return;
              }

              if(native_stack.size() != target->return_size)
              {
                  throw jit_error{
                    std::format(
                      "Native function returned {} bytes, expected {}.",
                      native_stack.size(),
                      target->return_size)};
              }

              if(target->return_size != 0)
              {
                  std::ranges::copy(
                    native_stack.span().first(target->return_size),
                    return_slot.begin());
              }
          }
          else
          {
              static_assert(utils::false_type<T>::value, "Unsupported JIT callable function type.");
          }
      },
      target->function);
}

/** X register size, as `std::int32_t` to avoid some type conversions. */
constexpr auto x_register_size = static_cast<std::int32_t>(sizeof(std::uint64_t));

/** Struct helper holding offsets of the stack frame elements. */
struct stack_frame_offsets
{
    /** Offset of the locals. */
    std::size_t locals;

    /** Offset of the stack. */
    std::size_t stack;
};

/** Calculate the stack frame offsets. */
stack_frame_offsets calculate_stack_frame_offsets()
{
    const std::vector<slang::module_::constant_table_entry> dummy_constants;
    const auto dummy_frame = si::stack_frame::with_capacity(dummy_constants, 0, 0);

    const auto base =
      reinterpret_cast<std::uintptr_t>(&dummy_frame);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::size_t locals_offset =
      reinterpret_cast<std::uintptr_t>(&dummy_frame.locals) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::size_t stack_offset =
      reinterpret_cast<std::uintptr_t>(&dummy_frame.stack) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return {
      .locals = locals_offset,
      .stack = stack_offset,
    };
}

/** Branch fixups to map labels to offsets. */
struct branch_fixup
{
    /** Instruction index in the emitted machine code. */
    std::size_t instruction_index;

    /** Label id. */
    std::int64_t label_id;

    /** Whether this is a conditional jump (JNZ). */
    bool conditional;
};

}    // namespace

namespace slang::jit::aarch64
{

jit_function jit_compiler::create_jit_function(
  const std::vector<std::uint32_t>& machine_code,
  std::size_t locals_size,
  std::size_t stack_size)
{
    executable_memory memory{machine_code};

    auto function =
      reinterpret_cast<jit_function_pointer>(memory.data());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return jit_function{
      std::move(memory),
      function,
      locals_size,
      stack_size};
}

jit_function jit_compiler::compile(
  std::span<const std::byte> bytecode,
  const std::vector<std::size_t>& local_offsets,
  std::size_t locals_size,
  const std::vector<jit_call_target*>& call_targets,
  const std::vector<jit_call_target*>& import_call_targets,
  const std::function<module_::struct_descriptor(std::int64_t)>& resolve_type)
{
    /*
     * AAPCS64 Register Mapping & Frame Layout
     * ----------------------------------------
     * Callee-Saved Registers (Preserved in Prologue, Restored in Epilogue):
     *   X19 = Pointer to frame->locals.data()
     *   X20 = Pointer to frame->stack.data()
     *   X21 = Preserved pointer to runtime `si::stack_frame` (moved from X0)
     *   X22 = General-purpose JIT scratch register
     *
     * Volatile / Parameter Registers:
     *   X0  = Runtime parameter: Pointer to `si::stack_frame`
     */

    instruction_emitter emitter;
    std::unordered_map<std::int64_t, std::size_t> labels;
    std::vector<std::size_t> return_fixups;
    std::vector<std::size_t> exception_fixups;
    std::vector<branch_fixup> branch_fixups;

    /*
     * Prologue.
     */

    emitter.push_fp_lr();

    // Save callee-saved registers X19–X22 on the stack.
    emitter.stp_x_pre(
      cpu_registers::X19,
      cpu_registers::X20,
      cpu_registers::SP,
      -2 * x_register_size);
    emitter.stp_x_pre(
      cpu_registers::X21,
      cpu_registers::X22,
      cpu_registers::SP,
      -2 * x_register_size);

    // Preserve the incoming stack-frame pointer in X21
    emitter.mov_reg_x(cpu_registers::X21, cpu_registers::X0);

    // Get dynamic offsets regardless of non-standard layout rules
    const auto [locals_offset, stack_offset] = calculate_stack_frame_offsets();

    // The offsets should always be smaller, otherwise something is wrong
    assert(locals_offset < std::numeric_limits<std::uint32_t>::max());
    assert(stack_offset < std::numeric_limits<std::uint32_t>::max());

    // Load frame->locals.data() into X19
    emitter.ldr_x(
      cpu_registers::X19,
      cpu_registers::X0,
      static_cast<std::uint32_t>(locals_offset));

    // Load frame->stack.data() into X20
    emitter.ldr_x(
      cpu_registers::X20,
      cpu_registers::X0,
      static_cast<std::uint32_t>(stack_offset));

    /*
     * Compile bytecode.
     */

    std::size_t pc = 0;

    std::uint32_t max_stack_size{0};
    std::uint32_t current_stack_size{0};
    memory_read_archive input{bytecode, true, std::endian::little};

    const auto update_stack_size =
      [&](opcode instr, std::int32_t delta)
    {
        if(delta == 0)
        {
            return;
        }

        if(delta < 0
           && std::cmp_less(current_stack_size, -delta))
        {
            throw jit_error{
              std::format(
                "Got negative stack size while decoding instruction '{}'.",
                to_string(instr))};
        }

        current_stack_size += delta;
        max_stack_size = std::max(max_stack_size, current_stack_size);
    };

    const auto emit_integer_instruction =
      [&](bool is_64_bit, auto emit_w, auto emit_x, auto... args)
    {
        (emitter.*(is_64_bit ? emit_x : emit_w))(args...);
    };

    const auto emit_integer_binary =
      [&](opcode instr, bool is_64_bit)
    {
        const std::uint32_t width = is_64_bit ? 8u : 4u;
        if(current_stack_size < 2u * width)
        {
            throw jit_error{
              std::format(
                "Not enough stack values for '{}'.",
                to_string(instr))};
        }

        const auto left_offset = current_stack_size - (2u * width);
        const auto right_offset = current_stack_size - width;
        if(is_64_bit)
        {
            emitter.load_x(
              cpu_registers::X0,
              left_offset);
            emitter.load_x(
              cpu_registers::X1,
              right_offset);
        }
        else
        {
            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              left_offset);
            emitter.ldr_w(
              cpu_registers::X1,
              cpu_registers::X20,
              right_offset);
        }

        switch(instr)
        {
        case opcode::iadd: [[fallthrough]];
        case opcode::ladd:
            if(is_64_bit)
            {
                emitter.add_x(
                  cpu_registers::X0,
                  cpu_registers::X0,
                  cpu_registers::X1);
            }
            else
            {
                emitter.add_w(
                  cpu_registers::X0,
                  cpu_registers::X0,
                  cpu_registers::X1);
            }
            break;
        case opcode::isub: [[fallthrough]];
        case opcode::lsub:
            if(is_64_bit)
            {
                emitter.sub_x(
                  cpu_registers::X0,
                  cpu_registers::X0,
                  cpu_registers::X1);
            }
            else
            {
                emitter.sub_w(
                  cpu_registers::X0,
                  cpu_registers::X0,
                  cpu_registers::X1);
            }
            break;
        case opcode::imul: [[fallthrough]];
        case opcode::lmul:
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::mul_w,
              &instruction_emitter::mul_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            break;
        case opcode::idiv: [[fallthrough]];
        case opcode::ldiv:
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::sdiv_w,
              &instruction_emitter::sdiv_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            break;
        case opcode::imod: [[fallthrough]];
        case opcode::lmod:
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::sdiv_w,
              &instruction_emitter::sdiv_x,
              cpu_registers::X2,
              cpu_registers::X0,
              cpu_registers::X1);
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::msub_w,
              &instruction_emitter::msub_x,
              cpu_registers::X0,
              cpu_registers::X2,
              cpu_registers::X1,
              cpu_registers::X0);
            break;
        case opcode::iand:
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::and_reg_w,
              &instruction_emitter::and_reg_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            break;
        case opcode::ior:
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::orr_reg_w,
              &instruction_emitter::orr_reg_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            break;
        case opcode::ixor: [[fallthrough]];
        case opcode::lxor:
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::eor_reg_w,
              &instruction_emitter::eor_reg_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            break;
        default:
            throw jit_error{
              std::format(
                "Unsupported integer operation '{}'.",
                to_string(instr))};
        }

        if(is_64_bit)
        {
            emitter.store_x(
              cpu_registers::X0,
              left_offset);
        }
        else
        {
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              left_offset);
        }
        update_stack_size(instr, -static_cast<std::int32_t>(width));
    };

    const auto emit_integer_shift =
      [&](opcode instr, bool is_64_bit)
    {
        const std::uint32_t width = is_64_bit ? 8u : 4u;
        if(current_stack_size < width + 4u)
        {
            throw jit_error{
              std::format(
                "Not enough stack values for '{}'.",
                to_string(instr))};
        }

        const auto value_offset = current_stack_size - width - 4u;
        const auto shift_offset = current_stack_size - 4u;
        if(is_64_bit)
        {
            emitter.load_x(
              cpu_registers::X0,
              value_offset);
        }
        else
        {
            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              value_offset);
        }
        emitter.ldr_w(
          cpu_registers::X1,
          cpu_registers::X20,
          shift_offset);

        if(instr == opcode::ishl
           || instr == opcode::lshl)
        {
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::lslv_w,
              &instruction_emitter::lslv_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
        }
        else
        {
            emit_integer_instruction(
              is_64_bit,
              &instruction_emitter::lsrv_w,
              &instruction_emitter::lsrv_x,
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
        }

        if(is_64_bit)
        {
            emitter.store_x(
              cpu_registers::X0,
              value_offset);
        }
        else
        {
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              value_offset);
        }

        update_stack_size(instr, -4);
    };

    const auto emit_integer_negation =
      [&](opcode instr, bool is_64_bit)
    {
        const std::uint32_t width = is_64_bit ? 8u : 4u;
        if(current_stack_size < width)
        {
            throw jit_error{
              std::format(
                "Not enough stack values for '{}'.",
                to_string(instr))};
        }

        const auto offset = current_stack_size - width;
        if(is_64_bit)
        {
            emitter.load_x(
              cpu_registers::X0,
              offset);
            emitter.sub_x(
              cpu_registers::X0,
              cpu_registers::XZR,
              cpu_registers::X0);
            emitter.store_x(
              cpu_registers::X0,
              offset);
        }
        else
        {
            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              offset);
            emitter.sub_w(
              cpu_registers::X0,
              cpu_registers::XZR,
              cpu_registers::X0);
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              offset);
        }
    };

    const auto emit_safepoint = [&]
    {
        const auto helper_address = reinterpret_cast<std::intptr_t>(&run_gc_safepoint);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        emitter.mov_reg_x(
          cpu_registers::X0,
          cpu_registers::X21);
        emitter.mov_x(
          cpu_registers::X16,
          static_cast<std::int64_t>(helper_address));
        emitter.blr(cpu_registers::X16);
    };

    const auto emit_exception_guard = [&]
    {
        const auto helper_address = reinterpret_cast<std::intptr_t>(&has_pending_exception);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        emitter.mov_reg_x(
          cpu_registers::X0,
          cpu_registers::X21);
        emitter.mov_x(
          cpu_registers::X16,
          static_cast<std::int64_t>(helper_address));
        emitter.blr(cpu_registers::X16);

        const auto instruction_index = emitter.code.size();
        emitter.cbnz_w(cpu_registers::X0, 0);
        exception_fixups.push_back(instruction_index);
    };

    const auto read_local_offset =
      [&](opcode instr)
    {
        if(pc >= bytecode.size())
        {
            throw jit_error{
              std::format(
                "Missing local index for '{}'.",
                to_string(instr))};
        }

        input.seek(pc);
        vle_int local_index;
        input & local_index;
        pc = input.tell();

        if(local_index.i < 0
           || static_cast<std::size_t>(local_index.i) >= local_offsets.size())
        {
            throw jit_error{
              std::format(
                "Local index {} for '{}' is out of range.",
                local_index.i,
                to_string(instr))};
        }

        const auto offset = local_offsets.at(
          static_cast<std::size_t>(local_index.i));
        if(offset > std::numeric_limits<std::uint32_t>::max())
        {
            throw jit_error{
              std::format(
                "Local offset {} exceeds the JIT address range.",
                offset)};
        }

        return static_cast<std::uint32_t>(offset);
    };

    // TODO bytecode reading should work through memory_read_archive
    while(pc < bytecode.size())
    {
        auto op = static_cast<opcode>(bytecode[pc++]);

        switch(op)
        {
        case opcode::iconst:
        {
            if(bytecode.size() - pc < sizeof(std::int32_t))
            {
                throw jit_error{
                  "Truncated i32 constant."};
            }

            std::int32_t val{0};
            std::memcpy(
              &val,
              &bytecode[pc],
              sizeof(val));

            pc += sizeof(val);

            auto uval = static_cast<std::uint32_t>(val);
            auto low16 = static_cast<std::uint16_t>(uval & 0xFFFFu);              // NOLINT(readability-magic-numbers)
            auto high16 = static_cast<std::uint16_t>((uval >> 16u) & 0xFFFFu);    // NOLINT(readability-magic-numbers)

            // Always emit MOVZ (low 16 bits) + MOVK (high 16 bits if non-zero)
            emitter.movz_w(
              cpu_registers::X0,
              low16,
              0);
            if(high16 != 0)
            {
                emitter.movk_w(
                  cpu_registers::X0,
                  high16,
                  16);    // Shift 16 bits left // NOLINT(readability-magic-numbers)
            }

            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);

            update_stack_size(op, 4);

            break;
        }
        case opcode::lconst:
        {
            if(bytecode.size() - pc < sizeof(std::int64_t))
            {
                throw jit_error{
                  "Truncated i64 constant."};
            }

            std::int64_t val{0};
            auto constant_bytes = bytecode.subspan(pc, sizeof(val));
            std::memcpy(
              &val,
              constant_bytes.data(),
              sizeof(val));
            pc += sizeof(val);

            emitter.mov_x(
              cpu_registers::X0,
              val);
            emitter.store_x(
              cpu_registers::X0,
              current_stack_size);

            update_stack_size(op, static_cast<std::int32_t>(sizeof(std::int64_t)));

            break;
        }
        case opcode::fconst:
        {
            if(sizeof(float) > bytecode.size() - pc)
            {
                throw jit_error{
                  "Truncated f32 constant."};
            }

            float value{0};
            std::memcpy(
              &value,
              bytecode.data() + pc,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
              sizeof(value));
            pc += sizeof(value);

            emitter.mov_w(
              cpu_registers::X0,
              static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)));
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);
            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(float)));

            break;
        }
        case opcode::dconst:
        {
            if(sizeof(double) > bytecode.size() - pc)
            {
                throw jit_error{
                  "Truncated f64 constant."};
            }

            double value{0};
            std::memcpy(
              &value,
              bytecode.data() + pc,    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
              sizeof(value));
            pc += sizeof(value);

            emitter.mov_x(
              cpu_registers::X0,
              std::bit_cast<std::int64_t>(value));
            emitter.store_x(
              cpu_registers::X0,
              current_stack_size);
            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(double)));

            break;
        }
        case opcode::sconst:
        {
            input.seek(pc);
            vle_int constant_index;
            input & constant_index;
            pc = input.tell();
            if(constant_index.i < 0)
            {
                throw jit_error{
                  "Invalid JIT string constant index."};
            }

            const auto helper_address = reinterpret_cast<std::intptr_t>(&load_string_constant);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              constant_index.i);
            emitter.mov_x(
              cpu_registers::X2,
              current_stack_size);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);
            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(void*)));

            break;
        }
        case opcode::aconst_null:
            emitter.store_x(
              cpu_registers::XZR,
              current_stack_size);
            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(void*)));

            break;
        case opcode::dup:
        {
            if(current_stack_size < sizeof(std::int32_t))
            {
                throw jit_error{
                  "Not enough stack values for 'dup'."};
            }

            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size - sizeof(std::int32_t));
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);
            update_stack_size(
              op,
              sizeof(std::int32_t));

            break;
        }
        case opcode::dup2:
        {
            if(current_stack_size < sizeof(std::int64_t))
            {
                throw jit_error{
                  "Not enough stack values for 'dup2'."};
            }

            emitter.load_x(
              cpu_registers::X0,
              current_stack_size - sizeof(std::int64_t));
            emitter.store_x(
              cpu_registers::X0,
              current_stack_size);
            update_stack_size(
              op,
              sizeof(std::int64_t));

            break;
        }
        case opcode::adup:
        {
            if(current_stack_size < sizeof(void*))
            {
                throw jit_error{
                  "Not enough stack values for 'adup'."};
            }
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(void*));
            const auto helper_address = reinterpret_cast<std::intptr_t>(&duplicate_reference);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);
            update_stack_size(
              op,
              sizeof(void*));

            break;
        }
        case opcode::pop:
            update_stack_size(
              op,
              -static_cast<std::int32_t>(sizeof(std::int32_t)));
            break;
        case opcode::pop2:
            update_stack_size(
              op,
              -static_cast<std::int32_t>(sizeof(std::int64_t)));
            break;
        case opcode::apop:
        {
            if(current_stack_size < sizeof(void*))
            {
                throw jit_error{
                  "Not enough stack values for 'apop'."};
            }
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(void*));
            const auto helper_address = reinterpret_cast<std::intptr_t>(&discard_reference);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);
            update_stack_size(
              op,
              -static_cast<std::int32_t>(sizeof(void*)));

            break;
        }
        case opcode::dup_x1: [[fallthrough]];
        case opcode::dup_x2: [[fallthrough]];
        case opcode::dup2_x0:
        {
            type_class type1;
            type_class type2;
            type_class type3{type_class::cat1};
            input.seek(pc);
            input & type1 & type2;
            if(op == opcode::dup_x2)
            {
                input & type3;
            }
            pc = input.tell();

            const auto size1 = static_cast<std::size_t>(target_type_layout::for_class(type1).size);
            const auto size2 = static_cast<std::size_t>(target_type_layout::for_class(type2).size);
            const auto size3 = static_cast<std::size_t>(target_type_layout::for_class(type3).size);
            const auto needs_gc1 =
              type1 == type_class::ref;
            const auto needs_gc2 =
              op == opcode::dup2_x0
              && type2 == type_class::ref;
            const auto required_size =
              op == opcode::dup_x1
                ? size1 + size2
              : op == opcode::dup_x2 ? size1 + size2 + size3
                                     : size1 + size2;
            if(current_stack_size < required_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto helper_address = reinterpret_cast<std::intptr_t>(&duplicate_stack_block);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(cpu_registers::X1, std::to_underlying(op));
            emitter.mov_x(cpu_registers::X2, size1);
            emitter.mov_x(cpu_registers::X3, size2);
            emitter.mov_x(cpu_registers::X4, size3);
            emitter.mov_x(cpu_registers::X5, needs_gc1);
            emitter.mov_x(cpu_registers::X6, needs_gc2);
            emitter.mov_x(cpu_registers::X7, current_stack_size);
            emitter.mov_x(cpu_registers::X16, static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(
                op == opcode::dup2_x0
                  ? size1 + size2
                  : size1));

            break;
        }
        case opcode::iload:
        case opcode::fload:
        {
            const auto local_offset = read_local_offset(op);

            // Read from X19 (locals), push to X20 (stack)
            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X19,
              local_offset);
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);

            update_stack_size(op, 4);

            break;
        }
        case opcode::lload: [[fallthrough]];
        case opcode::dload:
        {
            const auto local_offset = read_local_offset(op);
            emitter.load_x(
              cpu_registers::X0,
              local_offset);
            emitter.store_x(
              cpu_registers::X0,
              current_stack_size);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(std::int64_t)));

            break;
        }
        case opcode::istore:
        case opcode::fstore:
        {
            const auto local_offset = read_local_offset(op);
            update_stack_size(op, -4);

            // Pop from X20 (stack), write to X19 (locals)
            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X19,
              local_offset);

            break;
        }
        case opcode::lstore: [[fallthrough]];
        case opcode::dstore:
        {
            const auto local_offset = read_local_offset(op);
            update_stack_size(
              op,
              -static_cast<std::int32_t>(sizeof(std::int64_t)));

            emitter.load_x(
              cpu_registers::X0,
              current_stack_size);
            emitter.store_x(
              cpu_registers::X0,
              local_offset);
            break;
        }
        case opcode::aload:
        {
            const auto local_offset = read_local_offset(op);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&load_reference_local);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              local_offset);
            emitter.mov_x(
              cpu_registers::X2,
              current_stack_size);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(void*)));

            break;
        }
        case opcode::astore:
        {
            const auto local_offset = read_local_offset(op);
            if(current_stack_size < sizeof(void*))
            {
                throw jit_error{
                  "Not enough stack values for 'astore'."};
            }

            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(void*));
            const auto helper_address = reinterpret_cast<std::intptr_t>(&store_reference_local);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              local_offset);
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              -static_cast<std::int32_t>(sizeof(void*)));

            break;
        }
        case opcode::icmpl: [[fallthrough]];
        case opcode::lcmpl: [[fallthrough]];
        case opcode::fcmpl: [[fallthrough]];
        case opcode::dcmpl: [[fallthrough]];
        case opcode::icmple: [[fallthrough]];
        case opcode::lcmple: [[fallthrough]];
        case opcode::fcmple: [[fallthrough]];
        case opcode::dcmple: [[fallthrough]];
        case opcode::icmpg: [[fallthrough]];
        case opcode::lcmpg: [[fallthrough]];
        case opcode::fcmpg: [[fallthrough]];
        case opcode::dcmpg: [[fallthrough]];
        case opcode::icmpge: [[fallthrough]];
        case opcode::lcmpge: [[fallthrough]];
        case opcode::fcmpge: [[fallthrough]];
        case opcode::dcmpge: [[fallthrough]];
        case opcode::icmpeq: [[fallthrough]];
        case opcode::lcmpeq: [[fallthrough]];
        case opcode::fcmpeq: [[fallthrough]];
        case opcode::dcmpeq: [[fallthrough]];
        case opcode::icmpne: [[fallthrough]];
        case opcode::lcmpne: [[fallthrough]];
        case opcode::fcmpne: [[fallthrough]];
        case opcode::dcmpne: [[fallthrough]];
        case opcode::acmpeq: [[fallthrough]];
        case opcode::acmpne:
        {
            // TODO Not fully compiled yet.

            const auto operand_size =
              op == opcode::lcmpl || op == opcode::lcmple || op == opcode::lcmpg
                  || op == opcode::lcmpge || op == opcode::lcmpeq || op == opcode::lcmpne
                  || op == opcode::dcmpl || op == opcode::dcmple || op == opcode::dcmpg
                  || op == opcode::dcmpge || op == opcode::dcmpeq || op == opcode::dcmpne
                  || op == opcode::acmpeq || op == opcode::acmpne
                ? sizeof(std::int64_t)
                : sizeof(std::int32_t);

            if(current_stack_size < 2 * operand_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(2 * operand_size);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&compare_values);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              std::to_underlying(op));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(std::int32_t)) - static_cast<std::int32_t>(2 * operand_size));

            break;
        }
        case opcode::newarray:
        {
            if(current_stack_size < sizeof(std::int32_t)
               || pc >= bytecode.size())
            {
                throw jit_error{
                  "Truncated newarray instruction."};
            }
            const auto type = static_cast<module_::array_type>(std::to_integer<std::uint8_t>(bytecode[pc++]));
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(std::int32_t));
            const auto helper_address = reinterpret_cast<std::intptr_t>(&create_array);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              std::to_underlying(type));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t)));

            break;
        }
        case opcode::new_:
        {
            if(!resolve_type)
            {
                throw jit_error{
                  "Struct allocation requires a JIT type resolver."};
            }

            input.seek(pc);
            vle_int type_index;
            input & type_index;
            pc = input.tell();

            const auto type = resolve_type(type_index.i);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&create_struct);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              static_cast<std::int64_t>(type.size));
            emitter.mov_x(
              cpu_registers::X2,
              static_cast<std::int64_t>(type.alignment));
            emitter.mov_x(
              cpu_registers::X3,
              static_cast<std::int64_t>(type.layout_id));
            emitter.mov_x(
              cpu_registers::X4,
              current_stack_size);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(void*)));

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

            input.seek(pc);
            vle_int type_index;
            vle_int field_index;
            input & type_index & field_index;
            pc = input.tell();

            const auto type = resolve_type(type_index.i);
            if(field_index.i < 0
               || static_cast<std::size_t>(field_index.i) >= type.member_types.size())
            {
                throw jit_error{
                  "Struct field index is out of range."};
            }

            const auto& field = type.member_types.at(static_cast<std::size_t>(field_index.i)).second;
            const auto base_type = field.base_type.base_type();
            const bool field_needs_gc =
              field.base_type.is_array()
              || base_type == "str"
              || (base_type != "void" && base_type != "i8" && base_type != "i16" && base_type != "i32"
                  && base_type != "i64" && base_type != "f32" && base_type != "f64");
            const auto input_size = sizeof(void*) + (op == opcode::setfield ? field.size : 0);
            if(current_stack_size < input_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(input_size);
            const auto helper_address = reinterpret_cast<std::intptr_t>(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              op == opcode::setfield
                ? &set_field
                : &get_field);

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              static_cast<std::int64_t>(field.size));
            emitter.mov_x(
              cpu_registers::X2,
              static_cast<std::int64_t>(field.offset));
            emitter.mov_x(
              cpu_registers::X3,
              field_needs_gc);
            emitter.mov_x(
              cpu_registers::X4,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            if(op == opcode::setfield)
            {
                update_stack_size(
                  op,
                  -static_cast<std::int32_t>(input_size));
            }
            else
            {
                update_stack_size(
                  op,
                  static_cast<std::int32_t>(field.size) - static_cast<std::int32_t>(sizeof(void*)));
            }

            break;
        }
        case opcode::checkcast:
        {
            if(!resolve_type
               || current_stack_size < sizeof(void*))
            {
                throw jit_error{
                  "Invalid checkcast instruction."};
            }

            input.seek(pc);
            vle_int type_index;
            input & type_index;
            pc = input.tell();

            const auto type = resolve_type(type_index.i);
            if((type.flags & std::to_underlying(module_::struct_flags::allow_cast)) == 0)
            {
                const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(void*));
                const auto helper_address = reinterpret_cast<std::intptr_t>(&check_cast);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                emitter.mov_reg_x(
                  cpu_registers::X0,
                  cpu_registers::X21);
                emitter.mov_x(
                  cpu_registers::X1,
                  stack_offset);
                emitter.mov_x(
                  cpu_registers::X2,
                  static_cast<std::int64_t>(type.layout_id));
                emitter.mov_x(
                  cpu_registers::X16,
                  static_cast<std::int64_t>(helper_address));
                emitter.blr(cpu_registers::X16);
            }

            break;
        }
        case opcode::anewarray:
        {
            if(!resolve_type
               || current_stack_size < sizeof(std::int32_t))
            {
                throw jit_error{
                  "Truncated anewarray instruction."};
            }

            input.seek(pc);
            vle_int type_index;
            input & type_index;
            pc = input.tell();

            const auto type = resolve_type(type_index.i);
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(std::int32_t));
            const auto helper_address = reinterpret_cast<std::intptr_t>(&create_struct_array);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              static_cast<std::int64_t>(type.layout_id));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t)));

            break;
        }
        case opcode::arraylength:
        {
            if(current_stack_size < sizeof(void*))
            {
                throw jit_error{
                  "Not enough stack values for 'arraylength'."};
            }
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(void*));
            const auto helper_address = reinterpret_cast<std::intptr_t>(&get_array_length);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.load_x(
              cpu_registers::X1,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);
            emitter.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              stack_offset);

            update_stack_size(
              op,
              static_cast<std::int32_t>(sizeof(std::int32_t)) - static_cast<std::int32_t>(sizeof(void*)));

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
            constexpr auto input_size = sizeof(void*) + sizeof(std::int32_t);
            if(current_stack_size < input_size)
            {
                throw jit_error{
                  std::format("Not enough stack values for '{}'.", to_string(op))};
            }
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(input_size);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&load_array_element);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              std::to_underlying(op));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            const auto result_size =
              op == opcode::laload || op == opcode::daload || op == opcode::aaload
                ? sizeof(std::int64_t)
                : sizeof(std::int32_t);
            update_stack_size(
              op,
              static_cast<std::int32_t>(result_size) - static_cast<std::int32_t>(input_size));

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
            const auto value_size =
              op == opcode::lastore || op == opcode::dastore || op == opcode::aastore
                ? sizeof(std::int64_t)
                : sizeof(std::int32_t);
            const auto input_size = sizeof(void*) + sizeof(std::int32_t) + value_size;
            if(current_stack_size < input_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }
            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(input_size);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&store_array_element);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              std::to_underlying(op));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              -static_cast<std::int32_t>(input_size));

            break;
        }
        case opcode::invoke:
        {
            input.seek(pc);
            vle_int function_index;
            input & function_index;
            pc = input.tell();

            const jit_call_target* target = nullptr;
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
                target = import_call_targets.at(static_cast<std::size_t>(import_index));
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
                target = call_targets.at(static_cast<std::size_t>(function_index.i));
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

            const auto argument_offset = current_stack_size - static_cast<std::uint32_t>(target->argument_size);
            const auto target_address = reinterpret_cast<std::intptr_t>(target);                 // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            const auto helper_address = reinterpret_cast<std::intptr_t>(&invoke_call_target);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_x(
              cpu_registers::X0,
              static_cast<std::int64_t>(target_address));
            emitter.mov_reg_x(
              cpu_registers::X1,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X2,
              static_cast<std::int64_t>(argument_offset));
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);
            emit_exception_guard();

            update_stack_size(
              op,
              static_cast<std::int32_t>(target->return_size)
                - static_cast<std::int32_t>(target->argument_size));

            break;
        }
        case opcode::label:
        {
            input.seek(pc);
            vle_int label_id;
            input & label_id;
            pc = input.tell();

            if(!labels.emplace(label_id.i, emitter.code.size()).second)
            {
                throw jit_error{
                  std::format(
                    "Duplicate JIT label {}.",
                    label_id.i)};
            }

            break;
        }
        case opcode::jmp:
        {
            input.seek(pc);
            vle_int label_id;
            input & label_id;
            pc = input.tell();

            emit_safepoint();

            const auto instruction_index = emitter.code.size();
            emitter.b(0);

            branch_fixups.emplace_back(branch_fixup{
              .instruction_index = instruction_index,
              .label_id = label_id.i,
              .conditional = false});

            break;
        }
        case opcode::jnz:
        {
            if(current_stack_size < sizeof(std::int32_t))
            {
                throw jit_error{
                  "Not enough stack values for 'jnz'."};
            }

            input.seek(pc);
            vle_int then_label;
            vle_int else_label;
            input & then_label & else_label;
            pc = input.tell();

            const auto condition_offset = current_stack_size - static_cast<std::uint32_t>(sizeof(std::int32_t));
            update_stack_size(
              op,
              -static_cast<std::int32_t>(sizeof(std::int32_t)));
            emit_safepoint();

            emitter.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              condition_offset);

            const auto conditional_index = emitter.code.size();
            emitter.cbnz_w(cpu_registers::X0, 0);
            branch_fixups.push_back(branch_fixup{
              .instruction_index = conditional_index,
              .label_id = then_label.i,
              .conditional = true,
            });

            const auto else_index = emitter.code.size();
            emitter.b(0);
            branch_fixups.push_back(branch_fixup{
              .instruction_index = else_index,
              .label_id = else_label.i,
              .conditional = false,
            });

            break;
        }
        case opcode::iadd: [[fallthrough]];
        case opcode::isub: [[fallthrough]];
        case opcode::imul: [[fallthrough]];
        case opcode::idiv: [[fallthrough]];
        case opcode::imod: [[fallthrough]];
        case opcode::iand: [[fallthrough]];
        case opcode::ior: [[fallthrough]];
        case opcode::ixor:
            emit_integer_binary(op, false);
            break;
        case opcode::ladd: [[fallthrough]];
        case opcode::lsub: [[fallthrough]];
        case opcode::lmul: [[fallthrough]];
        case opcode::ldiv: [[fallthrough]];
        case opcode::lmod: [[fallthrough]];
        case opcode::lxor:
            emit_integer_binary(op, true);
            break;
        case opcode::fadd: [[fallthrough]];
        case opcode::fsub: [[fallthrough]];
        case opcode::fmul: [[fallthrough]];
        case opcode::fdiv: [[fallthrough]];
        case opcode::dadd: [[fallthrough]];
        case opcode::dsub: [[fallthrough]];
        case opcode::dmul: [[fallthrough]];
        case opcode::ddiv:
        {
            // TODO Not fully compiled yet.

            const bool is_double =
              op == opcode::dadd
              || op == opcode::dsub
              || op == opcode::dmul
              || op == opcode::ddiv;
            const auto width =
              is_double
                ? sizeof(double)
                : sizeof(float);

            if(current_stack_size < 2 * width)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(2 * width);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&execute_fp_operation);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              std::to_underlying(op));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            update_stack_size(
              op,
              -static_cast<std::int32_t>(width));

            break;
        }
        case opcode::i2f: [[fallthrough]];
        case opcode::d2f: [[fallthrough]];
        case opcode::f2d: [[fallthrough]];
        case opcode::d2i:
        {
            const auto input_size =
              op == opcode::d2f || op == opcode::d2i
                ? sizeof(double)
                : sizeof(std::int32_t);

            if(current_stack_size < input_size)
            {
                throw jit_error{
                  std::format(
                    "Not enough stack values for '{}'.",
                    to_string(op))};
            }

            const auto stack_offset = current_stack_size - static_cast<std::uint32_t>(input_size);
            const auto helper_address = reinterpret_cast<std::intptr_t>(&numeric_conversion);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            emitter.mov_reg_x(
              cpu_registers::X0,
              cpu_registers::X21);
            emitter.mov_x(
              cpu_registers::X1,
              std::to_underlying(op));
            emitter.mov_x(
              cpu_registers::X2,
              stack_offset);
            emitter.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            emitter.blr(cpu_registers::X16);

            const auto output_size =
              op == opcode::d2i || op == opcode::d2f
                ? sizeof(std::int32_t)
                : sizeof(double);
            update_stack_size(
              op,
              static_cast<std::int32_t>(output_size) - static_cast<std::int32_t>(input_size));

            break;
        }
        case opcode::ishl: [[fallthrough]];
        case opcode::ishr:
            emit_integer_shift(op, false);
            break;
        case opcode::lshl: [[fallthrough]];
        case opcode::lshr:
            emit_integer_shift(op, true);
            break;
        case opcode::ineg:
            emit_integer_negation(op, false);
            break;
        case opcode::lneg:
            emit_integer_negation(op, true);
            break;
        case opcode::iret: [[fallthrough]];
        case opcode::lret: [[fallthrough]];
        case opcode::fret: [[fallthrough]];
        case opcode::dret: [[fallthrough]];
        case opcode::aret: [[fallthrough]];
        case opcode::ret:
        {
            return_fixups.push_back(emitter.code.size());
            emitter.b(0);
            break;
        }
        default:
            throw jit_error{
              std::format(
                "Unimplemented opcode '{}'.",
                to_string(op))};
        }
    }

    for(const auto& fixup: branch_fixups)
    {
        const auto label_it = labels.find(fixup.label_id);
        if(label_it == labels.end())
        {
            throw jit_error{
              std::format(
                "Unknown JIT label {}.",
                fixup.label_id)};
        }

        if(fixup.conditional)
        {
            emitter.patch_cbnz_w(
              fixup.instruction_index,
              label_it->second);
        }
        else
        {
            emitter.patch_b(
              fixup.instruction_index,
              label_it->second);
        }
    }

    const auto epilogue_index = emitter.code.size();
    for(const auto instruction_index: return_fixups)
    {
        emitter.patch_b(
          instruction_index,
          epilogue_index);
    }

    for(const auto instruction_index: exception_fixups)
    {
        emitter.patch_cbnz_w(
          instruction_index,
          epilogue_index);
    }

    emitter.ldp_x_post(
      cpu_registers::X21,
      cpu_registers::X22,
      cpu_registers::SP,
      2 * x_register_size);
    emitter.ldp_x_post(
      cpu_registers::X19,
      cpu_registers::X20,
      cpu_registers::SP,
      2 * x_register_size);
    emitter.pop_fp_lr();
    emitter.ret();

    return create_jit_function(
      emitter.code,
      locals_size,
      max_stack_size);
}

}    // namespace slang::jit::aarch64
