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

}    // namespace

// NOLINTBEGIN(readability-magic-numbers)

TEST(jit, instruction_encodings)
{
    /*
     * Compare generated machine code against
     * manually generated/assembled references.
     */

    instruction_emitter emitter;

    // ldp x19, x20, [sp], #0x10
    emitter.ldp_x_post(
      cpu_registers::X19,
      cpu_registers::X20,
      cpu_registers::SP,
      0x10);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa8c153f3});

    // stp x19, x20, [sp, #-0x10]!
    emitter.stp_x_pre(
      cpu_registers::X19,
      cpu_registers::X20,
      cpu_registers::SP,
      -0x10);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa9bf53f3});

    // mov x0, x1
    emitter.mov_reg_x(
      cpu_registers::X0,
      cpu_registers::X1);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xaa0103e0});

    // movn w0, #0x1234
    emitter.movn_w(
      cpu_registers::X0,
      0x1234);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x12824680});

    // movk x0, #0x5678, lsl #16
    emitter.movk_x(
      cpu_registers::X0,
      0x5678,
      0x16);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xf2aacf00});

    // ldr w0, [x1, #0x4]
    emitter.ldr_w(
      cpu_registers::X0,
      cpu_registers::X1,
      4);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xb9400420});

    // ldr x0, [x1, #0x8]
    emitter.ldr_x(
      cpu_registers::X0,
      cpu_registers::X1,
      8);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xf9400420});

    // add w0, w1, w2
    emitter.add_w(
      cpu_registers::X0,
      cpu_registers::X1,
      cpu_registers::X2);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x0b020020});

    // sub w0, w1, w2
    emitter.sub_w(
      cpu_registers::X0,
      cpu_registers::X1,
      cpu_registers::X2);

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0x4b020020});

    // stp	x29, x30, [sp, #-0x10]!
    emitter.push_fp_lr();

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa9bf7bfd});

    // ldp	x29, x30, [sp], #0x10
    emitter.pop_fp_lr();

    EXPECT_EQ(hex_value{emitter.code.back()}, hex_value{0xa8c17bfd});

    // ret
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
     *
     *   iconst 100
     *   istore 0
     *   iload 0
     *   iconst 42
     *   iadd
     *   iconst 10
     *   isub
     *   ret
     */

    // iconst 100
    bytecode_ar& constant_serializer{opcode::iconst};
    bytecode_ar& constant_serializer<std::int32_t>{100};    // NOLINT(readability-magic-numbers)

    // istore 0  (locals[0] = 100)
    bytecode_ar& constant_serializer{opcode::istore};
    bytecode_ar& constant_serializer<std::int64_t>{0};

    // iload 0
    bytecode_ar& constant_serializer{opcode::iload};
    bytecode_ar& constant_serializer<std::int64_t>{0};

    // iconst 42
    bytecode_ar& constant_serializer{opcode::iconst};
    bytecode_ar& constant_serializer<std::int32_t>{42};    // NOLINT(readability-magic-numbers)

    // iadd (100 + 42 = 142)
    bytecode_ar& constant_serializer{opcode::iadd};

    // iconst 10
    bytecode_ar& constant_serializer{opcode::iconst};
    bytecode_ar& constant_serializer<std::int32_t>{10};    // NOLINT(readability-magic-numbers)

    // isub (142 - 10 = 132)
    bytecode_ar& constant_serializer{opcode::isub};

    // ret
    bytecode_ar& constant_serializer{opcode::ret};

    // Compile
    std::optional<sj::jit_function> compiled_fn;
    ASSERT_NO_THROW(compiled_fn = sj::jit_compiler::compile(bytecode_ar.get_buffer()));
    ASSERT_TRUE(compiled_fn.has_value());
    ASSERT_TRUE(compiled_fn->get() != nullptr);

    // Setup stack frame
    std::optional<si::stack_frame> frame;
    ASSERT_NO_THROW(frame.emplace(sj::jit_compiler::make_stack(4, 8)));
    ASSERT_TRUE(frame.has_value());

    ASSERT_EQ(frame->locals.size(), 4);
    ASSERT_EQ(frame->stack.size(), 8);

    // Run function.
    ASSERT_NO_THROW(compiled_fn->get()(&frame.value()));

    // Verify top of stack value.
    std::int32_t result = 0;
    std::memcpy(&result, frame->stack.tail(frame->stack.size()).data(), sizeof(std::int32_t));
    EXPECT_EQ(result, 132);
}

}    // namespace
