use crate::context::Context;
use crate::error::Error;
use crate::sys;
use crate::types::{NativeType, TypeLayout};
use core::fmt;
use std::ffi::c_void;

/// A value prepared for passing to the interpreter.
///
/// Native values and strings are borrowed and must remain valid for the
/// duration of the call.
pub enum Value<'a> {
    /// A 32-bit signed integer.
    I32(i32),

    /// A 32-bit floating-point number.
    F32(f32),

    /// A borrowed UTF-8 string.
    Str(&'a str),

    /// A borrowed native value, identified by its registered type layout.
    Native {
        data: *const c_void,
        layout_id: usize,
        context: &'a Context,
    },
}

/// Converts a Rust value into a value that can be passed to the interpreter.
///
/// Implementations are provided for `i32`, `f32`, `&str`, and references to
/// types implementing [`NativeType`].
pub trait IntoValue<'a> {
    /// Converts the value using the given interpreter context.
    fn into_value(self, context: &'a Context) -> Result<Value<'a>, Error>;
}

impl<'a> IntoValue<'a> for i32 {
    fn into_value(self, _: &'a Context) -> Result<Value<'a>, Error> {
        Ok(Value::I32(self))
    }
}

impl<'a> IntoValue<'a> for f32 {
    fn into_value(self, _: &'a Context) -> Result<Value<'a>, Error> {
        Ok(Value::F32(self))
    }
}

impl<'a> IntoValue<'a> for &'a str {
    fn into_value(self, _: &'a Context) -> Result<Value<'a>, Error> {
        Ok(Value::Str(self))
    }
}

impl<'a, T: NativeType> IntoValue<'a> for &'a T {
    fn into_value(self, context: &'a Context) -> Result<Value<'a>, Error> {
        let layout = context.get_type_layout::<T>()?;
        Ok(Value::from_type(self, &layout))
    }
}

/// An owned native value returned by the interpreter.
///
/// The underlying value is destroyed when the `NativeValue` is dropped.
pub struct NativeValue<'ctx> {
    raw: sys::slang_value,
    layout_id: usize,
    context: &'ctx Context,
}

impl<'ctx> fmt::Debug for NativeValue<'ctx> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("NativeValue")
            .field("context", &self.context)
            .finish_non_exhaustive()
    }
}

impl Drop for NativeValue<'_> {
    fn drop(&mut self) {
        unsafe {
            sys::slang_value_destroy(self.context.as_raw(), &mut self.raw);
        }
    }
}

impl<'ctx> NativeValue<'ctx> {
    /// Returns a reference to the underlying native value.
    ///
    /// The requested type must match the value's registered type layout.
    /// Returns `None` if the value contains a null pointer.
    ///
    /// # Errors
    ///
    /// Returns [`Error::InvalidArgument`] if the requested type does not match
    /// the value's type layout. Returns an error if the type layout cannot be
    /// retrieved.
    pub fn as_ref<T: NativeType>(&self) -> Result<Option<&T>, Error> {
        let layout = self.context.get_type_layout::<T>()?;

        if self.layout_id != layout.id {
            return Err(Error::InvalidArgument);
        }

        let data = unsafe { self.raw.data.type_.data }.cast::<T>();

        if data.is_null() {
            return Ok(None);
        }

        Ok(Some(unsafe { &*data }))
    }
}

/// Owns a raw C value until it is either destroyed or transferred.
struct RawValueGuard<'ctx> {
    raw: sys::slang_value,
    context: &'ctx Context,
}

fn void_value() -> sys::slang_value {
    sys::slang_value {
        kind: sys::slang_value_kind_SLANG_VALUE_VOID,
        data: unsafe { std::mem::zeroed() },
    }
}

impl<'ctx> RawValueGuard<'ctx> {
    fn new(context: &'ctx Context, raw: sys::slang_value) -> Self {
        Self { raw, context }
    }

    fn into_raw(mut self) -> sys::slang_value {
        std::mem::replace(&mut self.raw, void_value())
    }
}

impl Drop for RawValueGuard<'_> {
    fn drop(&mut self) {
        unsafe {
            let _ = sys::slang_value_destroy(self.context.as_raw(), &mut self.raw);
        }
    }
}

/// An owned value returned by the interpreter.
///
/// Native values are automatically destroyed when dropped.
#[derive(Debug)]
pub enum OwnedValue<'ctx> {
    /// No value was returned.
    Void,

    /// A 32-bit signed integer.
    I32(i32),

    /// A 32-bit floating-point number.
    F32(f32),

    /// An owned UTF-8 string.
    String(String),

    /// An owned native value.
    Native(NativeValue<'ctx>),
}

impl<'a> Value<'a> {
    pub(crate) fn as_raw(&self, context: &Context) -> Result<sys::slang_value, Error> {
        match self {
            Self::I32(value) => Ok(sys::slang_value {
                kind: sys::slang_value_kind_SLANG_VALUE_I32,
                data: sys::slang_value_value_data { i32_: *value },
            }),
            Self::F32(value) => Ok(sys::slang_value {
                kind: sys::slang_value_kind_SLANG_VALUE_F32,
                data: sys::slang_value_value_data { f32_: *value },
            }),
            Self::Str(value) => Ok(sys::slang_value {
                kind: sys::slang_value_kind_SLANG_VALUE_STR,
                data: sys::slang_value_value_data {
                    str_: sys::slang_value_value_data_str_data {
                        length: value.len(),
                        data: value.as_ptr().cast(),
                    },
                },
            }),
            Self::Native {
                layout_id,
                data,
                context: value_context,
            } => {
                if !std::ptr::eq(*value_context, context) {
                    return Err(Error::InvalidArgument);
                }

                Ok(sys::slang_value {
                    kind: sys::slang_value_kind_SLANG_VALUE_TYPE,
                    data: sys::slang_value_value_data {
                        type_: sys::slang_value_value_data_type_data {
                            layout_id: *layout_id,
                            data: *data,
                        },
                    },
                })
            }
        }
    }

    /// Creates an integer value.
    pub fn from_i32(value: i32) -> Self {
        Self::I32(value)
    }

    /// Creates a floating-point value.
    pub fn from_f32(value: f32) -> Self {
        Self::F32(value)
    }

    /// Creates a borrowed native value using the given type layout.
    ///
    /// The value and its type layout must belong to the same interpreter context.
    pub fn from_type<T: NativeType>(value: &'a T, layout: &TypeLayout<'a, T>) -> Self {
        Self::Native {
            data: (value as *const T).cast(),
            layout_id: layout.id,
            context: layout.context(),
        }
    }
}

impl<'ctx> OwnedValue<'ctx> {
    pub(crate) fn from_raw(context: &'ctx Context, raw: sys::slang_value) -> Result<Self, Error> {
        let guard = RawValueGuard::new(context, raw);

        match raw.kind {
            sys::slang_value_kind_SLANG_VALUE_VOID => Ok(Self::Void),
            sys::slang_value_kind_SLANG_VALUE_I32 => unsafe { Ok(Self::I32(raw.data.i32_)) },
            sys::slang_value_kind_SLANG_VALUE_F32 => unsafe { Ok(Self::F32(raw.data.f32_)) },
            sys::slang_value_kind_SLANG_VALUE_STR => {
                let str_value = unsafe { &raw.data.str_ };

                let bytes = if str_value.length == 0 {
                    &[]
                } else {
                    if str_value.data.is_null() {
                        return Err(Error::NullPointer);
                    }

                    unsafe {
                        std::slice::from_raw_parts(str_value.data.cast::<u8>(), str_value.length)
                    }
                };

                let value = String::from_utf8(bytes.to_vec()).map_err(|_| Error::InvalidUtf8)?;
                Ok(Self::String(value))
            }
            sys::slang_value_kind_SLANG_VALUE_TYPE => {
                let layout_id = unsafe { guard.raw.data.type_.layout_id };
                let raw = guard.into_raw();

                Ok(Self::Native(NativeValue {
                    raw,
                    layout_id,
                    context,
                }))
            }
            _ => Err(Error::UnsupportedValue),
        }
    }
}

/// Converts a collection of Rust arguments into values for the interpreter.
///
/// Implementations are provided for tuples containing up to twelve arguments.
/// Individual arguments must implement [`IntoValue`].
pub trait IntoArgs<'a> {
    /// Converts the arguments using the given interpreter context.
    fn into_args(self, context: &'a Context) -> Result<Vec<Value<'a>>, Error>;
}

impl<'a> IntoArgs<'a> for () {
    fn into_args(self, _: &'a Context) -> Result<Vec<Value<'a>>, Error> {
        Ok(Vec::new())
    }
}

macro_rules! impl_into_args {
    ($($type:ident $value:ident),+) => {
        impl<'a, $($type),+> IntoArgs<'a> for ($($type,)+)
        where
            $($type: IntoValue<'a>,)+
        {
            fn into_args(
                self,
                context: &'a Context,
            ) -> Result<Vec<Value<'a>>, Error> {
                let ($($value,)+) = self;

                Ok(vec![
                    $($value.into_value(context)?,)+
                ])
            }
        }
    };
}

impl_into_args!(A a);
impl_into_args!(A a, B b);
impl_into_args!(A a, B b, C c);
impl_into_args!(A a, B b, C c, D d);
impl_into_args!(A a, B b, C c, D d, E e);
impl_into_args!(A a, B b, C c, D d, E e, F f);
impl_into_args!(A a, B b, C c, D d, E e, F f,
                G g);
impl_into_args!(A a, B b, C c, D d, E e, F f,
                G g, H h);
impl_into_args!(A a, B b, C c, D d, E e, F f,
                G g, H h, I i);
impl_into_args!(A a, B b, C c, D d, E e, F f,
                G g, H h, I i, J j);
impl_into_args!(A a, B b, C c, D d, E e, F f,
                G g, H h, I i, J j, K k);
impl_into_args!(A a, B b, C c, D d, E e, F f,
                G g, H h, I i, J j, K k, L l);
