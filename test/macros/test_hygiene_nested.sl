import std;

// Phase 0a hygiene probe — nested expansion, inner binding hygiene.
//
// outer! expands to a nested invocation inner!(); inner!'s body references a
// bare name. The name must resolve in *inner!'s* definition context, not the
// call site where nested_capture_target is bound. This exercises the nested
// expansion pass (do…while(expand_macros)) and the "inner binding" case.
//
// Expected under CORRECT hygiene: nested_capture_target is undefined in
// inner!'s definition context, so this file FAILS to compile.
// Expected under BROKEN capture: it would bind to the call-site binding and
// compile+run, printing 7.
//
// Note: nested *macro arguments* are rejected outright (macro.cpp:215-229) and
// `let`-introduced bindings need multi-statement bodies (gated on token
// expansion, Phase 0b). This probe uses a nested *invocation* in a body, which
// is the reachable single-expression nested surface today.
macro inner! {
    () => {
        nested_capture_target;
    };
}

macro outer! {
    () => {
        inner!();
    };
}

fn main(args: str[]) -> i32 {
    let nested_capture_target: i32 = 7;

    // Hygienic: neither outer! nor inner! sees this binding.
    let r: i32 = outer!();
    std::println(std::format!("{}", r));

    return 0;
}
