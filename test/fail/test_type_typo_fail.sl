// A mistyped type name should yield a "did you mean" suggestion.
struct custom_type {
};

struct S {
    field: custom_types
};
