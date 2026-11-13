use slang_bindings::{Context, Error, OwnedValue};
use std::path::PathBuf;

fn context_with_test_paths() -> Context {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..");

    let mut context = Context::new().unwrap();
    context.add_search_path(root.join("lang")).unwrap();
    context.add_search_path(root.join("test")).unwrap();

    context
}

#[test]
fn invoke_return_hello() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();
    let function = module.get_function("return_hello").unwrap();

    let result = function.invoke(()).unwrap();

    assert!(matches!(
        result,
        OwnedValue::String(ref value) if value == "Hello"
    ));
}

#[test]
fn invoke_with_i32_argument() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();
    let function = module.get_function("identity_i32").unwrap();

    let result = function.invoke((42,)).unwrap();

    assert!(matches!(result, OwnedValue::I32(42)));
}

#[test]
fn invoke_with_f32_argument() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();
    let function = module.get_function("identity_f32").unwrap();

    let result = function.invoke((1.23,)).unwrap();

    assert!(matches!(
        result,
        OwnedValue::F32(value) if (value - 1.23).abs() < 0.0001
    ));
}

#[test]
fn invoke_with_string_argument() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();
    let function = module.get_function("identity_str").unwrap();

    let result = function.invoke(("Hello",)).unwrap();

    assert!(matches!(
        result,
        OwnedValue::String(ref value) if value == "Hello"
    ));
}

#[test]
fn invoke_with_too_few_arguments() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();
    let function = module.get_function("identity_i32").unwrap();

    let result = function.invoke(());

    match result {
        Err(Error::Generic(message)) => {
            assert!(!message.is_empty());
        }
        other => panic!("Expected invocation error, got {other:?}"),
    }
}

#[test]
fn invoke_with_too_many_arguments() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();
    let function = module.get_function("identity_i32").unwrap();

    let result = function.invoke((42, 43));

    match result {
        Err(Error::Generic(message)) => {
            assert!(!message.is_empty());
        }
        other => panic!("Expected invocation error, got {other:?}"),
    }
}
