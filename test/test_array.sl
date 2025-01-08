import std;

fn test_builtin() -> void {
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
}

fn test_custom() -> void {
    let arr: [std::i32s] = new std::i32s[2];

    std::assert(arr.length == 2, "arr.length == 2");

    arr[0] = std::i32s{value: 123};
    arr[1] = std::i32s{value: 321};

    std::assert(arr[0].value == 123, "arr[0].value == 123");
    std::assert(arr[1].value == 321, "arr[1].value == 321");

    arr[1] = arr[0] = arr[1];

    std::assert(arr[0] == arr[1], "arr[0] == arr[1]");
    
    let d: std::i32s = std::i32s{value: 321};

    std::assert(arr[0] != d, "arr[0] != d");
    std::assert(arr[0].value == d.value, "arr[0].value == d.value");
}

fn main(args: [str]) -> i32 {
    test_builtin();
    test_custom();

    return 0;
}
