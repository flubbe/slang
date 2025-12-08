import std;

const ABS: f32 = 1e-6 as f32;

fn approx_eq(value: f32, expected: f32, message: str) -> void
{
    std::assert(std::abs(value - expected) < ABS, message);
}

macro p! {
    () => {
        127i8;
    };
}

fn test_i8() -> void {
    let x: i8 = 3 as i8;
    let y: i8 = (-123) as i8;
    let z: i8 = (x as i32 + y as i32) as i8;

    std::assert(z == (-120) as i8, "z == -120");

    let a: i8 = (100i8 + 100i8 + 100i8);
    std::println(
        std::format!("a = {}", a));
    std::assert(a == 44i8, "a == 44");

    let b: i8 = p!() + p!();
    std::println(
        std::format!("b = {}", b));
    std::assert(b == -2i8, "b == -2");

    b -= 100i8;
    b -= 90i8;
    std::println(
        std::format!("b = {}", b));
    std::assert(b == 64i8, "b == 64");
}

fn test_i64() -> void {
    let x: i64 = 3 as i64;
    let y: i64 = x << 2;
    std::assert(y == 12 as i64, "y == 12");
}

fn test_literal_suffixes() -> void {
    let a: i8 = 13i8;
    let b: i16 = -127i16;
    let c: i32 = 8392839i32;
    let d: i64 = 238923893928329839i64;

    let f: f32 = -123.3213231f32;
    let g: f64 = 2389819.23928f64;

    std::assert(a as i32 == 13, "a == 13");
    std::assert(b as i32 == -127, "b == -127");
    std::assert(c == 8392839, "c == 8392839");
    std::assert(d == 238923893928329839i64, "d == 238923893928329839i64");

    approx_eq(f, -123.3213231f32, "f == -123.3213231f32");
    approx_eq(g as f32, 2389819.23928f64 as f32, "g == 2389819.23928f64");

    let r0: i32 = a == 13i8;
    std::assert(r0 == 1, "r0 == 1");

    let r1: i32 = b != 123i16;
    std::assert(r1 == 1, "r1 == 1");

    let r2: i32 = c < 0;
    std::assert(r2 == 0, "r2 == 0");

    let r3: i32 = d > 0i64;
    std::assert(r3 == 1, "r3 == 1");

    let r4: i32 = f != 12.3f32;
    std::assert(r4 == 1, "r4 == 1");

    let r5: i32 = g < 0.f64;
    std::assert(r5 == 0, "r5 == 0");
}

fn test_builtin_arrays() -> void {
    let arr_i8: [i8] = new i8[2];
    std::assert(arr_i8.length == 2, "arr_i8.length == 2");

    arr_i8[0] = 1i8;
    arr_i8[1] = 2i8;
    std::assert(arr_i8[0] == 1i8, "arr_i8[0] == 1i8");
    std::assert(arr_i8[1] == 2i8, "arr_i8[1] == 2i8");

    let arr_i16: [i16] = new i16[3];
    std::assert(arr_i16.length == 3, "arr_i16.length == 3");

    arr_i16[0] = (-1) as i16;
    arr_i16[1] = 123 as i16;
    arr_i16[2] = 3 as i16;
    std::assert(arr_i16[0] == (-1) as i16, "arr_i16[0] == (-1) as i16");
    std::assert(arr_i16[1] == 123 as i16, "arr_i16[0] == 123 as i16");
    std::assert(arr_i16[2] == 3 as i16, "arr_i16[0] == 3 as i16");

    let arr_i32: [i32] = new i32[1];
    std::assert(arr_i32.length == 1, "arr_i32.length == 1");

    arr_i32[0] = 761;
    std::assert(arr_i32[0] == 761, "arr_i32[0] == 761");

    let arr_i64: [i64] = new i64[5];
    std::assert(arr_i64.length == 5, "arr_i64.length == 1");

    arr_i64[0] = 762i64;
    arr_i64[1] = 763i64;
    arr_i64[2] = -764i64;
    arr_i64[3] = -762i64;
    std::assert(arr_i64[0] == 762i64, "arr_i64[0] == 762i64");
    std::assert(arr_i64[1] == 763i64, "arr_i64[1] == 763i64");
    std::assert(arr_i64[2] == -764i64, "arr_i64[2] == -764i64");
    std::assert(arr_i64[3] == -762i64, "arr_i64[3] == -762i64");

    let arr_f32: [f32] = new f32[2];
    std::assert(arr_f32.length == 2, "arr_f32.length == 2");

    arr_f32[0] = 1.23f32;
    arr_f32[1] = -1 as f32;
    std::assert(arr_f32[0] == 1.23f32, "arr_f32[0] == 1.23f32");
    std::assert(arr_f32[1] == -1 as f32, "arr_f32[1] == -1 as f32");

    let arr_f64: [f64] = new f64[3];
    std::assert(arr_f64.length == 3, "arr_f64.length == 3");

    arr_f64[0] = 3.23f64;
    arr_f64[1] = -2 as f64;
    arr_f64[2] = -2.3 as f64;
    std::assert(arr_f64[0] == 3.23f64, "arr_f64[0] == 3.23f64");
    std::assert(arr_f64[1] == -2 as f64, "arr_f64[1] == -2 as f64");
    std::assert(arr_f64[2] == -2.3 as f64, "arr_f64[2] == -2.3 as f64");
}

struct S {
    c: i8,
    s: i16,
    i: i32,
    l: i64,
    f: f32,
    d: f64
};

fn test_struct() -> void {
    let s: S = S{
        c: 1i8,
        s: 2i16,
        i: 3i32,
        l: 4i64,
        f: 5.1f32,
        d: 6.2f64 
    };

    std::print(
        std::format!(
            "{} {} {} {} {} {}\n", 
            s.c as i32, s.s as i32, s.i, s.l, s.f, s.d));

    std::assert(s.c == 1i8, "s.c == 1i8");
    std::assert(s.s == 2i16, "s.s == 2i16");
    std::assert(s.i == 3i32, "s.i == 3i32");
    std::assert(s.l == 4i64, "s.l == 4i64");

    std::assert(s.f == 5.1f32, "s.f == 5.1f32");
    std::assert(s.d == 6.2f64, "s.d == 6.2f64");

    s.i = -1;
    std::assert(s.s == 2i16, "s.s == 2i16");
    std::assert(s.i == -1, "s.i == -1");
    std::assert(s.l == 4i64, "s.l == 4i64");

    s.l = -3 as i64;
    s.f = -1.2f32;
    s.d = 3.4f64;

    std::assert(s.i == -1, "s.i == -1");
    std::assert(s.l == -3 as i64, "s.l == -3 as i64");
    std::assert(s.f == -1.2f32, "s.f == -1.2f32");
    std::assert(s.d == 3.4f64, "s.d == 3.4f64");
}

fn test_unary_operators() -> void {
    let i0: i8 = 1i8;
    let i1: i8 = +i0;
    i1 = -i0;
    i1 = ~i0;

    let s1: str = std::format!("{} {} {} {}", i0, +i0, -i0, ~i0);
    std::assert(
        std::string_equals(
            s1,
            "1 1 -1 -2"
        ),
        "s1 == 1 1 -1 -2"
    );

    let i2: i16 = -1i16;
    let i3: i16 = +i2;
    i3 = -i2;
    i3 = ~i2;

    let s2: str = std::format!("{} {} {} {}", i2, +i2, -i2, ~i2);
    std::assert(
        std::string_equals(
            s2,
            "-1 -1 1 0"
        ),
        "s1 == -1 -1 1 0"
    );
}

fn main(args: [str]) -> i32 {
    let a: i8 = 3 as i8;

    test_i8();
    test_i64();

    test_literal_suffixes();

    test_builtin_arrays();
    test_struct();

    test_unary_operators();

    return 0;
}
