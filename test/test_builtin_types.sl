import std;

const ABS: f32 = 1e-6 as f32;

fn approx_eq(value: f32, expected: f32, message: str) -> void
{
    std::assert(std::abs(value - expected) < ABS, message);
}

fn test_i8() -> void {
    let x: i8 = 3 as i8;
    let y: i8 = (-123) as i8;
    let z: i8 = (x as i32 + y as i32) as i8;

    std::assert(z == (-120) as i8, "z == -120");
}

fn test_i64() -> void {
    let x: i64 = 3 as i64;
    let y: i64 = x << 2;
    std::assert(y == 12 as i64, "y == 12");
}

fn test_literal_suffixes() -> void {
    let a: i8 = 13i8;
    let b: i16 = (-(127i16 as i32)) as i16;
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
}

fn main(args: [str]) -> i32 {
    let a: i8 = 3 as i8;

    test_i8();
    test_i64();

    test_literal_suffixes();

    return 0;
}
