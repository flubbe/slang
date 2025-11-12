import std;

fn arithmetic_i8() -> void {
    let x: i8 = 3 as i8;
    let y: i8 = (-123) as i8;
    let z: i8 = (x as i32 + y as i32) as i8;

    std::assert(z == (-120) as i8, "z == -120");
}

fn main(args: [str]) -> i32 {
    let a: i8 = 3 as i8;

    return 0;
}
