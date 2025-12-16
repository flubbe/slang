import std;

struct R {
    i: i32
};

struct S {
    arr: [i32]
};

struct T {
    s: S
};

fn foo() -> i32 {
    return -1;
}

fn test_nested_evaluation0() -> void {
    std::println("nested evaluations: objects");

    {
        let obj: R = R{
            i: 2
        };

        let y: i32 = obj.i++;

        std::println(
            std::format!(
                "obj.i = {} / y = {}",
                obj.i,
                y));

        std::assert(obj.i == 3, "obj.i == 3");
        std::assert(y == 2, "y == 2");
    }
    {
        let obj: R = R{
            i: 2
        };

        let y: i32 = obj.i--;

        std::println(
            std::format!(
                "obj.i = {} / y = {}",
                obj.i,
                y));

        std::assert(obj.i == 1, "obj.i == 1");
        std::assert(y == 2, "y == 2");
    }
}

fn test_nested_evaluation1() -> void {
    std::println("nested evaluations: objects / arrays 1");

    {
        let obj: S = S{
            arr: [1, 2, 3]
        };
        let i: i32 = 1;

        obj.arr[i++] += foo();

        std::println(
            std::format!(
                "i = {} / obj.arr=[{}, {}, {}] / obj.arr.length = {},",
                i,
                obj.arr[0], obj.arr[1], obj.arr[2],
                obj.arr.length));

        std::assert(obj.arr[0] == 1, "obj.arr[0] == 1");
        std::assert(obj.arr[1] == 1, "obj.arr[1] == 1");
        std::assert(obj.arr[2] == 3, "obj.arr[2] == 3");
        std::assert(i == 2, "i == 2");
    }
    {
        let obj: S = S{
            arr: [1, 2, 3]
        };
        let i: i32 = 1;

        obj.arr[i--] -= foo();

        std::println(
            std::format!(
                "i = {} / obj.arr=[{}, {}, {}] / obj.arr.length = {},",
                i,
                obj.arr[0], obj.arr[1], obj.arr[2],
                obj.arr.length));

        std::assert(obj.arr[0] == 1, "obj.arr[0] == 1");
        std::assert(obj.arr[1] == 3, "obj.arr[1] == 3");
        std::assert(obj.arr[2] == 3, "obj.arr[2] == 3");
        std::assert(i == 0, "i == 0");
    }
}

fn test_nested_evaluation2() -> void {
    std::println("nested evaluations: objects / arrays 2");

    {
        let obj2: T = T{
            s: S{
                arr: [1, 2, 3]
            }
        };
        let i: i32 = 2;

        obj2.s.arr[i++] += foo();
        
        std::println(
            std::format!(
                "i = {} / obj2.s.arr=[{}, {}, {}] / obj2.s.arr.length = {},",
                i,
                obj2.s.arr[0], obj2.s.arr[1], obj2.s.arr[2],
                obj2.s.arr.length));

        std::assert(obj2.s.arr[0] == 1, "obj2.s.arr[0] == 1");
        std::assert(obj2.s.arr[1] == 2, "obj2.s.arr[1] == 2");
        std::assert(obj2.s.arr[2] == 2, "obj2.s.arr[2] == 2");
        std::assert(i == 3, "i == 3");
    }
    {
        let obj2: T = T{
            s: S{
                arr: [1, 2, 3]
            }
        };
        let i: i32 = 2;

        obj2.s.arr[i--] -= foo();
        
        std::println(
            std::format!(
                "i = {} / obj2.s.arr=[{}, {}, {}] / obj2.s.arr.length = {},",
                i,
                obj2.s.arr[0], obj2.s.arr[1], obj2.s.arr[2],
                obj2.s.arr.length));

        std::assert(obj2.s.arr[0] == 1, "obj2.s.arr[0] == 1");
        std::assert(obj2.s.arr[1] == 2, "obj2.s.arr[1] == 2");
        std::assert(obj2.s.arr[2] == 4, "obj2.s.arr[2] == 4");
        std::assert(i == 1, "i == 1");
    }
}

fn test_nested_evaluation3() -> void {
    std::println("nested evaluations: arrays");

    {
        let y: i32;
        let a: [i32] = [1, 2, 3];
        let i: i32 = 0;

        y = a[i]++;

        std::println(
            std::format!(
                "i = {} / y = {} / a = [{}, {}, {}]",
                i,
                y,
                a[0], a[1], a[2]));

        std::assert(a[0] == 2, "a[0] == 2");
        std::assert(a[1] == 2, "a[1] == 2");
        std::assert(a[2] == 3, "a[2] == 3");
        std::assert(y == 1, "y == 1");
        std::assert(i == 0, "i == 0");

        let z: i32;
        z = ++a[i];

        std::println(
            std::format!(
                "i = {} / z = {} / a = [{}, {}, {}]",
                i,
                z,
                a[0], a[1], a[2]));

        std::assert(a[0] == 3, "a[0] == 3");
        std::assert(a[1] == 2, "a[1] == 2");
        std::assert(a[2] == 3, "a[2] == 3");
        std::assert(z == 3, "z == 3");
        std::assert(i == 0, "i == 0");
    }
    {
        let y: i32;
        let a: [i32] = [1, 2, 3];
        let i: i32 = 0;

        y = a[i]--;

        std::println(
            std::format!(
                "i = {} / y = {} / a = [{}, {}, {}]",
                i,
                y,
                a[0], a[1], a[2]));

        std::assert(a[0] == 0, "a[0] == 0");
        std::assert(a[1] == 2, "a[1] == 2");
        std::assert(a[2] == 3, "a[2] == 3");
        std::assert(y == 1, "y == 1");
        std::assert(i == 0, "i == 0");

        let z: i32;
        z = --a[i];

        std::println(
            std::format!(
                "i = {} / z = {} / a = [{}, {}, {}]",
                i,
                z,
                a[0], a[1], a[2]));

        std::assert(a[0] == -1, "a[0] == -1");
        std::assert(a[1] == 2, "a[1] == 2");
        std::assert(a[2] == 3, "a[2] == 3");
        std::assert(z == -1, "z == -1");
        std::assert(i == 0, "i == 0");
    }
}

fn test_chained_assignments() -> void
{
    let x: i32;
    let y: i32;
    let z: i32;

    x = y = 3;
    std::assert(x == 3, "x == 3");
    std::assert(y == 3, "y == 3");

    z = x = y = 4;
    std::assert(x == 4, "x == 4");
    std::assert(y == 4, "y == 4");
    std::assert(z == 4, "z == 4");
}

fn test_compound_assignments() -> void
{
    let x: i32 = 1;
    x += 2;

    std::assert(x == 3, "x == 3");

    let a: [i32] = [0];
    let y: i32;
    
    a[0] = 1; 
    y = a[0] += 2; 

    std::assert(a[0] == 3, "a[0] == 3");
    std::assert(y == 3, "y == 3");
}

fn return_modified_input_i32(a: [i32]) -> [i32]
{
    a[1] += -1;
    return a;
}

fn return_modified_input_i8(a: [i8]) -> [i8]
{
    a[1] += -1i8;
    return a;
}

fn generate_array_i32() -> [i32]
{
    return new i32[3];
}

fn test_modify_returned_array() -> void
{
    std::println("modify returned array: i8");

    let a: [i8] = [1i8, 2i8, 3i8];
    return_modified_input_i8(a)[0] -= 1i8;

    std::println(
        std::format!(
            "{} {} {}",
            a[0], a[1], a[2]));

    std::assert(a[0] == 0i8, "a[0] == 0");
    std::assert(a[1] == 1i8, "a[1] == 1");
    std::assert(a[2] == 3i8, "a[2] == 3");

    std::println("modify returned array: i32");

    let b: [i32] = [1, 2, 3];
    return_modified_input_i32(b)[0] += 1;

    std::println(
        std::format!(
            "{} {} {}",
            b[0], b[1], b[2]));

    std::assert(b[0] == 2, "b[0] == 2");
    std::assert(b[1] == 1, "b[1] == 1");
    std::assert(b[2] == 3, "b[2] == 3");

    // GC test for return values.
    generate_array_i32()[0] = 13;
}

fn main(args: [str]) -> i32 {
    test_nested_evaluation0();
    test_nested_evaluation1();
    test_nested_evaluation2();
    test_nested_evaluation3();

    test_chained_assignments();
    test_compound_assignments();

    test_modify_returned_array();

    return 0;
}
