import std;

// Phase 0a hygiene probe — single-level, compile-time observable.
//
// A macro body that references a bare name must NOT capture a same-named
// binding at the call site. expand_macro() keeps body-originated identifiers
// in a fresh context parented at the macro's *definition* context
// (macro.cpp:299,378), so the reference below should resolve where the macro
// is defined — not at the call site.
//
// Expected under CORRECT hygiene: hygiene_capture_target is undefined in
// capture_probe!'s definition context, so this file FAILS to compile
// ("could not resolve identifier 'hygiene_capture_target'").
// Expected under BROKEN (unhygienic) capture: it would bind to the
// call-site `let hygiene_capture_target` and compile+run, printing 42.
//
// Polarity: this is a "compile must fail" probe. The ctest passes (WILL_FAIL)
// exactly when hygiene holds.
macro capture_probe! {
    () => {
        hygiene_capture_target;
    };
}

fn main(args: str[]) -> i32 {
    let hygiene_capture_target: i32 = 42;

    // Hygienic: capture_probe! does NOT see this binding.
    let r: i32 = capture_probe!();
    std::println(std::format!("{}", r));

    return 0;
}
