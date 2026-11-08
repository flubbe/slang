/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler tests (x86_64).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <bit>

#include "archives/memory.h"
#include "filemanager.h"
#include "jit/abi/sysv_x86_64/sysv_x86_64.h"
#include "jit/module_loader.h"
#include "package.h"
#include "shared/type_class.h"

namespace
{

namespace sj = slang::jit;

using namespace sj::abi::sysv_x86_64;
using namespace sj::x86_64;
using sj::module_loader;
using sj::stack_slot_size;
using slang::constant_serializer;
using slang::memory_write_archive;
using slang::opcode;

const std::vector<slang::module_::constant_table_entry>& empty_constants()
{
    static const std::vector<slang::module_::constant_table_entry> constants;
    return constants;
}

template<typename T>
void emit_bytecode(memory_write_archive& archive, const T& value)
{
    archive& constant_serializer<T>(value);
}

template<typename T>
T run_bytecode(
  std::span<const std::byte> bytecode,
  std::vector<std::size_t> local_offsets = {},
  std::size_t locals_size = 0,
  slang::gc::garbage_collector* collector = nullptr,
  const std::vector<slang::module_::constant_table_entry>& constants = empty_constants())
{
    auto function = jit_compiler::compile(
      {},
      bytecode,
      local_offsets,
      locals_size,
      {},
      {});
    auto frame = slang::interpreter::stack_frame::with_size(
      constants,
      function.get_locals_size(),
      function.get_stack_size(),
      collector);
    function(&frame);

    T result{};
    std::memcpy(&result, frame.stack.span().data(), sizeof(result));
    return result;
}

template<typename Input, typename Output>
Output run_numeric_conversion(opcode source, opcode conversion, Input value)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, source);
    emit_bytecode(bytecode, value);
    emit_bytecode(bytecode, conversion);
    emit_bytecode(bytecode, opcode::ret);
    return run_bytecode<Output>(bytecode.get_buffer());
}

template<typename T, typename EmitOperation>
T execute_scalar_operation(T lhs, T rhs, EmitOperation emit_operation)
{
    instruction_emitter emitter;
    if constexpr(sizeof(T) == sizeof(std::uint32_t))
    {
        emitter.mov_imm32(cpu_registers::RAX, std::bit_cast<std::uint32_t>(lhs));
        emitter.movd_xmm_r32(simd_registers::XMM0, cpu_registers::RAX);
        emitter.mov_imm32(cpu_registers::RCX, std::bit_cast<std::uint32_t>(rhs));
        emitter.movd_xmm_r32(simd_registers::XMM1, cpu_registers::RCX);
    }
    else
    {
        emitter.mov_imm64(cpu_registers::RAX, std::bit_cast<std::uint64_t>(lhs));
        emitter.movq_xmm_r64(simd_registers::XMM0, cpu_registers::RAX);
        emitter.mov_imm64(cpu_registers::RCX, std::bit_cast<std::uint64_t>(rhs));
        emitter.movq_xmm_r64(simd_registers::XMM1, cpu_registers::RCX);
    }
    emit_operation(emitter);
    emitter.ret();

    sj::executable_memory executable{
      std::as_bytes(std::span{emitter.code})};
    using function_type = T (*)();
    const auto function = reinterpret_cast<function_type>(executable.data());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    return function();
}

TEST(jit_x86_64_sysv, instruction_encodings)
{
    instruction_emitter emitter;

    emitter.mov_imm64(cpu_registers::RAX, 0x1122334455667788);
    EXPECT_EQ(
      emitter.code,
      (std::vector<std::uint8_t>{
        0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11}));

    emitter.code.clear();
    emitter.add_r64(cpu_registers::R8, cpu_registers::R9);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0x4D, 0x01, 0xC8}));

    emitter.code.clear();
    emitter.imul_r64(cpu_registers::RAX, cpu_registers::RCX);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0x48, 0x0F, 0xAF, 0xC1}));

    emitter.code.clear();
    emitter.push(cpu_registers::R12);
    emitter.pop(cpu_registers::R12);
    emitter.ret();
    EXPECT_EQ(
      emitter.code,
      (std::vector<std::uint8_t>{0x41, 0x54, 0x41, 0x5C, 0xC3}));

    emitter.code.clear();
    emitter.movd_xmm_r32(simd_registers::XMM8, cpu_registers::R9);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0x66, 0x45, 0x0F, 0x6E, 0xC1}));

    emitter.code.clear();
    emitter.movq_xmm_r64(simd_registers::XMM0, cpu_registers::RAX);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0x66, 0x48, 0x0F, 0x6E, 0xC0}));

    emitter.code.clear();
    emitter.addss(simd_registers::XMM0, simd_registers::XMM1);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0xF3, 0x0F, 0x58, 0xC1}));

    emitter.code.clear();
    emitter.subsd(simd_registers::XMM0, simd_registers::XMM1);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0xF2, 0x0F, 0x5C, 0xC1}));

    emitter.code.clear();
    emitter.divss(simd_registers::XMM8, simd_registers::XMM9);
    EXPECT_EQ(emitter.code, (std::vector<std::uint8_t>{0xF3, 0x45, 0x0F, 0x5E, 0xC1}));
}

TEST(jit_x86_64_sysv, branch_patching_and_execution)
{
    instruction_emitter emitter;
    emitter.jmp_rel32();
    emitter.mov_imm32(cpu_registers::RAX, 42);
    emitter.ret();
    emitter.patch_jmp_rel32(0, 5);

    EXPECT_EQ(
      emitter.code,
      (std::vector<std::uint8_t>{
        0xE9, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x2A, 0x00, 0x00, 0x00,
        0xC3}));
    EXPECT_THROW(emitter.patch_jmp_rel32(5, 0), sj::jit_error);

    sj::executable_memory executable{
      std::as_bytes(std::span{emitter.code})};
    using function_type = std::int32_t (*)();
    const auto function = reinterpret_cast<function_type>(executable.data());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(function(), 42);
}

TEST(jit_x86_64_sysv, cfg_stack_validation)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{1});
    emit_bytecode(bytecode, opcode::label);
    emit_bytecode(bytecode, slang::vle_int{7});
    emit_bytecode(bytecode, opcode::pop);
    emit_bytecode(bytecode, opcode::ret);

    const auto depths = sj::analyze_bytecode_stack_depths(
      bytecode.get_buffer(),
      {},
      {});
    ASSERT_EQ(depths.size(), 1);
    EXPECT_EQ(depths.at(7), stack_slot_size);

    memory_write_archive underflow{false};
    emit_bytecode(underflow, opcode::pop);
    emit_bytecode(underflow, opcode::ret);
    EXPECT_THROW(
      sj::analyze_bytecode_stack_depths(underflow.get_buffer(), {}, {}),
      sj::jit_error);
}

TEST(jit_x86_64_sysv, cfg_rejects_inconsistent_branch_depths)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{1});
    emit_bytecode(bytecode, opcode::jnz);
    emit_bytecode(bytecode, slang::vle_int{1});
    emit_bytecode(bytecode, slang::vle_int{2});

    emit_bytecode(bytecode, opcode::label);
    emit_bytecode(bytecode, slang::vle_int{1});
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{2});
    emit_bytecode(bytecode, opcode::jmp);
    emit_bytecode(bytecode, slang::vle_int{3});

    emit_bytecode(bytecode, opcode::label);
    emit_bytecode(bytecode, slang::vle_int{2});
    emit_bytecode(bytecode, opcode::jmp);
    emit_bytecode(bytecode, slang::vle_int{3});

    emit_bytecode(bytecode, opcode::label);
    emit_bytecode(bytecode, slang::vle_int{3});
    emit_bytecode(bytecode, opcode::ret);

    EXPECT_THROW(
      sj::analyze_bytecode_stack_depths(bytecode.get_buffer(), {}, {}),
      sj::jit_error);
}

TEST(jit_x86_64_sysv, scalar_floating_point_execution)
{
    EXPECT_FLOAT_EQ(
      execute_scalar_operation<float>(19.0F, 23.0F, [](instruction_emitter& emitter)
                                      { emitter.addss(simd_registers::XMM0, simd_registers::XMM1); }),
      42.0F);
    EXPECT_FLOAT_EQ(
      execute_scalar_operation<float>(19.0F, 23.0F, [](instruction_emitter& emitter)
                                      { emitter.subss(simd_registers::XMM0, simd_registers::XMM1); }),
      -4.0F);
    EXPECT_FLOAT_EQ(
      execute_scalar_operation<float>(6.0F, 7.0F, [](instruction_emitter& emitter)
                                      { emitter.mulss(simd_registers::XMM0, simd_registers::XMM1); }),
      42.0F);
    EXPECT_FLOAT_EQ(
      execute_scalar_operation<float>(84.0F, 2.0F, [](instruction_emitter& emitter)
                                      { emitter.divss(simd_registers::XMM0, simd_registers::XMM1); }),
      42.0F);

    EXPECT_DOUBLE_EQ(
      execute_scalar_operation<double>(19.0, 23.0, [](instruction_emitter& emitter)
                                       { emitter.addsd(simd_registers::XMM0, simd_registers::XMM1); }),
      42.0);
    EXPECT_DOUBLE_EQ(
      execute_scalar_operation<double>(19.0, 23.0, [](instruction_emitter& emitter)
                                       { emitter.subsd(simd_registers::XMM0, simd_registers::XMM1); }),
      -4.0);
    EXPECT_DOUBLE_EQ(
      execute_scalar_operation<double>(6.0, 7.0, [](instruction_emitter& emitter)
                                       { emitter.mulsd(simd_registers::XMM0, simd_registers::XMM1); }),
      42.0);
    EXPECT_DOUBLE_EQ(
      execute_scalar_operation<double>(84.0, 2.0, [](instruction_emitter& emitter)
                                       { emitter.divsd(simd_registers::XMM0, simd_registers::XMM1); }),
      42.0);
}

TEST(jit_x86_64_sysv, compile_execute_i32)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{100});
    emit_bytecode(bytecode, opcode::istore);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::iload);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{42});
    emit_bytecode(bytecode, opcode::iadd);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{10});
    emit_bytecode(bytecode, opcode::isub);
    emit_bytecode(bytecode, opcode::iret);

    EXPECT_EQ(
      run_bytecode<std::int32_t>(bytecode.get_buffer(), {0}, 4),
      132);
}

TEST(jit_x86_64_sysv, compile_execute_i32_wraparound)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::numeric_limits<std::int32_t>::max());
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{1});
    emit_bytecode(bytecode, opcode::iadd);
    emit_bytecode(bytecode, opcode::iret);

    EXPECT_EQ(
      run_bytecode<std::int32_t>(bytecode.get_buffer()),
      std::numeric_limits<std::int32_t>::min());
}

TEST(jit_x86_64_sysv, compile_execute_i64)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::lconst);
    emit_bytecode(bytecode, std::int64_t{-19});
    emit_bytecode(bytecode, opcode::lconst);
    emit_bytecode(bytecode, std::int64_t{23});
    emit_bytecode(bytecode, opcode::ladd);
    emit_bytecode(bytecode, opcode::lret);

    EXPECT_EQ(run_bytecode<std::int64_t>(bytecode.get_buffer()), 4);
}

TEST(jit_x86_64_sysv, compile_execute_f32)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::fconst);
    emit_bytecode(bytecode, 19.0F);
    emit_bytecode(bytecode, opcode::fconst);
    emit_bytecode(bytecode, 23.0F);
    emit_bytecode(bytecode, opcode::fadd);
    emit_bytecode(bytecode, opcode::fret);

    EXPECT_FLOAT_EQ(run_bytecode<float>(bytecode.get_buffer()), 42.0F);
}

TEST(jit_x86_64_sysv, compile_execute_f64)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::dconst);
    emit_bytecode(bytecode, 6.0);
    emit_bytecode(bytecode, opcode::dconst);
    emit_bytecode(bytecode, 7.0);
    emit_bytecode(bytecode, opcode::dmul);
    emit_bytecode(bytecode, opcode::dret);

    EXPECT_DOUBLE_EQ(run_bytecode<double>(bytecode.get_buffer()), 42.0);
}

TEST(jit_x86_64_sysv, compile_execute_stack_manipulation)
{
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{21});
    emit_bytecode(bytecode, opcode::dup);
    emit_bytecode(bytecode, opcode::iadd);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{99});
    emit_bytecode(bytecode, opcode::pop);
    emit_bytecode(bytecode, opcode::iret);

    EXPECT_EQ(run_bytecode<std::int32_t>(bytecode.get_buffer()), 42);
}

TEST(jit_x86_64_sysv, compile_execute_integer_negation)
{
    memory_write_archive i32_bytecode{false};
    emit_bytecode(i32_bytecode, opcode::iconst);
    emit_bytecode(i32_bytecode, std::int32_t{42});
    emit_bytecode(i32_bytecode, opcode::ineg);
    emit_bytecode(i32_bytecode, opcode::iret);
    EXPECT_EQ(run_bytecode<std::int32_t>(i32_bytecode.get_buffer()), -42);

    memory_write_archive i64_bytecode{false};
    emit_bytecode(i64_bytecode, opcode::lconst);
    emit_bytecode(i64_bytecode, std::int64_t{-42});
    emit_bytecode(i64_bytecode, opcode::lneg);
    emit_bytecode(i64_bytecode, opcode::lret);
    EXPECT_EQ(run_bytecode<std::int64_t>(i64_bytecode.get_buffer()), 42);
}

TEST(jit_x86_64_sysv, compile_execute_integer_shifts)
{
    const auto run_shift = []<typename Result>(
                             opcode source,
                             opcode shift,
                             opcode ret,
                             Result value,
                             std::int32_t amount)
    {
        memory_write_archive bytecode{false};
        emit_bytecode(bytecode, source);
        emit_bytecode(bytecode, value);
        emit_bytecode(bytecode, opcode::iconst);
        emit_bytecode(bytecode, amount);
        emit_bytecode(bytecode, shift);
        emit_bytecode(bytecode, ret);
        return run_bytecode<Result>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_shift(opcode::iconst, opcode::ishl, opcode::iret, std::int32_t{21}, 1), 42);
    EXPECT_EQ(run_shift(opcode::iconst, opcode::ishr, opcode::iret, std::int32_t{-84}, 1), -42);
    EXPECT_EQ(run_shift(opcode::lconst, opcode::lshl, opcode::lret, std::int64_t{21}, 1), 42);
    EXPECT_EQ(run_shift(opcode::lconst, opcode::lshr, opcode::lret, std::int64_t{-84}, 1), -42);
    EXPECT_EQ(run_shift(opcode::iconst, opcode::ishl, opcode::iret, std::int32_t{21}, 33), 42);

    memory_write_archive invalid_shift{false};
    emit_bytecode(invalid_shift, opcode::iconst);
    emit_bytecode(invalid_shift, std::int32_t{21});
    emit_bytecode(invalid_shift, opcode::iconst);
    emit_bytecode(invalid_shift, std::int32_t{-1});
    emit_bytecode(invalid_shift, opcode::ishl);
    emit_bytecode(invalid_shift, opcode::iret);
    auto function = jit_compiler::compile(
      {},
      invalid_shift.get_buffer(),
      {},
      0,
      {},
      {});
    auto frame = slang::interpreter::stack_frame::with_size(
      empty_constants(),
      0,
      function.get_stack_size(),
      nullptr);
    function(&frame);
    ASSERT_NE(frame.pending_exception, nullptr);
    try
    {
        std::rethrow_exception(frame.pending_exception);
    }
    catch(const slang::interpreter::interpreter_error& error)
    {
        EXPECT_STREQ(error.what(), "Negative shift.");
    }
    catch(...)
    {
        FAIL() << "Expected interpreter_error for negative shift.";
    }
}

TEST(jit_x86_64_sysv, compile_execute_integer_division_and_remainder)
{
    const auto run_operation = []<typename Value>(
                                 opcode source,
                                 opcode operation,
                                 opcode ret,
                                 Value dividend,
                                 Value divisor)
    {
        memory_write_archive bytecode{false};
        emit_bytecode(bytecode, source);
        emit_bytecode(bytecode, dividend);
        emit_bytecode(bytecode, source);
        emit_bytecode(bytecode, divisor);
        emit_bytecode(bytecode, operation);
        emit_bytecode(bytecode, ret);
        return run_bytecode<Value>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_operation(opcode::iconst, opcode::idiv, opcode::iret, std::int32_t{-85}, std::int32_t{2}), -42);
    EXPECT_EQ(run_operation(opcode::iconst, opcode::imod, opcode::iret, std::int32_t{-85}, std::int32_t{2}), -1);
    EXPECT_EQ(run_operation(opcode::lconst, opcode::ldiv, opcode::lret, std::int64_t{-85}, std::int64_t{2}), -42);
    EXPECT_EQ(run_operation(opcode::lconst, opcode::lmod, opcode::lret, std::int64_t{-85}, std::int64_t{2}), -1);

    memory_write_archive divide_by_zero{false};
    emit_bytecode(divide_by_zero, opcode::iconst);
    emit_bytecode(divide_by_zero, std::int32_t{42});
    emit_bytecode(divide_by_zero, opcode::iconst);
    emit_bytecode(divide_by_zero, std::int32_t{0});
    emit_bytecode(divide_by_zero, opcode::idiv);
    emit_bytecode(divide_by_zero, opcode::iret);
    auto function = jit_compiler::compile(
      {},
      divide_by_zero.get_buffer(),
      {},
      0,
      {},
      {});
    auto frame = slang::interpreter::stack_frame::with_size(
      empty_constants(),
      0,
      function.get_stack_size(),
      nullptr);
    function(&frame);
    ASSERT_NE(frame.pending_exception, nullptr);
    try
    {
        std::rethrow_exception(frame.pending_exception);
    }
    catch(const slang::interpreter::interpreter_error& error)
    {
        EXPECT_STREQ(error.what(), "Division by zero.");
    }
    catch(...)
    {
        FAIL() << "Expected interpreter_error for division by zero.";
    }
}

TEST(jit_x86_64_sysv, compile_execute_floating_point_negation)
{
    memory_write_archive f32_bytecode{false};
    emit_bytecode(f32_bytecode, opcode::fconst);
    emit_bytecode(f32_bytecode, 12.5F);
    emit_bytecode(f32_bytecode, opcode::fneg);
    emit_bytecode(f32_bytecode, opcode::fret);
    EXPECT_FLOAT_EQ(run_bytecode<float>(f32_bytecode.get_buffer()), -12.5F);

    memory_write_archive f64_bytecode{false};
    emit_bytecode(f64_bytecode, opcode::dconst);
    emit_bytecode(f64_bytecode, -12.5);
    emit_bytecode(f64_bytecode, opcode::dneg);
    emit_bytecode(f64_bytecode, opcode::dret);
    EXPECT_DOUBLE_EQ(run_bytecode<double>(f64_bytecode.get_buffer()), 12.5);
}

TEST(jit_x86_64_sysv, compile_execute_remaining_numeric_conversions)
{
    EXPECT_EQ((run_numeric_conversion<std::int32_t, std::int32_t>(opcode::iconst, opcode::i2c, 0x180)), -128);
    EXPECT_EQ((run_numeric_conversion<std::int32_t, std::int32_t>(opcode::iconst, opcode::i2s, 0x18000)), -32768);
    EXPECT_EQ((run_numeric_conversion<std::int32_t, std::int64_t>(opcode::iconst, opcode::i2l, -42)), -42);
    EXPECT_EQ((run_numeric_conversion<std::int32_t, float>(opcode::iconst, opcode::i2f, -42)), -42.0F);
    EXPECT_EQ((run_numeric_conversion<std::int32_t, double>(opcode::iconst, opcode::i2d, -42)), -42.0);
    EXPECT_EQ((run_numeric_conversion<std::int64_t, std::int32_t>(opcode::lconst, opcode::l2i, 0x10000002aLL)), 42);
    EXPECT_EQ((run_numeric_conversion<std::int64_t, float>(opcode::lconst, opcode::l2f, 12)), 12.0F);
    EXPECT_EQ((run_numeric_conversion<std::int64_t, double>(opcode::lconst, opcode::l2d, -42)), -42.0);
    EXPECT_EQ((run_numeric_conversion<float, std::int32_t>(opcode::fconst, opcode::f2i, -12.75F)), -12);
    EXPECT_EQ((run_numeric_conversion<float, std::int64_t>(opcode::fconst, opcode::f2l, -12.75F)), -12);
    EXPECT_EQ((run_numeric_conversion<float, double>(opcode::fconst, opcode::f2d, 12.5F)), 12.5);
    EXPECT_EQ((run_numeric_conversion<double, std::int32_t>(opcode::dconst, opcode::d2i, -12.75)), -12);
    EXPECT_EQ((run_numeric_conversion<double, std::int64_t>(opcode::dconst, opcode::d2l, 12.75)), 12);
    EXPECT_EQ((run_numeric_conversion<double, float>(opcode::dconst, opcode::d2f, 12.5)), 12.5F);
}

TEST(jit_x86_64_sysv, compile_execute_integer_and_float_comparisons)
{
    const auto run_compare = []<typename T>(
                               opcode source,
                               opcode compare,
                               T lhs,
                               T rhs)
    {
        memory_write_archive bytecode{false};
        emit_bytecode(bytecode, source);
        emit_bytecode(bytecode, lhs);
        emit_bytecode(bytecode, source);
        emit_bytecode(bytecode, rhs);
        emit_bytecode(bytecode, compare);
        emit_bytecode(bytecode, opcode::iret);
        return run_bytecode<std::int32_t>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_compare(opcode::iconst, opcode::icmpl, 1, 2), 1);
    EXPECT_EQ(run_compare(opcode::iconst, opcode::icmple, 2, 2), 1);
    EXPECT_EQ(run_compare(opcode::iconst, opcode::icmpg, 3, 2), 1);
    EXPECT_EQ(run_compare(opcode::iconst, opcode::icmpge, 2, 2), 1);
    EXPECT_EQ(run_compare(opcode::iconst, opcode::icmpeq, 2, 2), 1);
    EXPECT_EQ(run_compare(opcode::iconst, opcode::icmpne, 2, 3), 1);

    EXPECT_EQ(run_compare(opcode::lconst, opcode::lcmpl, std::int64_t{-3}, std::int64_t{-2}), 1);
    EXPECT_EQ(run_compare(opcode::lconst, opcode::lcmple, std::int64_t{2}, std::int64_t{2}), 1);
    EXPECT_EQ(run_compare(opcode::lconst, opcode::lcmpg, std::int64_t{3}, std::int64_t{2}), 1);
    EXPECT_EQ(run_compare(opcode::lconst, opcode::lcmpge, std::int64_t{2}, std::int64_t{2}), 1);
    EXPECT_EQ(run_compare(opcode::lconst, opcode::lcmpeq, std::int64_t{2}, std::int64_t{2}), 1);
    EXPECT_EQ(run_compare(opcode::lconst, opcode::lcmpne, std::int64_t{2}, std::int64_t{3}), 1);

    EXPECT_EQ(run_compare(opcode::fconst, opcode::fcmpl, 1.0F, 2.0F), 1);
    EXPECT_EQ(run_compare(opcode::fconst, opcode::fcmple, 2.0F, 2.0F), 1);
    EXPECT_EQ(run_compare(opcode::fconst, opcode::fcmpg, 3.0F, 2.0F), 1);
    EXPECT_EQ(run_compare(opcode::fconst, opcode::fcmpge, 2.0F, 2.0F), 1);
    EXPECT_EQ(run_compare(opcode::fconst, opcode::fcmpeq, 2.0F, 2.0F), 1);
    EXPECT_EQ(run_compare(opcode::fconst, opcode::fcmpne, 2.0F, 3.0F), 1);

    EXPECT_EQ(run_compare(opcode::dconst, opcode::dcmpl, 1.0, 2.0), 1);
    EXPECT_EQ(run_compare(opcode::dconst, opcode::dcmple, 2.0, 2.0), 1);
    EXPECT_EQ(run_compare(opcode::dconst, opcode::dcmpg, 3.0, 2.0), 1);
    EXPECT_EQ(run_compare(opcode::dconst, opcode::dcmpge, 2.0, 2.0), 1);
    EXPECT_EQ(run_compare(opcode::dconst, opcode::dcmpeq, 2.0, 2.0), 1);
    EXPECT_EQ(run_compare(opcode::dconst, opcode::dcmpne, 2.0, 3.0), 1);
}

TEST(jit_x86_64_sysv, compile_execute_conditional_branch)
{
    const auto run_branch = [](std::int32_t condition)
    {
        memory_write_archive bytecode{false};
        emit_bytecode(bytecode, opcode::iconst);
        emit_bytecode(bytecode, condition);
        emit_bytecode(bytecode, opcode::jnz);
        emit_bytecode(bytecode, slang::vle_int{1});
        emit_bytecode(bytecode, slang::vle_int{2});
        emit_bytecode(bytecode, opcode::label);
        emit_bytecode(bytecode, slang::vle_int{1});
        emit_bytecode(bytecode, opcode::iconst);
        emit_bytecode(bytecode, std::int32_t{42});
        emit_bytecode(bytecode, opcode::jmp);
        emit_bytecode(bytecode, slang::vle_int{3});
        emit_bytecode(bytecode, opcode::label);
        emit_bytecode(bytecode, slang::vle_int{2});
        emit_bytecode(bytecode, opcode::iconst);
        emit_bytecode(bytecode, std::int32_t{-13});
        emit_bytecode(bytecode, opcode::label);
        emit_bytecode(bytecode, slang::vle_int{3});
        emit_bytecode(bytecode, opcode::iret);
        return run_bytecode<std::int32_t>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_branch(1), 42);
    EXPECT_EQ(run_branch(0), -13);
}

TEST(jit_x86_64_sysv, compile_execute_array_store_and_load)
{
    slang::gc::garbage_collector collector;
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{3});
    emit_bytecode(bytecode, opcode::newarray);
    emit_bytecode(bytecode, slang::module_::array_type::i32);
    emit_bytecode(bytecode, opcode::adup);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{1});
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{42});
    emit_bytecode(bytecode, opcode::iastore);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{1});
    emit_bytecode(bytecode, opcode::iaload);
    emit_bytecode(bytecode, opcode::iret);

    EXPECT_EQ(run_bytecode<std::int32_t>(bytecode.get_buffer(), {}, 0, &collector), 42);
}

TEST(jit_x86_64_sysv, compile_execute_narrow_array_load)
{
    slang::gc::garbage_collector collector;
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{1});
    emit_bytecode(bytecode, opcode::newarray);
    emit_bytecode(bytecode, slang::module_::array_type::i8);
    emit_bytecode(bytecode, opcode::adup);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{0});
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{-1});
    emit_bytecode(bytecode, opcode::castore);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{0});
    emit_bytecode(bytecode, opcode::caload);
    emit_bytecode(bytecode, opcode::iret);

    EXPECT_EQ(run_bytecode<std::int32_t>(bytecode.get_buffer(), {}, 0, &collector), -1);
}

TEST(jit_x86_64_sysv, compile_execute_extended_duplication)
{
    const auto run_duplication = [](opcode operation, std::vector<std::int32_t> values)
    {
        memory_write_archive bytecode{false};
        for(const auto value: values)
        {
            emit_bytecode(bytecode, opcode::iconst);
            emit_bytecode(bytecode, value);
        }
        emit_bytecode(bytecode, operation);
        emit_bytecode(bytecode, slang::type_class::cat1);
        emit_bytecode(bytecode, slang::type_class::cat1);
        if(operation == opcode::dup_x2)
        {
            emit_bytecode(bytecode, slang::type_class::cat1);
        }
        emit_bytecode(bytecode, opcode::iadd);
        emit_bytecode(bytecode, opcode::iadd);
        if(operation == opcode::dup_x2)
        {
            emit_bytecode(bytecode, opcode::iadd);
        }
        emit_bytecode(bytecode, opcode::iret);
        return run_bytecode<std::int32_t>(bytecode.get_buffer());
    };

    EXPECT_EQ(run_duplication(opcode::dup_x1, {10, 20}), 50);
    EXPECT_EQ(run_duplication(opcode::dup_x2, {10, 20, 30}), 90);

    memory_write_archive dup2_x0{false};
    emit_bytecode(dup2_x0, opcode::iconst);
    emit_bytecode(dup2_x0, std::int32_t{10});
    emit_bytecode(dup2_x0, opcode::iconst);
    emit_bytecode(dup2_x0, std::int32_t{20});
    emit_bytecode(dup2_x0, opcode::dup2_x0);
    emit_bytecode(dup2_x0, slang::type_class::cat1);
    emit_bytecode(dup2_x0, slang::type_class::cat1);
    emit_bytecode(dup2_x0, opcode::iadd);
    emit_bytecode(dup2_x0, opcode::iadd);
    emit_bytecode(dup2_x0, opcode::iadd);
    emit_bytecode(dup2_x0, opcode::iret);
    EXPECT_EQ(run_bytecode<std::int32_t>(dup2_x0.get_buffer()), 60);
}

TEST(jit_x86_64_sysv, compile_execute_struct_allocation_and_fields)
{
    slang::gc::garbage_collector collector;
    const auto layout_id = collector.register_type_layout("x86_test_struct", {});
    slang::module_::struct_descriptor type;
    type.size = sizeof(std::int32_t);
    type.alignment = alignof(std::int32_t);
    type.layout_id = layout_id;
    type.member_types.emplace_back("value", slang::module_::field_descriptor{"i32", false});
    type.member_types.back().second.size = sizeof(std::int32_t);
    type.member_types.back().second.alignment = alignof(std::int32_t);
    const auto resolve_type = [&type](std::int64_t index)
    {
        if(index != 0)
        {
            throw sj::jit_error{"Unexpected x86 test type index."};
        }
        return type;
    };

    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::new_);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::adup);
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{42});
    emit_bytecode(bytecode, opcode::setfield);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::adup);
    emit_bytecode(bytecode, opcode::checkcast);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::apop);
    emit_bytecode(bytecode, opcode::getfield);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::iret);

    auto function = jit_compiler::compile(
      {},
      bytecode.get_buffer(),
      {},
      0,
      {},
      {},
      resolve_type);
    auto frame = slang::interpreter::stack_frame::with_size(
      empty_constants(),
      function.get_locals_size(),
      function.get_stack_size(),
      &collector);
    function(&frame);
    ASSERT_EQ(frame.pending_exception, nullptr);
    std::int32_t result{0};
    std::memcpy(&result, frame.stack.span().data(), sizeof(result));
    EXPECT_EQ(result, 42);

    memory_write_archive array_bytecode{false};
    emit_bytecode(array_bytecode, opcode::iconst);
    emit_bytecode(array_bytecode, std::int32_t{2});
    emit_bytecode(array_bytecode, opcode::anewarray);
    emit_bytecode(array_bytecode, slang::vle_int{0});
    emit_bytecode(array_bytecode, opcode::arraylength);
    emit_bytecode(array_bytecode, opcode::iret);
    auto array_function = jit_compiler::compile(
      {},
      array_bytecode.get_buffer(),
      {},
      0,
      {},
      {},
      resolve_type);
    auto array_frame = slang::interpreter::stack_frame::with_size(
      empty_constants(),
      array_function.get_locals_size(),
      array_function.get_stack_size(),
      &collector);
    array_function(&array_frame);
    ASSERT_EQ(array_frame.pending_exception, nullptr);
    std::memcpy(&result, array_frame.stack.span().data(), sizeof(result));
    EXPECT_EQ(result, 2);
}

TEST(jit_x86_64_sysv, compile_execute_native_call)
{
    auto target = std::make_unique<sj::jit_call_target>();
    target->function = sj::native_function_type{
      [](slang::interpreter::operand_stack& stack)
      {
          const auto argument = stack.pop_cat1<std::int32_t>();
          stack.push_cat1<std::int32_t>(argument * 2);
      }};
    target->param_sizes = {sizeof(std::int32_t)};
    target->argument_size = stack_slot_size;
    target->return_type_kind = slang::codegen::type_kind::i32;
    target->return_size = stack_slot_size;
    std::vector<std::unique_ptr<sj::jit_call_target>> targets;
    targets.push_back(std::move(target));

    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::iconst);
    emit_bytecode(bytecode, std::int32_t{21});
    emit_bytecode(bytecode, opcode::invoke);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::iret);
    auto function = jit_compiler::compile(
      {},
      bytecode.get_buffer(),
      {},
      0,
      targets,
      {});
    auto frame = slang::interpreter::stack_frame::with_size(
      empty_constants(),
      function.get_locals_size(),
      function.get_stack_size(),
      nullptr);
    function(&frame);
    ASSERT_EQ(frame.pending_exception, nullptr);
    std::int32_t result{0};
    std::memcpy(&result, frame.stack.span().data(), sizeof(result));
    EXPECT_EQ(result, 42);
}

TEST(jit_x86_64_sysv, compile_execute_string_constant)
{
    slang::gc::garbage_collector collector;
    const std::vector<slang::module_::constant_table_entry> constants{
      {slang::module_::constant_type::str, std::string{"x86 jit"}}};
    memory_write_archive bytecode{false};
    emit_bytecode(bytecode, opcode::sconst);
    emit_bytecode(bytecode, slang::vle_int{0});
    emit_bytecode(bytecode, opcode::aret);
    const auto* string = run_bytecode<std::string*>(
      bytecode.get_buffer(),
      {},
      0,
      &collector,
      constants);
    ASSERT_NE(string, nullptr);
    EXPECT_EQ(*string, "x86 jit");
}

TEST(jit_x86_64_sysv, module_loader)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");
    file_mgr.add_search_path("test");

    slang::interpreter::context context{file_mgr};
    module_loader module{
      file_mgr,
      "test_jit",
      std::format("test_jit.{}", slang::package::module_ext),
      &context};
    const auto register_i32_add = [&module](const std::string& name)
    {
        module.register_native_function(
          "test",
          name,
          [](slang::interpreter::operand_stack& stack)
          {
              const auto rhs = stack.pop_cat1<std::int32_t>();
              const auto lhs = stack.pop_cat1<std::int32_t>();
              stack.push_cat1(lhs + rhs);
          });
    };
    register_i32_add("native_add_i8");
    register_i32_add("native_add_i16");
    register_i32_add("native_add_i32");
    module.register_native_function(
      "test",
      "native_add_i64",
      [](slang::interpreter::operand_stack& stack)
      {
          const auto rhs = stack.pop_cat2<std::int64_t>();
          const auto lhs = stack.pop_cat2<std::int64_t>();
          stack.push_cat2(lhs + rhs);
      });
    module.register_native_function(
      "test",
      "native_add_f32",
      [](slang::interpreter::operand_stack& stack)
      {
          const auto rhs = stack.pop_cat1<float>();
          const auto lhs = stack.pop_cat1<float>();
          stack.push_cat1(lhs + rhs);
      });
    module.register_native_function(
      "test",
      "native_add_f64",
      [](slang::interpreter::operand_stack& stack)
      {
          const auto rhs = stack.pop_cat2<double>();
          const auto lhs = stack.pop_cat2<double>();
          stack.push_cat2(lhs + rhs);
      });
    std::size_t native_void_calls{0};
    module.register_native_function(
      "test",
      "native_noop",
      [&native_void_calls]([[maybe_unused]] slang::interpreter::operand_stack& stack)
      { ++native_void_calls; });

    const auto run_i32 = [&module](const std::string& name)
    {
        auto& function = module.get_function(name);
        auto frame = slang::interpreter::stack_frame::with_size(
          module.get_constant_table(),
          function.get_locals_size(),
          function.get_stack_size(),
          nullptr);
        function(&frame);
        EXPECT_EQ(frame.pending_exception, nullptr);
        std::int32_t result{0};
        std::memcpy(&result, frame.stack.span().data(), sizeof(result));
        return result;
    };
    const auto run_value = [&module]<typename T>(const std::string& name)
    {
        auto& function = module.get_function(name);
        auto frame = slang::interpreter::stack_frame::with_size(
          module.get_constant_table(),
          function.get_locals_size(),
          function.get_stack_size(),
          nullptr);
        function(&frame);
        EXPECT_EQ(frame.pending_exception, nullptr);
        T result{};
        std::memcpy(&result, frame.stack.span().data(), sizeof(result));
        return result;
    };

    EXPECT_EQ(run_i32("test_ret"), 12);
    EXPECT_EQ(run_i32("test_call"), 12);
    EXPECT_EQ(run_i32("test_call_args"), 42);
    EXPECT_EQ(run_i32("test_imported_call_args"), 42);
    EXPECT_EQ(run_i32("test_native_call_i8"), -128);
    EXPECT_EQ(run_i32("test_native_call_i16"), -32768);
    EXPECT_EQ(run_i32("test_native_call_i32"), 42);
    EXPECT_EQ(run_value.template operator()<std::int64_t>("test_native_call_i64"), 4);
    EXPECT_FLOAT_EQ(run_value.template operator()<float>("test_native_call_f32"), 42.0F);
    EXPECT_DOUBLE_EQ(run_value.template operator()<double>("test_native_call_f64"), -4.0);
    auto& void_function = module.get_function("test_native_void");
    auto void_frame = slang::interpreter::stack_frame::with_size(
      module.get_constant_table(),
      void_function.get_locals_size(),
      void_function.get_stack_size(),
      nullptr);
    void_function(&void_frame);
    EXPECT_EQ(void_frame.pending_exception, nullptr);
    EXPECT_EQ(native_void_calls, 1);
}

}    // namespace
