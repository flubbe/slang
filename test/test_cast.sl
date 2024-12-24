import std;

fn main(args: [str]) -> i32 {
    let r: std::result = std::result{
        ok: 1, 
        value: std::i32s{value: 123} as std::type
    };

    (r.value as std::i32s).value = 321;

    return 0;
}
