use slang_bindings::{Context, Error, NativeType};
use std::ffi::c_void;

#[repr(C)]
struct NativePoint {
    x: f32,
    y: f32,
}

unsafe impl NativeType for NativePoint {
    const NAME: &'static str = "NativePoint";
    const MODULE: &'static str = "example_package.hello";

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
    const MODULE: &'static str = "example_package.hello";

    fn pointer_offsets() -> &'static [usize] {
        &[std::mem::offset_of!(Self, reference)]
    }
}

struct EmptyName;

unsafe impl NativeType for EmptyName {
    const NAME: &'static str = "";
    const MODULE: &'static str = "";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

struct QualifiedName;

unsafe impl NativeType for QualifiedName {
    const NAME: &'static str = "Some.QualifiedName";
    const MODULE: &'static str = "Some.Module.Path";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

struct NullName;

unsafe impl NativeType for NullName {
    const NAME: &'static str = "Invalid\0Name";
    const MODULE: &'static str = "Invalid\0Module\0Name";

    fn pointer_offsets() -> &'static [usize] {
        &[]
    }
}

#[test]
fn register_type() {
    let context = Context::new().unwrap();

    let registered_id = context.register_type::<NativePoint>().unwrap();
    let queried_id = context.get_type_layout::<NativePoint>().unwrap();

    assert_eq!(registered_id, queried_id);
}

#[test]
fn register_type_twice() {
    let context = Context::new().unwrap();

    context.register_type::<NativePoint>().unwrap();

    assert!(matches!(
        context.register_type::<NativePoint>(),
        Err(Error::GarbageCollector(_))
    ));
}

#[test]
fn register_type_with_pointer_field() {
    let context = Context::new().unwrap();

    context.register_type::<NativeReference>().unwrap();
}

#[test]
fn reject_empty_type_name() {
    let context = Context::new().unwrap();

    assert!(matches!(
        context.register_type::<EmptyName>(),
        Err(Error::InvalidTypeName)
    ));
}

#[test]
fn reject_qualified_type_name() {
    let context = Context::new().unwrap();

    assert!(matches!(
        context.register_type::<QualifiedName>(),
        Err(Error::InvalidTypeName)
    ));
}

#[test]
fn reject_type_name_with_null_byte() {
    let context = Context::new().unwrap();

    assert!(matches!(
        context.register_type::<NullName>(),
        Err(Error::InvalidTypeName)
    ));
}
