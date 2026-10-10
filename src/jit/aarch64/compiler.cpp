/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <sys/mman.h>
#include <span>
#include <type_traits>

#include "archives/memory.h"
#include "jit/aarch64.h"

namespace si = slang::interpreter;

namespace slang::jit
{

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
static void invoke_call_target(
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
      [target, arguments, return_slot](const auto& func) -> void
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
              std::ranges::copy(
                arguments,
                callee.locals.begin());

              func(&callee);

              if(target->return_size > callee.stack.size())
              {
                  throw jit_error{
                    "JIT function return value exceeds its operand stack."};
              }

              if(target->return_size != 0)
              {
                  std::ranges::copy(
                    callee.stack.span().first(target->return_size),
                    return_slot.begin());    // seems to be a std::span<const std::byte>
              }
          }
          else if constexpr(std::is_same_v<T, native_function_type>)
          {
              auto native_stack = si::operand_stack::with_capacity(
                target->argument_size + target->return_size);
              native_stack.push_bytes(arguments);

              func(native_stack);

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

}    // namespace slang::jit

namespace
{

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
  const std::vector<jit_call_target*>& import_call_targets)
{
    /*
     * Register Mapping
     * ----------------
     * X0  = Pointer to stack_frame passed at runtime
     * X19 = Direct pointer to frame->locals.data()
     * X20 = Direct pointer to frame->stack.data()
     */

    instruction_emitter e;

    /*
     * Prologue.
     */

    e.push_fp_lr();

    // Save callee-saved registers X19–X22 on the stack.
    e.stp_x_pre(
      cpu_registers::X19,
      cpu_registers::X20,
      cpu_registers::SP,
      -2 * x_register_size);
    e.stp_x_pre(
      cpu_registers::X21,
      cpu_registers::X22,
      cpu_registers::SP,
      -2 * x_register_size);

    // Preserve the incoming stack-frame pointer in X21.
    e.mov_reg_x(cpu_registers::X21, cpu_registers::X0);

    // Get dynamic offsets regardless of non-standard layout rules
    const auto [locals_offset, stack_offset] = calculate_stack_frame_offsets();

    // The offsets should always be smaller, otherwise something is wrong.
    assert(locals_offset < std::numeric_limits<std::uint32_t>::max());
    assert(stack_offset < std::numeric_limits<std::uint32_t>::max());

    // Load frame->locals.data() into X19
    e.ldr_x(
      cpu_registers::X19,
      cpu_registers::X0,
      static_cast<std::uint32_t>(locals_offset));

    // Load frame->stack.data() into X20
    e.ldr_x(
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

    const auto load_x =
      [&](cpu_registers reg, std::uint32_t offset)
    {
        if(offset % sizeof(std::int64_t) == 0)
        {
            e.ldr_x(reg, cpu_registers::X20, offset);
        }
        else
        {
            e.mov_x(cpu_registers::X2, static_cast<std::int64_t>(offset));
            e.add_x(cpu_registers::X2, cpu_registers::X20, cpu_registers::X2);
            e.ldr_x(reg, cpu_registers::X2, 0);
        }
    };

    const auto store_x =
      [&](cpu_registers reg, std::uint32_t offset)
    {
        if(offset % sizeof(std::int64_t) == 0)
        {
            e.str_x(reg, cpu_registers::X20, offset);
        }
        else
        {
            e.mov_x(cpu_registers::X2, static_cast<std::int64_t>(offset));
            e.add_x(cpu_registers::X2, cpu_registers::X20, cpu_registers::X2);
            e.str_x(reg, cpu_registers::X2, 0);
        }
    };

    const auto emit_integer_instruction =
      [&](bool is_64_bit, auto emit_w, auto emit_x, auto... args)
    {
        (e.*(is_64_bit ? emit_x : emit_w))(args...);
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
            load_x(cpu_registers::X0, left_offset);
            load_x(cpu_registers::X1, right_offset);
        }
        else
        {
            e.ldr_w(cpu_registers::X0, cpu_registers::X20, left_offset);
            e.ldr_w(cpu_registers::X1, cpu_registers::X20, right_offset);
        }

        switch(instr)
        {
        case opcode::iadd: [[fallthrough]];
        case opcode::ladd:
            if(is_64_bit)
            {
                e.add_x(cpu_registers::X0, cpu_registers::X0, cpu_registers::X1);
            }
            else
            {
                e.add_w(cpu_registers::X0, cpu_registers::X0, cpu_registers::X1);
            }
            break;
        case opcode::isub: [[fallthrough]];
        case opcode::lsub:
            if(is_64_bit)
            {
                e.sub_x(cpu_registers::X0, cpu_registers::X0, cpu_registers::X1);
            }
            else
            {
                e.sub_w(cpu_registers::X0, cpu_registers::X0, cpu_registers::X1);
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
            throw jit_error{std::format("Unsupported integer operation '{}'.", to_string(instr))};
        }

        if(is_64_bit)
        {
            store_x(cpu_registers::X0, left_offset);
        }
        else
        {
            e.str_w(cpu_registers::X0, cpu_registers::X20, left_offset);
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
              std::format("Not enough stack values for '{}'.", to_string(instr))};
        }

        const auto value_offset = current_stack_size - width - 4u;
        const auto shift_offset = current_stack_size - 4u;
        if(is_64_bit)
        {
            load_x(cpu_registers::X0, value_offset);
        }
        else
        {
            e.ldr_w(cpu_registers::X0, cpu_registers::X20, value_offset);
        }
        e.ldr_w(cpu_registers::X1, cpu_registers::X20, shift_offset);

        if(instr == opcode::ishl || instr == opcode::lshl)
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
            store_x(cpu_registers::X0, value_offset);
        }
        else
        {
            e.str_w(cpu_registers::X0, cpu_registers::X20, value_offset);
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
              std::format("Not enough stack values for '{}'.", to_string(instr))};
        }

        const auto offset = current_stack_size - width;
        if(is_64_bit)
        {
            load_x(cpu_registers::X0, offset);
            e.sub_x(cpu_registers::X0, cpu_registers::XZR, cpu_registers::X0);
            store_x(cpu_registers::X0, offset);
        }
        else
        {
            e.ldr_w(cpu_registers::X0, cpu_registers::X20, offset);
            e.sub_w(cpu_registers::X0, cpu_registers::XZR, cpu_registers::X0);
            e.str_w(cpu_registers::X0, cpu_registers::X20, offset);
        }
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
            e.movz_w(
              cpu_registers::X0,
              low16,
              0);
            if(high16 != 0)
            {
                e.movk_w(
                  cpu_registers::X0,
                  high16,
                  16);    // Shift 16 bits left // NOLINT(readability-magic-numbers)
            }

            e.str_w(
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

            e.mov_x(
              cpu_registers::X0,
              val);
            store_x(
              cpu_registers::X0,
              current_stack_size);

            update_stack_size(op, static_cast<std::int32_t>(sizeof(std::int64_t)));

            break;
        }
        case opcode::iload:
        {
            const auto local_offset = read_local_offset(op);

            // Read from X19 (locals), push to X20 (stack)
            e.ldr_w(
              cpu_registers::X0,
              cpu_registers::X19,
              local_offset);
            e.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);

            update_stack_size(op, 4);

            break;
        }
        case opcode::istore:
        {
            const auto local_offset = read_local_offset(op);
            update_stack_size(op, -4);

            // Pop from X20 (stack), write to X19 (locals)
            e.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);
            e.str_w(
              cpu_registers::X0,
              cpu_registers::X19,
              local_offset);

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
            e.mov_x(
              cpu_registers::X0,
              static_cast<std::int64_t>(target_address));
            e.mov_reg_x(
              cpu_registers::X1,
              cpu_registers::X21);
            e.mov_x(
              cpu_registers::X2,
              static_cast<std::int64_t>(argument_offset));
            e.mov_x(
              cpu_registers::X16,
              static_cast<std::int64_t>(helper_address));
            e.blr(cpu_registers::X16);

            update_stack_size(
              op,
              static_cast<std::int32_t>(target->return_size)
                - static_cast<std::int32_t>(target->argument_size));

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
        case opcode::ret:
        {
            // Epilogue: restore callee-saved registers

            e.ldp_x_post(
              cpu_registers::X21,
              cpu_registers::X22,
              cpu_registers::SP,
              2 * x_register_size);

            // Pop X19 and X20
            e.ldp_x_post(
              cpu_registers::X19,
              cpu_registers::X20,
              cpu_registers::SP,
              2 * x_register_size);

            e.pop_fp_lr();
            e.ret();

            break;
        }
        default:
            throw jit_error{
              "Unimplemented opcode."};
        }
    }

    return create_jit_function(
      e.code,
      locals_size,
      max_stack_size);
}

}    // namespace slang::jit::aarch64
