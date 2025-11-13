import std;

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

fn main(args: [str]) -> i32 {
    let a: i8 = 3 as i8;

    test_i8();
    test_i64();

    return 0;
}
