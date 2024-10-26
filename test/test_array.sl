import std;

fn main(args: [str]) -> i32 {
    let strs: [str] = [
        "This", "is", "a", "loop!"
    ];

    std::assert(strs.length == 4, "strs.length == 4");

    let i: i32 = 0;
    while(i < strs.length) {
        std::println(strs[i]);
        i++;
    }

    std::assert(i == strs.length, "i == strs.length");

    return 0;
}