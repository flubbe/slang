import std;

fn test_underflow() -> void {
    {
        let x: i8 = -100i8;
        x = x - 100i8;
        std::assert(x == 56i8, "x == 56");

        let y: i8 = -100i8;
        y -= 100i8;
        std::assert(y == 56i8, "y == 56");
    }

    {
        let x: i16 = -32700i16;
        x = x - 100i16;
        std::assert(x == 32736i16, "x == 32736");

        let y: i16 = -32700i16;
        y -= 100i16;
        std::assert(y == 32736i16, "y == 32736");
    }
}

fn test_overflow() -> void {
    {
        let x: i8 = 100i8;
        x = x + 100i8;
        std::assert(x == -56i8, "x == -56");

        let y: i8 = 100i8;
        y += 100i8;
        std::assert(y == -56i8, "y == -56");
    }

    {
        let x: i16 = 32700i16;
        x = x + 100i16;
        std::assert(x == -32736i16, "x == -32736");

        let y: i16 = 32700i16;
        y += 100i16;
        std::assert(y == -32736i16, "y == -32736");
    }
}

fn main(args: str[]) -> i32 {
    test_underflow();
    test_overflow();

    return 0;
}
