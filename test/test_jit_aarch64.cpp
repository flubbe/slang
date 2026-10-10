/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler tests (AArch64).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "archives/memory.h"
#include "jit/aarch64.h"
#include "package.h"

namespace
{

namespace si = slang::interpreter;
namespace sj = slang::jit;

using namespace sj::aarch64;

template<typename T>
using constant_serializer = slang::constant_serializer<T>;
using slang::memory_write_archive;
using slang::opcode;

namespace
{
struct hex_value
{
    std::uint32_t value{0};

    bool operator==(const hex_value&) const = default;
};

inline std::ostream& operator<<(
  std::ostream& os,
  hex_value v)
{
    return os << "0x"
              << std::hex
              << std::setw(8)    // NOLINT(readability-magic-numbers)
              << std::setfill('0')
              << v.value;
}

/** Bytecode emission helper. */
template<typename T>
void emit(
  memory_write_archive& archive,
  const T& v)
{
    archive& constant_serializer<T>(v);
}

const std::vector<slang::module_::constant_table_entry>& make_empty_constants()
{
    static std::vector<slang::module_::constant_table_entry> empty;
    return empty;
}

template<typename T>
T run_bytecode(
  std::span<const std::byte> bytecode,
  std::size_t result_offset = 0)
{
    auto function = jit_compiler::compile(
      bytecode,
      {} /* local_offsets */,
      0 /* locals_size */,
      {} /* call_targets */,
      {} /* import_call_targets */);
    auto frame = si::stack_frame::with_size(
      make_empty_constants(),
      function.get_locals_size(),
      function.get_stack_size());
    function(&frame);

    T result{};
    auto result_bytes = frame.stack.span().subspan(
      result_offset,
      sizeof(result));
    std::memcpy(
      &result,
      result_bytes.data(),
      sizeof(result));
    return result;
}

}    // namespace

// NOLINTBEGIN(readability-magic-numbers)

TEST(jit, instruction_encodings_i32)
{
    /*
     * Compare generated machine code against
     * manually generated/assembled references.
     */

    instruction_emitter emitter;

    emitter.add_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x0b020020});

    emitter.and_reg_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x0a020020});

    emitter.eor_reg_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x4a020020});

    emitter.ldr_w(cpu_registers::X0, cpu_registers::X1, 4);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xb9400420});

    emitter.lslv_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x1ac22020});

    emitter.lsrv_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x1ac22420});

    emitter.movn_w(cpu_registers::X0, 0x1234);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x12824680});

    emitter.msub_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2, cpu_registers::X3);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x1b028c20});

    emitter.mul_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x1b027c20});

    emitter.orr_reg_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x2a020020});

    emitter.sdiv_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x1ac20c20});

    emitter.sub_w(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x4b020020});
}

TEST(jit, instruction_encodings_i64)
{
    /*
     * Compare generated machine code against
     * manually generated/assembled references.
     */

    instruction_emitter emitter;

    emitter.add_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x8b020020});

    emitter.and_reg_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x8a020020});

    emitter.eor_reg_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xca020020});

    emitter.ldr_x(cpu_registers::X0, cpu_registers::X1, 8);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xf9400420});

    emitter.lslv_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x9ac22020});

    emitter.lsrv_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x9ac22420});

    emitter.mov_reg_x(cpu_registers::X0, cpu_registers::X1);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xaa0103e0});

    emitter.movk_x(cpu_registers::X0, 0x5678, 0x10);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xf2aacf00});

    emitter.movz_x(cpu_registers::X0, 0x1234);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xd2824680});

    emitter.msub_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2, cpu_registers::X3);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x9b028c20});

    emitter.mul_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x9b027c20});

    emitter.orr_reg_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xaa020020});

    emitter.sdiv_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x9ac20c20});

    emitter.str_x(cpu_registers::X0, cpu_registers::X1, 8);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xf9000420});

    emitter.sub_x(cpu_registers::X0, cpu_registers::X1, cpu_registers::X2);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xcb020020});
}

TEST(jit, instruction_encodings_others)
{
    /*
     * Compare generated machine code against
     * manually generated/assembled references.
     */

    instruction_emitter emitter;

    emitter.blr(cpu_registers::X16);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xd63f0200});

    emitter.b(-24);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x17fffffa});

    emitter.cbnz_w(cpu_registers::X4, 16);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x35000084});

    emitter.ldp_x_post(cpu_registers::X19, cpu_registers::X20, cpu_registers::SP, 0x10);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa8c153f3});

    emitter.stp_x_pre(cpu_registers::X19, cpu_registers::X20, cpu_registers::SP, -0x10);
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa9bf53f3});

    emitter.push_fp_lr();
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa9bf7bfd});

    emitter.pop_fp_lr();
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa8c17bfd});

    emitter.ret();
    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xd65f03c0});
}

// NOLINTEND(readability-magic-numbers)

TEST(jit, compile_execute_sample_function)
{
    memory_write_archive bytecode_ar{false /* persistent */};

    /*
     * Locals
     *   i32 in slot 0
     *
     * Bytecode:
     *                    Stack size
     *                         0
     *   iconst 100            4
     *   istore 0              0
     *   iload 0               4
     *   iconst 42             8
     *   iadd                  4
     *   iconst 10             8
     *   isub                  4
     *   ret
     */

    // iconst 100
    emit(bytecode_ar, opcode::iconst);
    emit<std::int32_t>(bytecode_ar, 100);    // NOLINT(readability-magic-numbers)

    // istore 0  (locals[0] = 100)
    emit(bytecode_ar, opcode::istore);
    emit(bytecode_ar, slang::vle_int{0});    // slot 0

    // iload 0
    emit(bytecode_ar, opcode::iload);
    emit(bytecode_ar, slang::vle_int{0});    // slot 0

    // iconst 42
    emit(bytecode_ar, opcode::iconst);
    emit<std::int32_t>(bytecode_ar, 42);    // NOLINT(readability-magic-numbers)

    // iadd (100 + 42 = 142)
    emit(bytecode_ar, opcode::iadd);

    // iconst 10
    emit(bytecode_ar, opcode::iconst);
    emit<std::int32_t>(bytecode_ar, 10);    // NOLINT(readability-magic-numbers)

    // isub (142 - 10 = 132)
    emit(bytecode_ar, opcode::isub);

    // ret
    emit(bytecode_ar, opcode::ret);

    // Compile
    std::optional<sj::jit_function> compiled_fn;
    ASSERT_NO_THROW(
      compiled_fn = sj::jit_compiler::compile(
        bytecode_ar.get_buffer(),
        std::vector<std::size_t>{0},
        4,
        {},
        {}));
    ASSERT_TRUE(compiled_fn.has_value());
    ASSERT_TRUE(compiled_fn->get() != nullptr);

    // Setup stack frame
    std::optional<si::stack_frame> frame;
    ASSERT_NO_THROW(frame.emplace(
      sj::si::stack_frame::with_size(
        make_empty_constants(),
        4,
        compiled_fn->get_stack_size())));
    ASSERT_TRUE(frame.has_value());

    ASSERT_EQ(frame->locals.size(), 4);
    ASSERT_EQ(frame->stack.size(), compiled_fn->get_stack_size());

    // Run function.
    ASSERT_NO_THROW(compiled_fn->get()(&frame.value()));

    // Verify top of stack value.
    std::int32_t result = 0;
    std::memcpy(
      &result,
      frame->stack.span().data(),
      sizeof(std::int32_t));
    EXPECT_EQ(result, 132);
}

TEST(jit, compile_execute_i32_math)
{
    constexpr std::int32_t expected_result = 42;

    const auto run_binary = [](opcode operation, std::int32_t lhs, std::int32_t rhs)
    {
        memory_write_archive bytecode{false};
        emit(bytecode, opcode::iconst);
        emit(bytecode, lhs);
        emit(bytecode, opcode::iconst);
        emit(bytecode, rhs);
        emit(bytecode, operation);
        emit(bytecode, opcode::iret);
        return run_bytecode<std::int32_t>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_binary(opcode::iadd, 27, 15), expected_result);
    EXPECT_EQ(run_binary(opcode::isub, 57, 15), expected_result);
    EXPECT_EQ(run_binary(opcode::imul, 6, 7), expected_result);
    EXPECT_EQ(run_binary(opcode::idiv, -85, 2), -42);
    EXPECT_EQ(run_binary(opcode::imod, -85, 2), -1);
    EXPECT_EQ(run_binary(opcode::iand, 0b111010, 0b101010), 0b101010);
    EXPECT_EQ(run_binary(opcode::ior, 0b110000, 0b001010), 0b111010);
    EXPECT_EQ(run_binary(opcode::ixor, 0b111000, 0b101010), 0b010010);

    memory_write_archive negation{false};
    emit(negation, opcode::iconst);
    emit(negation, expected_result);
    emit(negation, opcode::ineg);
    emit(negation, opcode::iret);
    EXPECT_EQ(run_bytecode<std::int32_t>(negation.get_buffer()), -42);

    const auto run_shift = [](opcode operation, std::int32_t value, std::int32_t amount)
    {
        memory_write_archive bytecode{false};
        emit(bytecode, opcode::iconst);
        emit(bytecode, value);
        emit(bytecode, opcode::iconst);
        emit(bytecode, amount);
        emit(bytecode, operation);
        emit(bytecode, opcode::iret);
        return run_bytecode<std::int32_t>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_shift(opcode::ishl, 21, 1), 42);
    EXPECT_EQ(run_shift(opcode::ishr, -84, 1), 0x7fffffd6);
}

TEST(jit, compile_execute_i64_math)
{
    constexpr std::int64_t expected_result = 42;
    constexpr std::int32_t unaligned_prefix_value = 7;

    const auto run_binary = [unaligned_prefix_value](opcode operation, std::int64_t lhs, std::int64_t rhs, bool unaligned = false)
    {
        memory_write_archive bytecode{false};
        if(unaligned)
        {
            emit(bytecode, opcode::iconst);
            emit(bytecode, unaligned_prefix_value);
        }
        emit(bytecode, opcode::lconst);
        emit(bytecode, lhs);
        emit(bytecode, opcode::lconst);
        emit(bytecode, rhs);
        emit(bytecode, operation);
        emit(bytecode, opcode::lret);
        return run_bytecode<std::int64_t>(bytecode.get_buffer(), unaligned ? 4 : 0);
    };

    EXPECT_EQ(run_binary(opcode::ladd, 27, 15), expected_result);
    EXPECT_EQ(run_binary(opcode::ladd, 27, 15, true), expected_result);
    EXPECT_EQ(run_binary(opcode::lsub, 57, 15), expected_result);
    EXPECT_EQ(run_binary(opcode::lmul, 6, 7), expected_result);
    EXPECT_EQ(run_binary(opcode::ldiv, -85, 2), -42);
    EXPECT_EQ(run_binary(opcode::lmod, -85, 2), -1);
    EXPECT_EQ(run_binary(opcode::lxor, 0b111000, 0b101010), 0b010010);

    memory_write_archive negation{false};
    emit(negation, opcode::lconst);
    emit(negation, expected_result);
    emit(negation, opcode::lneg);
    emit(negation, opcode::lret);
    EXPECT_EQ(run_bytecode<std::int64_t>(negation.get_buffer()), -42);

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const auto run_shift = [](opcode operation, std::int64_t value, std::int32_t amount)
    {
        memory_write_archive bytecode{false};
        emit(bytecode, opcode::lconst);
        emit(bytecode, value);
        emit(bytecode, opcode::iconst);
        emit(bytecode, amount);
        emit(bytecode, operation);
        emit(bytecode, opcode::lret);
        return run_bytecode<std::int64_t>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_shift(opcode::lshl, 21, 1), 42);
    EXPECT_EQ(run_shift(opcode::lshr, -84, 1), 0x7fffffffffffffd6);
}

TEST(jit, module_loader)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");

    module_loader mod{
      file_mgr,
      "test_jit",
      std::format("test_jit.{}", slang::package::module_ext)};

    {
        auto& function = mod.get_function("test_ret");
        auto frame = si::stack_frame::with_size(
          make_empty_constants(),
          function.get_locals_size(),
          function.get_stack_size());
        function(&frame);

        std::int32_t result = 0;
        std::memcpy(
          &result,
          frame.stack.tail(sizeof(result)).data(),
          sizeof(result));
        EXPECT_EQ(result, 12);
    }

    {
        auto& function = mod.get_function("test_call");
        auto frame = si::stack_frame::with_size(
          make_empty_constants(),
          function.get_locals_size(),
          function.get_stack_size());
        function(&frame);

        std::int32_t result = 0;
        std::memcpy(
          &result,
          frame.stack.tail(sizeof(result)).data(),
          sizeof(result));
        EXPECT_EQ(result, 12);
    }

    {
        auto& function = mod.get_function("test_call_args");
        auto frame = si::stack_frame::with_size(
          make_empty_constants(),
          function.get_locals_size(),
          function.get_stack_size());
        function(&frame);

        std::int32_t result = 0;
        std::memcpy(
          &result,
          frame.stack.span().data(),
          sizeof(result));
        EXPECT_EQ(result, 42);
    }

    {
        auto& function = mod.get_function("test_imported_call_args");
        auto frame = si::stack_frame::with_size(
          make_empty_constants(),
          function.get_locals_size(),
          function.get_stack_size());
        function(&frame);

        std::int32_t result = 0;
        std::memcpy(
          &result,
          frame.stack.span().data(),
          sizeof(result));
        EXPECT_EQ(result, 42);
    }

    mod.register_native_function(
      "test",
      "native_add_i32",
      [](si::operand_stack& stack)
      {
          auto rhs = stack.pop_cat1<std::int32_t>();
          auto lhs = stack.pop_cat1<std::int32_t>();
          stack.push_cat1(lhs + rhs);
      });

    std::size_t native_void_calls = 0;
    mod.register_native_function(
      "test",
      "native_noop",
      [&native_void_calls]([[maybe_unused]] si::operand_stack& stack)
      { ++native_void_calls; });

    {
        auto& function = mod.get_function("test_native_call");
        auto frame = si::stack_frame::with_size(
          make_empty_constants(),
          function.get_locals_size(),
          function.get_stack_size());
        function(&frame);

        std::int32_t result = 0;
        std::memcpy(
          &result,
          frame.stack.span().data(),
          sizeof(result));
        EXPECT_EQ(result, 42);
    }

    {
        auto& function = mod.get_function("test_native_void");
        auto frame = si::stack_frame::with_size(
          make_empty_constants(),
          function.get_locals_size(),
          function.get_stack_size());
        function(&frame);
        EXPECT_EQ(native_void_calls, 1);
    }
}

}    // namespace
