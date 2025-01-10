import std;

fn main(args: [str]) -> i32 {
    let i32s_arr: [std::i32s] = new std::i32s[2];
    i32s_arr[0] = std::result{ok: 1, value: null}; // should fail
    return 0;
}
