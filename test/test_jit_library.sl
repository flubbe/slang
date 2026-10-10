fn add_i32(a: i32, b: i32) -> i32 {
    return a + b;
}

#[native(lib = "test")]
fn native_add_i32(a: i32, b: i32) -> i32;
