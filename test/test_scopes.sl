import std;

const CONST: i32 = 123;

fn test() -> i32 
{
    let CONST: i32 = 1;
    return CONST;
}

fn test_local_scopes() -> void 
{
    let i: i32 = 1;

    {
        let i: i32 = 2;
        std::assert(i == 2, "i == 2");

        {
            let j: f32 = 1.23 as f32;
            std::assert(j == 1.23 as f32, "j == 1.23");

            i = 0;
        }

        std::assert(i == 0, "i == 0");
    }

    std::assert(i == 1, "i == 1");
}

fn main(args: str[]) -> i32 
{
    std::assert(test() == 1, "test() == 1");

    test_local_scopes();

    return 0;
}

