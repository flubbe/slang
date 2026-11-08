/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for x86_64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "jit/common.h"
#include "jit/x86_64/emitter.h"

namespace slang::jit::abi::sysv_x86_64
{

using x86_64::condition_code;
using x86_64::cpu_registers;
using x86_64::instruction_emitter;
using x86_64::simd_registers;

/** x86_64 bytecode compiler. */
class jit_compiler
{
    /**
     * Create a JIT-compiled function from machine code.
     *
     * @param signature The function signature.
     * @param machine_code The machine code to execute.
     * @param locals_size Size in bytes of the function's local-variable storage.
     * @param stack_size Size in bytes of the function's required operand stack.
     * @returns A JIT-compiled function owning the executable memory.
     */
    static jit_function create_jit_function(
      module_::function_signature signature,
      const std::vector<std::uint8_t>& machine_code,
      std::size_t locals_size,
      std::size_t stack_size);

public:
    /**
     * Compile a function.
     *
     * @param signature The function signature.
     * @param bytecode The function's bytecode.
     * @param local_offsets Byte offsets of local variables within the function's
     *     local-variable storage, indexed by local-variable index.
     * @param locals_size Size in bytes of the function's local-variable storage.
     * @param call_targets Module-local function call targets, indexed by function index.
     * @param import_call_targets Imported function call targets, indexed by import index.
     * @returns The compiled native function.
     * @throws Throws `jit_error` if compilation fails due to invalid bytecode, unsupported
     *    instructions, invalid indices, or incompatible stack requirements.
     */
    static jit_function compile(
      module_::function_signature signature,
      std::span<const std::byte> bytecode,
      const std::vector<std::size_t>& local_offsets,
      std::size_t locals_size,
      const std::vector<std::unique_ptr<jit_call_target>>& call_targets,
      const std::vector<jit_call_target*>& import_call_targets,
      const std::function<module_::struct_descriptor(std::int64_t)>& resolve_type = {});
};

}    // namespace slang::jit::abi::sysv_x86_64

#include "jit/cfg.h"
