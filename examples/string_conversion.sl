import std;

fn main(args: [str]) -> i32
{
    std::println("Arguments as i32:");
    
    let i: i32 = 0;
    while(i < args.length)
    {
        std::println(std::i32_to_string(std::parse_i32(args[i])));
        i += 1;
    }

    std::println("Arguments as f32:");
    
    i = 0;
    while(i < args.length)
    {
        std::println(std::f32_to_string(std::parse_f32(args[i])));
        i += 1;
    }

    return 0;
}
