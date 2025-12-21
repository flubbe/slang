import std;

fn main(args: str[]) -> i32
{
    std::println("Arguments as i32:");
    
    let i: i32 = 0;
    let res: std::result;
    while(i < args.length)
    {
        res = std::parse_i32(args[i]);
        if(res.ok)
        {
            std::println(std::i32_to_string((res.value as std::i32s).value));
        }
        else
        {
            std::println(std::string_concat("Conversion to i32 failed: ", res.value as str));
        }

        i += 1;
    }

    std::println("Arguments as f32:");
    
    i = 0;
    while(i < args.length)
    {
        res = std::parse_f32(args[i]);
        if(res.ok)
        {
            std::println(std::f32_to_string((res.value as std::f32s).value));
        }
        else
        {
            std::println(std::string_concat("Conversion to f32 failed: ", res.value as str));
        }

        i += 1;
    }

    return 0;
}
