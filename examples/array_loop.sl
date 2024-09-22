import std;

fn main(s: str) -> i32 {
    let strs: [str] = [
        "This", "is", "a", "loop!"
    ];

    let i: i32 = 0;
    while(i < strs.length) {
        std::println(strs[i]);
        i = i + 1;
    }

    return 0;
}