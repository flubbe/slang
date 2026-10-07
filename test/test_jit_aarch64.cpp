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

template<typename T>
using constant_serializer = slang::constant_serializer<T>;
using slang::memory_write_archive;
using slang::opcode;

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
