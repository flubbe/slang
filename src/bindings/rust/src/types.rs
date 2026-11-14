use crate::context::Context;
use std::marker::PhantomData;

/// Describes the memory layout of a native type.
///
/// Types passed to or returned from the interpreter must use a representation
/// compatible with C, typically `#[repr(C)]`.
///
/// # Safety
///
/// Implementors must ensure that:
/// - The Rust type's memory layout matches the corresponding interpreter type
///   whenever instances are passed to or returned from the interpreter.
/// - All GC-managed pointer fields are included in [`NativeType::pointer_offsets`].
/// - Every reported offset points to a valid pointer field within the type.
pub unsafe trait NativeType {
    /// The unqualified type name.
    const NAME: &'static str;

    /// Dot-separated fully qualified name of the module defining the type, e.g. `path.to.module`.
    const MODULE: &'static str;

    /// Returns the byte offsets of all GC-managed pointer fields.
    ///
    /// The offsets are relative to the beginning of the type.
    fn pointer_offsets() -> &'static [usize];
}

/// Describes the registered memory layout of a native type.
///
/// A `TypeLayout` is associated with the [`Context`] in which the type was
/// registered.
pub struct TypeLayout<'ctx, T: NativeType> {
    pub(crate) id: usize,
    context: &'ctx Context,
    _type: PhantomData<fn() -> T>,
}

impl<T: NativeType> std::fmt::Debug for TypeLayout<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TypeLayout")
            .field("id", &self.id)
            .finish_non_exhaustive()
    }
}

impl<'ctx, T: NativeType> TypeLayout<'ctx, T> {
    pub(crate) fn new(context: &'ctx Context, id: usize) -> Self {
        Self {
            id,
            context,
            _type: PhantomData,
        }
    }

    pub(crate) fn context(&self) -> &'ctx Context {
        self.context
    }
}

impl<T: NativeType> PartialEq for TypeLayout<'_, T> {
    fn eq(&self, other: &Self) -> bool {
        std::ptr::eq(self.context, other.context) && self.id == other.id
    }
}

impl<T: NativeType> Eq for TypeLayout<'_, T> {}
