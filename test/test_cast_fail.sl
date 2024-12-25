import std;

fn main(args: [str]) -> i32 {
    let v: std::i32s = std::i32s{value: 1};
    (v as std::f32s).value = 2.0; // should fail.
    return 0;
}
