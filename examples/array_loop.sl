import std;

fn main(args: [str]) -> i32 {
    let strs: [str] = [
        "This", "is", "a", "loop!"
    ];

    let i: i32 = 0;
    while(i < strs.length) {
        std::println(strs[i]);
        i++;
    }

    return 0;
}