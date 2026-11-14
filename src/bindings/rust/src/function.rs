use crate::error::Error;
use crate::module::Module;
use crate::sys;
use crate::value::{IntoArgs, OwnedValue};
use std::ptr::NonNull;

/// A function defined in a loaded module.
///
/// A `Function` is tied to the lifetime of the [`Module`] it belongs to and
/// can be used to invoke the function with native arguments.
pub struct Function<'module, 'ctx> {
    module: &'module Module<'ctx>,
    raw: NonNull<sys::slang_function_impl>,
}

impl<'module, 'ctx> Function<'module, 'ctx> {
    pub(crate) fn from_raw(
        module: &'module Module<'ctx>,
        raw: NonNull<sys::slang_function_impl>,
    ) -> Self {
        Self { module, raw }
    }

    /// Invokes the function with the given arguments.
    ///
    /// Arguments are converted into the interpreter's internal representation
    /// through [`IntoArgs`] before invocation.
    ///
    /// Returns `Some` if the function produces a value, or `None` if it returns
    /// no value.
    ///
    /// # Errors
    ///
    /// Returns an error if the arguments cannot be converted, the function
    /// invocation fails, or the returned value cannot be represented by the bindings.
    pub fn invoke<A>(&self, args: A) -> Result<Option<OwnedValue<'ctx>>, Error>
    where
        A: IntoArgs<'ctx>,
    {
        let args = args.into_args(self.module.context())?;
        let raw_args: Result<Vec<_>, _> = args
            .iter()
            .map(|arg| arg.as_raw(self.module.context()))
            .collect();
        let raw_args = raw_args?;

        let mut raw_result = sys::slang_value {
            kind: sys::slang_value_kind_SLANG_VALUE_VOID,
            data: unsafe { std::mem::zeroed() },
        };

        let status = unsafe {
            sys::slang_function_invoke(
                self.module.context_as_raw(),
                self.raw.as_ptr(),
                if raw_args.is_empty() {
                    std::ptr::null()
                } else {
                    raw_args.as_ptr()
                },
                raw_args.len(),
                &mut raw_result,
            )
        };
        self.module.check_status(status)?;

        if raw_result.kind == sys::slang_value_kind_SLANG_VALUE_NULL {
            return Ok(None);
        }

        if (raw_result.kind == sys::slang_value_kind_SLANG_VALUE_STR
            && unsafe { raw_result.data.str_.data.is_null() })
            || (raw_result.kind == sys::slang_value_kind_SLANG_VALUE_TYPE
                && unsafe { raw_result.data.type_.data.is_null() })
        {
            return Err(Error::NullPointer);
        }

        let result = OwnedValue::from_raw(self.module.context(), raw_result)?;
        Ok(Some(result))
    }
}
