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
    while(i > 0)
    {
        std::assert(0x7fffffff >> i == r, std::string_concat(
            std::i32_to_string(0x7fffffff >> i), 
            std::string_concat(" == ", std::i32_to_string(r))));

        i--;
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

    std::assert(1 as i64 << 64 == 1 as i64, "1 << 64 == 1");
    std::assert(1 as i64 << 65 == 2 as i64, "1 << 65 == 2");

    std::println("i64: >>");
    i = 0;
    r = 2147483647 as i64; // FIXME Should be -9223372036854775807
    while(i > 0)
    {
        std::assert((0x7fffffff >> i) as i64 == r, std::format!("i: {}", i));

        i--;
        r /= 2 as i64;
    }    
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
    std::assert(-123.0 == 0.0 - 123.0, "-123.0 == 0.0 - 123.0");
    std::assert(+321.0 == 0.0 + 321.0, "+321.0 == 0.0 + 321.0");
}

fn test_f32_binary_operators() -> void
{
    // + - * /
    std::println("f32: + - * /");
    std::assert(1.0 + 2.0 == 3.0, "1.0 + 2.0 == 3.0");
    std::assert(1.0 - 2.0 == -1.0, "1.0 - 2.0 == -1.0");
    std::assert(-1.0 + 2.0 == 1.0, "-1.0 + 2.0 == 1.0");
    std::assert(3.0 * 5.0 == 15.0, "3.0 * 5.0 == 15.0");
    std::assert(-3.0 * 5.0 == -15.0, "-3.0 * 5.0 == -15.0");
    std::assert(-3.0 * -5.0 == 15.0, "-3.0 * -5.0 == 15.0");
    std::assert(3.0 * -5.0 == -15.0, "3.0 * -5.0 == -15.0");
    std::assert(3.0 / 5.0 == 0.6, "3.0 / 5.0 == 0.6");
    std::assert(5.0 / 3.0 == 1.6666666, "5.0 / 3.0 == 1.6666666");

    // < <= > >= == !=
    std::println("f32: < <= > >= == !=");
    std::assert(-1.0 < 1.0, "-1.0 < 1.0");
    std::assert((1.0 < 0.0) == 0, "(1.0 < 0.0) == 0");
    std::assert(17.0 <= 17.0, "17.0 <= 17.0");
    std::assert((17.0 <= 16.0) == 0, "(17.0 <= 16.0) == 0");
    std::assert((-1.0 > 1.0) == 0, "(-1.0 > 1.0) == 0");
    std::assert(1.0 > 0.0, "1.0 > 0.0");
    std::assert(17.0 >= 17.0, "17.0 >= 17.0");
    std::assert((16.0 >= 17.0) == 0, "(16.0 >= 17.0) == 0");
    std::assert((-123.0 == -123.0) == 1, "(-123.0 == -123.0) == 1");
    std::assert((123.0 == -123.0) == 0, "(123.0 == -123.0) == 0");
    std::assert((123.0 != -123.0) == 1, "(123.0 != -123.0) == 1");
    std::assert((-123.0 != -123.0) == 0, "(-123.0 != -123.0) == 0");
    std::assert((1.0 == 2.0) != (3.0 == 3.0), "(1.0 == 2.0) != (3.0 == 3.0)");    
}

fn main(args: [str]) -> i32 {
    test_i32_binary_operators();
    test_i32_unary_operators();
    test_i64_binary_operators();
    test_f32_binary_operators();
    test_f32_unary_operators();

    test_logical_operators();

    return 0;
}
