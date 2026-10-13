import std;

import test_jit_library;

fn test_ret() -> i32 {
    let i: i32 = 12;
    return i;
}

fn test_call() -> i32 {
    return test_ret();
}

fn add_i32(a: i32, b: i32) -> i32 {
    return a + b;
}

fn test_call_args() -> i32 {
    return add_i32(20, 22);
}

fn test_imported_call_args() -> i32 {
    return test_jit_library::add_i32(20, 22);
}

fn test_native_call_i8() -> i8 {
    return test_jit_library::native_add_i8(120i8, 8i8); // should result in -128.
}

fn test_native_call_i16() -> i16 {
    return test_jit_library::native_add_i16(32767i16, 1i16); // should result in -32768.
}

fn test_native_call_i32() -> i32 {
    return test_jit_library::native_add_i32(19, 23);
}

fn test_native_call_i64() -> i64 {
    return test_jit_library::native_add_i64(-19i64, 23i64);
}

fn test_native_call_f32() -> f32 {
    return test_jit_library::native_add_f32(19.f32, 23.f32);
}

fn test_native_call_f64() -> f64 {
    return test_jit_library::native_add_f64(19.f64, -23.f64);
}

#[native(lib = "test")]
fn native_noop() -> void;

fn test_native_void() -> void {
    native_noop();
}
