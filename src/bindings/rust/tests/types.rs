use slang_bindings::{Context, Error, NativeType};
use std::ffi::c_void;

#[repr(C)]
struct NativePoint {
    x: f32,
    y: f32,
}

unsafe impl NativeType for NativePoint {
    const NAME: &'static str = "NativePoint";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

#[repr(C)]
struct NativeReference {
    value: i32,
    reference: *mut c_void,
}

unsafe impl NativeType for NativeReference {
    const NAME: &'static str = "NativeReference";

    fn pointer_offsets() -> &'static [usize] {
        &[std::mem::offset_of!(Self, reference)]
    }
}

struct EmptyName;

unsafe impl NativeType for EmptyName {
    const NAME: &'static str = "";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

struct QualifiedName;

unsafe impl NativeType for QualifiedName {
    const NAME: &'static str = "Some.QualifiedName";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

struct NullName;

unsafe impl NativeType for NullName {
    const NAME: &'static str = "Invalid\0Name";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

const MODULE: &str = "example_package.hello";

#[test]
fn register_type() {
    let context = Context::new().unwrap();

    let registered_id = context.register_type::<NativePoint>(MODULE).unwrap();
    let queried_id = context
        .get_type_layout::<NativePoint>("example_package.hello")
        .unwrap();

    assert_eq!(registered_id, queried_id);
}

#[test]
fn register_type_twice() {
    let context = Context::new().unwrap();

    context.register_type::<NativePoint>(MODULE).unwrap();

    assert!(matches!(
        context.register_type::<NativePoint>(MODULE),
        Err(Error::GarbageCollector(_))
    ));
}

#[test]
fn register_type_with_pointer_field() {
    let context = Context::new().unwrap();

    context.register_type::<NativeReference>(MODULE).unwrap();
}

#[test]
fn reject_empty_type_name() {
    let context = Context::new().unwrap();

    assert!(matches!(
        context.register_type::<EmptyName>(MODULE),
        Err(Error::InvalidTypeName)
    ));
}

#[test]
fn reject_qualified_type_name() {
    let context = Context::new().unwrap();

    assert!(matches!(
        context.register_type::<QualifiedName>(MODULE),
        Err(Error::InvalidTypeName)
    ));
}

#[test]
fn reject_type_name_with_null_byte() {
    let context = Context::new().unwrap();

    assert!(matches!(
        context.register_type::<NullName>(MODULE),
        Err(Error::InvalidTypeName)
    ));
}
