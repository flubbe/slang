use crate::context::Context;
use std::marker::PhantomData;

/// Describes the native memory layout of a Slang type.
///
/// # Safety
///
/// Implementors must ensure that:
/// - The Rust type's memory layout matches the corresponding Slang type.
/// - All GC-managed pointer fields are represented in `pointer_offsets()`.
/// - Every reported offset is valid for the type's memory layout.
pub unsafe trait NativeType {
    const NAME: &'static str;
    const MODULE: &'static str;

    fn pointer_offsets() -> &'static [usize];
}

pub struct TypeLayout<'ctx, T> {
    pub(crate) id: usize,
    context: &'ctx Context,
    _type: PhantomData<fn() -> T>,
}

impl<T> std::fmt::Debug for TypeLayout<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TypeLayout")
            .field("id", &self.id)
            .finish_non_exhaustive()
    }
}

impl<'ctx, T> TypeLayout<'ctx, T> {
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
