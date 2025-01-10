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

fn g(i: i32) -> i32 {
    return i + 1;
}

fn f(n: i32) -> [std::i32s] {
    return new std::i32s[g(n) + 2];
}

fn test_custom2() -> void {
    let arr: [std::i32s] = f(3);
    std::assert(arr.length == 6, "arr.length == 6");

    let i: i32 = 0;
    while(i<arr.length - 1)
    {
        arr[i] = std::i32s{0};
        arr[i].value = i;
        i++;
    }

    arr[arr.length - 1] = std::i32s{0};
    arr[arr.length - 1].value = 123;

    i = 0;
    while(i<arr.length - 1)
    {
        std::assert(arr[i].value == i, "arr[i].value == i");
        i++;
    }
    std::assert(arr[arr.length - 1].value == 123, "arr[arr.length - 1].value == 123");
}

struct L{
    i: i32,
    f: f32
};

fn test_local() -> void {
    let arr: [L] = new L[3];
    arr[0] = L{1, 2.0};
    arr[1] = L{2, 3.0};
    arr[2] = L{3, 4.0};

    let t: std::type = arr[1] as std::type;

    let k: f32 = (arr[0].i as f32) + arr[0].f
        + ((t as L).i as f32) + (t as L).f
        + (arr[2].i as f32) + arr[2].f;

    std::assert(std::abs(k - 15.0) < 1e-6, "k == 15.0");
}

fn main(args: [str]) -> i32 {
    test_builtin();
    test_custom();
    test_custom2();

    return 0;
}
