import std;

fn test_i32_unary_operators() -> void
{
    // unary + - !
    std::println("i32: unary + - !");
    std::assert(-123 == 0 - 123, "-123 == 0 - 123");
    std::assert(+321 == 0 + 321, "+321 == 0 + 321");
    std::assert(!0 == 1, "!0 == 1");
    std::assert(!1 == 0, "!1 == 0");
    std::assert(!123 == 0, "!123 == 0");
    std::assert(!!123 == 1, "!!123 == 1");

    std::assert(-!!123 == -1, "-!!123 == -1");
    std::assert(!-!123 == 1, "!-!123 == 1");
    std::assert(!!-123 == 1, "!!-123 == 1");
}

fn test_i32_binary_operators() -> void
{
    // + - * /
    std::println("i32: + - * /");
    std::assert(1 + 2 == 3, "1 + 2 == 3");
    std::assert(1 - 2 == -1, "1 - 2 == -1");
    std::assert(-1 + 2 == 1, "-1 + 2 == 1");
    std::assert(3 * 5 == 15, "3 * 5 == 15");
    std::assert(-3 * 5 == -15, "-3 * 5 == -15");
    std::assert(-3 * -5 == 15, "-3 * -5 == 15");
    std::assert(3 * -5 == -15, "3 * -5 == -15");
    std::assert(3 / 5 == 0, "3 / 5 == 0");
    std::assert(5 / 3 == 1, "5 / 3 == 1");

    // & | ^ %
    std::println("i32: & | ^ %");
    std::assert((12 & 8) == 8, "(12 & 8) == 8");
    std::assert((12 & -8) == 8, "(12 & -8) == 8");
    std::assert((-12 & 8) == 0, "(-12 & 8) == 0");
    std::assert((-12 & -8) == -16, "(-12 & -8) == -16");
    std::assert((12 | 2) == 14, "(12 | 2) == 14");
    std::assert((12 | -2) == -2, "(12 | -2) == -2");
    std::assert((-12 | 2) == -10, "(-12 | 2) == -10");
    std::assert((-12 | -2) == -2, "(-12 | -2) == -2");
    std::assert((123 ^ 43) == 80, "(123 ^ 43) == 80");
    std::assert((123 ^ -43) == -82, "(123 ^ -43) == -82");
    std::assert((-123 ^ 43) == -82, "(-123 ^ 43) == -82");
    std::assert((-123 ^ -43) == 80, "(-123 ^ -43) == 80");
    std::assert(17 % 3 == 2, "17 % 3 == 2");
    std::assert(17 % -3 == 2, "17 % -3 == 2");
    std::assert(-17 % 3 == -2, "-17 % 3 == -2");
    std::assert(-17 % -3 == -2, "-17 % -3 == -2");

    // < <= > >= == !=
    std::println("i32: < <= > >= == !=");
    std::assert(-1 < 1, "-1 < 1");
    std::assert((1 < 0) == 0, "(1 < 0) == 0");
    std::assert(17 <= 17, "17 <= 17");
    std::assert((17 <= 16) == 0, "(17 <= 16) == 0");
    std::assert((-1 > 1) == 0, "(-1 > 1) == 0");
    std::assert(1 > 0, "1 > 0");
    std::assert(17 >= 17, "17 >= 17");
    std::assert((16 >= 17) == 0, "(16 >= 17) == 0");
    std::assert((-123 == -123) == 1, "(-123 == -123) == 1");
    std::assert((123 == -123) == 0, "(123 == -123) == 0");
    std::assert((123 != -123) == 1, "(123 != -123) == 1");
    std::assert((-123 != -123) == 0, "(-123 != -123) == 0");
    std::assert((1 == 2) != (3 == 3), "(1 == 2) != (3 == 3)");

    std::println("i32: <<");
    let i: i32 = 0;
    let r: i32 = 1;
    while(i < 32)
    {
        std::assert(1 << i == r, std::string_concat(
            std::i32_to_string(1 << i), 
            std::string_concat(" == ", std::i32_to_string(r))));
        
        i++;
        r *= 2;
    }
    std::assert(1 << 32 == 1, "1 << 32 == 1");
    std::assert(1 << 33 == 2, "1 << 33 == 2");

    std::println("i32: >>");
    i = 0;
    r = 2147483647;
    while(i < 32)
    {
        std::assert(0x7fffffff >> i == r, std::string_concat(
            std::i32_to_string(0x7fffffff >> i), 
            std::string_concat(" == ", std::i32_to_string(r))));

        i++;
        r /= 2;
    }
}

fn test_i64_binary_operators() -> void
{
    std::println("i64: <<");
    let i: i32 = 0;
    let r: i64 = 1 as i64;
    while(i < 32)
    {
        std::assert(1 as i64 << i == r, std::format!("shl {}", i));
        
        i++;
        r *= 2 as i64;
    }

    std::println(std::format!("1 << 64 == {}", (1 as i64 << 64) as i32));
    std::println(std::format!("1 << 64 == {}", (1 as i64 << 64 == 1 as i64) as i32));

    std::assert(1 as i64 << 64 == 1 as i64, "1 << 64 == 1");
    std::assert(1 as i64 << 65 == 2 as i64, "1 << 65 == 2");

    std::println("i64: >>");
    i = 0;
    r = 9223372036854775807i64;
    while(i < 64)
    {
        std::assert(9223372036854775807i64 >> i == r, std::format!("i: {}", i));

        i++;
        r /= 2 as i64;
    }

    std::assert((9223372036854775807i64 ^ 9223372036854775807i64) == 0i64, "(9223372036854775807i64 ^ 9223372036854775807i64) == 0i64");
}

fn test_logical_operators() -> void
{
    std::println("testing logical and...");
    std::assert((0 && 0) == 0, "(0 && 0) == 0");
    std::assert((0 && 1) == 0, "(0 && 1) == 0");
    std::assert((1 && 0) == 0, "(1 && 0) == 0");
    std::assert((1 && 1) == 1, "(1 && 1) == 1");

    std::assert((123 && -3) == 1, "(123 && -3) == 1");
    std::println("testing logical or...");
    std::assert((0 || 0) == 0, "(0 || 0) == 0");
    std::assert((0 || 1) == 1, "(0 || 1) == 1");
    std::assert((1 || 0) == 1, "(1 || 0) == 1");
    std::assert((1 || 1) == 1, "(1 || 1) == 0");

    std::assert((-4 || 5) == 1, "(-4 || 5) == 1");

    // test short-circuit evaluation.
    let x: i32 = 0;
    
    0 && (x = 1);
    std::assert(x == 0, "x == 0");

    1 && (x = 1);
    std::assert(x == 1, "x == 1");

    x = 0;
    0 || (x = 1);
    std::assert(x == 1, "x == 1");

    x = 0;
    1 || (x = 1);
    std::assert(x == 0, "x == 0");
}

fn test_f32_unary_operators() -> void
{
    // unary + -
    std::println("f32: unary + -");
    std::assert(-123.0 as f32 == 0.0 as f32 - 123.0 as f32, "-123.0 == 0.0 - 123.0");
    std::assert(+321.0 as f32 == 0.0 as f32 + 321.0 as f32, "+321.0 == 0.0 + 321.0");
}

fn test_f32_binary_operators() -> void
{
    // + - * /
    std::println("f32: + - * /");
    std::assert(1.0 as f32 + 2.0 as f32 == 3.0 as f32, "1.0 + 2.0 == 3.0");
    std::assert(1.0 as f32 - 2.0 as f32 == -1.0 as f32, "1.0 - 2.0 == -1.0");
    std::assert(-1.0 as f32 + 2.0 as f32 == 1.0 as f32, "-1.0 + 2.0 == 1.0");
    std::assert(3.0 as f32 * 5.0 as f32 == 15.0 as f32, "3.0 * 5.0 == 15.0");
    std::assert(-3.0 as f32 * 5.0 as f32 == -15.0 as f32, "-3.0 * 5.0 == -15.0");
    std::assert(-3.0 as f32 * -5.0 as f32 == 15.0 as f32, "-3.0 * -5.0 == 15.0");
    std::assert(3.0 as f32 * -5.0 as f32 == -15.0 as f32, "3.0 * -5.0 == -15.0");
    std::assert(3.0 as f32 / 5.0 as f32 == 0.6 as f32, "3.0 / 5.0 == 0.6");
    std::assert(5.0 as f32 / 3.0 as f32 == 1.6666666 as f32, "5.0 / 3.0 == 1.6666666");

    // < <= > >= == !=
    std::println("f32: < <= > >= == !=");
    std::assert(-1.0 as f32 < 1.0 as f32, "-1.0 < 1.0");
    std::assert((1.0 as f32 < 0.0 as f32) == 0, "(1.0 < 0.0) == 0");
    std::assert(17.0 as f32 <= 17.0 as f32, "17.0 <= 17.0");
    std::assert((17.0 as f32 <= 16.0 as f32) == 0, "(17.0 <= 16.0) == 0");
    std::assert((-1.0 as f32 > 1.0 as f32) == 0, "(-1.0 > 1.0) == 0");
    std::assert(1.0 as f32 > 0.0 as f32, "1.0 > 0.0");
    std::assert(17.0 as f32 >= 17.0 as f32, "17.0 >= 17.0");
    std::assert((16.0 as f32 >= 17.0 as f32) == 0, "(16.0 >= 17.0) == 0");
    std::assert((-123.0 as f32 == -123.0 as f32) == 1, "(-123.0 == -123.0) == 1");
    std::assert((123.0 as f32 == -123.0 as f32) == 0, "(123.0 == -123.0) == 0");
    std::assert((123.0 as f32 != -123.0 as f32) == 1, "(123.0 != -123.0) == 1");
    std::assert((-123.0 as f32 != -123.0 as f32) == 0, "(-123.0 != -123.0) == 0");
    std::assert((1.0 as f32 == 2.0 as f32) != (3.0 as f32 == 3.0 as f32), "(1.0 == 2.0) != (3.0 == 3.0)");    
}

fn test_compound_assignments() -> void {
    std::println("compound assignments: [i32]/i32");

    let x: [i32] = [0, 0, 0];
    let i: i32 = 0;

    x[i += 1] += 2;

    std::println(
        std::format!(
            "i = {} / x = [{}, {}, {}]", 
            i, x[0], x[1], x[2]));

    std::assert(x[0] == 0, "x[0] == 0");
    std::assert(x[1] == 2, "x[1] == 2");
    std::assert(x[2] == 0, "x[2] == 0");
    std::assert(i == 1, "i == 1");

    std::println("compound assignments: [i32]/i8");

    x[0] = 0;
    x[1] = 0;
    x[2] = 0;
    let j: i8 = 0i8;

    x[(j += 1i8) as i32] += 2;

    std::println(
        std::format!(
            "i = {} / x = [{}, {}, {}]", 
            i, x[0], x[1], x[2]));

    std::assert(x[0] == 0, "x[0] == 0");
    std::assert(x[1] == 2, "x[1] == 2");
    std::assert(x[2] == 0, "x[2] == 0");
    std::assert(j == 1i8, "j == 1");

    std::println("compound assignments: [i16]/i32");

    let y: [i16] = [1i16, 2i16, 3i16];
    i = 0;

    y[(i += 1) + 1] += 2i16;

    std::println(
        std::format!(
            "i = {} / y = [{}, {}, {}]", 
            i, y[0], y[1], y[2]));

    std::assert(y[0] == 1i16, "y[0] == 1");
    std::assert(y[1] == 2i16, "y[1] == 2");
    std::assert(y[2] == 5i16, "y[2] == 5");
    std::assert(i == 1, "i == 1");
}

struct S {
    arr: [i32]
};

fn foo() -> i32 {
    return -1;
}

fn test_nested_evaluation() -> void {
    std::println("nested evaluations:");

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
    test_i32_binary_operators();
    test_i32_unary_operators();
    test_i64_binary_operators();
    test_f32_binary_operators();
    test_f32_unary_operators();

    test_logical_operators();
    test_compound_assignments();

    test_nested_evaluation();
    test_modify_returned_array();

    return 0;
}
