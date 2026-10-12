fn add_i32(a: i32, b: i32) -> i32 {
    return a + b;
}

#[native(lib = "test")]
fn native_add_i8(a: i8, b: i8) -> i8;

#[native(lib = "test")]
fn native_add_i16(a: i16, b: i16) -> i16;

#[native(lib = "test")]
fn native_add_i32(a: i32, b: i32) -> i32;

#[native(lib = "test")]
fn native_add_i64(a: i64, b: i64) -> i64;

#[native(lib = "test")]
fn native_add_f32(a: f32, b: f32) -> f32;

#[native(lib = "test")]
fn native_add_f64(a: f64, b: f64) -> f64;
