/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <string>
#include <vector>
#include <functional>

#include "aarch64/registers.h"
#include "common.h"

/*
 * Forward declarations.
 */
namespace slang
{
class file_manager;
}    // namespace slang

namespace slang::jit::aarch64
{

/**
 * AArch64 instruction emitter.
 *
 * References
 * ----------
 * 1. Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile,
 *    https://support.arm.com/documentation/ddi0487/latest/
 * 2. Procedure Call Standard for the Arm(R) 64-bit Architecture (AArch64),
 *    https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst
 */
struct instruction_emitter
{
    std::vector<std::uint32_t> code{};

    /*
     * Helpers.
     */

    /** Emit a 32-bit instruction. */
    void emit(
      std::uint32_t insn);

    /** Emit an LDP or STP instruction for X registers. */
    void emit_ldp_stp_x(
      bool is_load,
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t byte_offset,
      bool pre_index);

    /*
     * AArch64 instruction mappings.
     */

    /** Add (shifted register). Emits `ADD <Wd>, <Wn>, <Wm>`. */
    void add_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Add (shifted register). Emits `ADD <Xd>, <Xn>, <Xm>`. */
    void add_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Bitwise AND (shifted register). Emits `AND <Wd>, <Wn>, <Wm>`. */
    void and_reg_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Bitwise AND (shifted register). Emits `AND <Xd>, <Xn>, <Xm>`. */
    void and_reg_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /**
     * Branch.
     *
     * Emits `B #<imm26>`, where the signed 26-bit immediate specifies
     * a PC-relative byte offset divided by four.
     *
     * @param byte_offset Signed byte offset from the address of this
     *     instruction to the branch target. Must be 4-byte aligned and in
     *     the range `[-2^27, 2^27)`.
     */
    void b(
      std::int32_t byte_offset);

    /** Branch with link to register. Emits `BLR <Xn>`. */
    void blr(
      cpu_registers xn);

    /**
     * Compare and branch if nonzero (32-bit).
     *
     * Emits `CBNZ <Wt>, #<imm19>`, where the signed 19-bit immediate
     * specifies a PC-relative byte offset divided by four.
     *
     * @param wt The W register to test.
     * @param byte_offset Signed byte offset from the address of this
     *     instruction to the branch target. Must be 4-byte aligned and in
     *     the range `[-2^20, 2^20)`.
     */
    void cbnz_w(
      cpu_registers wt,
      std::int32_t byte_offset);

    /** Bitwise exclusive-OR (shifted register). Emits `EOR <Wd>, <Wn>, <Wm>`. */
    void eor_reg_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Bitwise exclusive-OR (shifted register). Emits `EOR <Xd>, <Xn>, <Xm>`. */
    void eor_reg_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Load pair of registers (post-indexed). Emits `LDP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>`. */
    void ldp_x_post(
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t offset);

    /** Load register (immediate). Emits `LDR <Wt>, [<Xn|SP>{, #<pimm>}]`. */
    void ldr_w(
      cpu_registers wd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Load register (immediate). Emits `LDR <Xt>, [<Xn|SP>{, #<pimm>}]`. */
    void ldr_x(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Logical shift left variable. Emits `LSLV <Wd>, <Wn>, <Wm>`. */
    void lslv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Logical shift left variable. Emits `LSLV <Xd>, <Xn>, <Xm>`. */
    void lslv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Logical shift right variable. Emits `LSRV <Wd>, <Wn>, <Wm>`. */
    void lsrv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Logical shift right variable. Emits `LSRV <Xd>, <Xn>, <Xm>`. */
    void lsrv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Move wide with keep. Emits `MOVK <Wd>, #<imm>{, LSL #<shift>}`. */
    void movk_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with keep. Emits `MOVK <Xd>, #<imm>{, LSL #<shift>}`. */
    void movk_x(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with NOT. Emits `MOVN <Wd>, #<imm>{, LSL #<shift>}`. */
    void movn_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with zero. Emits `MOVZ <Wd>, #<imm>{, LSL #<shift>}`. */
    void movz_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with zero. Emits `MOVZ <Xd>, #<imm>{, LSL #<shift>}`. */
    void movz_x(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Multiply-subtract. Emits `MSUB <Wd>, <Wn>, <Wm>, <Wa>`. */
    void msub_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm,
      cpu_registers wa);

    /** Multiply-subtract. Emits `MSUB <Xd>, <Xn>, <Xm>, <Xa>`. */
    void msub_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm,
      cpu_registers xa);

    /** Multiply. Emits `MUL <Wd>, <Wn>, <Wm>`. */
    void mul_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Multiply. Emits `MUL <Xd>, <Xn>, <Xm>`. */
    void mul_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Bitwise OR (shifted register). Emits `ORR <Wd>, <Wn>, <Wm>`. */
    void orr_reg_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Bitwise OR (shifted register). Emits `ORR <Xd>, <Xn>, <Xm>`. */
    void orr_reg_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Return from subroutine. Emits `RET`. */
    void ret();

    /** Signed divide. Emits `SDIV <Wd>, <Wn>, <Wm>`. */
    void sdiv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Signed divide. Emits `SDIV <Xd>, <Xn>, <Xm>`. */
    void sdiv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Store pair of registers (pre-indexed). Emits `STP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>]!`. */
    void stp_x_pre(
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t offset);

    /** Store register (immediate). Emits `STR <Wt>, [<Xn|SP>{, #<pimm>}]`. */
    void str_w(
      cpu_registers wd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Store register (immediate). Emits `STR <Xt>, [<Xn|SP>{, #<pimm>}]`. */
    void str_x(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Subtract (shifted register). Emits `SUB <Wd>, <Wn>, <Wm>`. */
    void sub_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Subtract (shifted register). Emits `SUB <Xd>, <Xn>, <Xm>`. */
    void sub_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /*
     * Convenience operations.
     */

    /**
     * Load a 64-bit value from a base register plus a byte offset.
     *
     * Uses an immediate-offset load when the offset is encodable;
     * otherwise, computes the effective address in a temporary register.
     */
    void load_x(
      cpu_registers reg,
      std::uint32_t offset);

    /**
     * Store a 64-bit value to a base register plus a byte offset.
     *
     * Uses an immediate-offset store when the offset is encodable;
     * otherwise, computes the effective address in a temporary register.
     */
    void store_x(
      cpu_registers reg,
      std::uint32_t offset);

    /** Move register. Emits `MOV <Xd>, <Xm>`. */
    void mov_reg_x(
      cpu_registers xd,
      cpu_registers xm);

    /** Move a 32-bit immediate value into a W register. */
    void mov_w(
      cpu_registers wd,
      std::int32_t val);

    /** Move a 64-bit immediate value into an X register. */
    void mov_x(
      cpu_registers xd,
      std::int64_t val);

    /** Store pair of registers (pre-indexed). Emits `STP X29, X30, [SP, #-16]!`. */
    void push_fp_lr();

    /** Load pair of registers (post-indexed). Emits `LDP X29, X30, [SP], #16`. */
    void pop_fp_lr();

    /*
     * Patching.
     */

    /** Patch the target of a `B` instruction. */
    void patch_b(
      std::size_t instruction_index,
      std::size_t target_index);

    /** Patch the target of a `CBNZ <Wt>` instruction. */
    void patch_cbnz_w(
      std::size_t instruction_index,
      std::size_t target_index);
};

/**
 * AArch64 JIT compiler.
 *
 * Reference
 * ---------
 * Procedure Call Standard for the Arm(R) 64-bit Architecture (AArch64),
 * https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst
 */
class jit_compiler
{
    /**
     * Create a JIT-compiled function from machine code.
     *
     * @param machine_code The machine code to execute.
     * @param locals_size Size in bytes of the function's local-variable storage.
     * @param stack_size Size in bytes of the function's required operand stack.
     * @returns A JIT-compiled function owning the executable memory.
     */
    static jit_function create_jit_function(
      const std::vector<std::uint32_t>& machine_code,
      std::size_t locals_size,
      std::size_t stack_size);

public:
    /**
     * Compile a function.
     *
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
      std::span<const std::byte> bytecode,
      const std::vector<std::size_t>& local_offsets,
      std::size_t locals_size,
      const std::vector<jit_call_target*>& call_targets,
      const std::vector<jit_call_target*>& import_call_targets,
      const std::function<module_::struct_descriptor(std::int64_t)>& resolve_type = {});
};

class module_loader;

/** An entry in the import table. */
struct imported_symbol
{
    /** Symbol type. */
    module_::symbol_type type;

    /** Symbol name. */
    std::string name;

    /** Index into the package import table. Unused for package imports (set to `(uint32_t)(-1)`). */
    std::uint32_t package_index;

    /** If the import is resolved, this points to the corresponding module or into the export table. Not serialized. */
    std::variant<
      const module_loader*,
      const module_::exported_symbol*>
      export_reference;
};

/** Runtime header state owned by the JIT loader. */
struct module_header
{
    /** Import table. */
    std::vector<imported_symbol> imports;

    /** Export table. */
    std::vector<module_::exported_symbol> exports;

    /** Constant table. */
    std::vector<module_::constant_table_entry> constants;
};

/** A module loader. Represents a loaded module with JITted bytecode. */
class module_loader
{
    /** Shared file manager for resolving imported modules. */
    slang::file_manager& file_mgr;

    /** Runtime context shared with this module and its imports. */
    interpreter::context* runtime_context{nullptr};

    /** The module's import name. */
    std::string import_name;

    /** The module's path. */
    fs::path path;

    /** JIT-owned runtime header. */
    module_header header;

    /** JIT-owned bytecode. */
    std::vector<std::byte> binary;

    /** Decoded types, ordered by name. */
    std::unordered_map<std::string, module_::struct_descriptor> struct_map;

    /** Compiled functions, ordered by name. */
    std::unordered_map<std::string, jit_function> function_map;

    /** Stable call targets, indexed by module export index. */
    std::vector<std::unique_ptr<jit_call_target>> call_targets;

    /** Imported modules kept alive for imported call targets. */
    std::unordered_map<std::string, std::unique_ptr<module_loader>> imported_modules;

    /** Call targets indexed by module import index. */
    std::vector<jit_call_target*> import_call_targets;

    /** Resolve and retain an imported module. */
    module_loader& resolve_module(const std::string& module_name);

    /**
     * Create stable call target records for module exports.
     *
     * Populates `call_targets` with owning pointers to function targets and
     * returns a non-owning lookup table corresponding to `header.exports`.
     * The returned table has the same size and index ordering as `header.exports`.
     * Non-function exports are represented by null pointers.
     *
     * @returns A non-owning call target table indexed by export index.
     *     The pointed-to objects are owned by `call_targets`.
     */
    std::vector<jit_call_target*> create_export_call_target_table();

    /**
     * Resolve imported symbols and populate the import-indexed call target table.
     *
     * Resolves package imports to modules and other imports to matching exports.
     * Validates imported symbol types and package references. Function imports
     * are mapped to their corresponding call targets; other imports have null
     * entries in the call target table.
     */
    void resolve_imports();

    /**
     * Compile all non-native functions in the module.
     *
     * @param call_target_table Call target table with call targets
     *     appearing in the same order as in the export table.
     */
    void compile_functions(
      const std::vector<jit_call_target*>& call_target_table);

    /** Bind matching native exports in this module and its dependencies. */
    std::size_t register_native_function_recursive(
      const std::string& library_name,
      const std::string& function_name,
      const std::function<void(interpreter::operand_stack&)>& function);

    /** Decode the structs. Set types sizes, alignments and offsets. */
    void decode_structs();

public:
    /** Defaulted and deleted constructors. */
    module_loader() = delete;
    module_loader(const module_loader&) = delete;
    module_loader(module_loader&& other) noexcept;

    /** Default assignments. */
    module_loader& operator=(const module_loader&) = delete;
    module_loader& operator=(module_loader&&) = delete;

    /**
     * Create a new module loader.
     *
     * @param file_mgr File manager.
     * @param import_name The module's import name.
     * @param path The module's path.
     */
    module_loader(
      slang::file_manager& file_mgr,
      std::string import_name,
      fs::path path,
      interpreter::context* runtime_context = nullptr);

    /** Get the module contant table. */
    const std::vector<
      module_::constant_table_entry>&
      get_constant_table() const
    {
        return header.constants;
    }

    /** Get a compiled function by its exported name. */
    jit_function& get_function(const std::string& name);

    /** Bind a native export to a host callback. */
    void register_native_function(
      const std::string& library_name,
      const std::string& function_name,
      const std::function<void(interpreter::operand_stack&)>& function);

    /** Resolve all native exports using a caller-provided runtime callback. */
    void register_native_functions(
      const std::function<std::function<void(interpreter::operand_stack&)>(
        const std::string&,
        const std::string&)>& resolver);
};

}    // namespace slang::jit::aarch64
