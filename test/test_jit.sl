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

fn test_native_call() -> i32 {
    return test_jit_library::native_add_i32(19, 23);
}

#[native(lib = "test")]
fn native_noop() -> void;

fn test_native_void() -> void {
    native_noop();
}
