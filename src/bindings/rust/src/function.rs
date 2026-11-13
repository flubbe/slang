use crate::error::Error;
use crate::module::Module;
use crate::sys;
use crate::value::{OwnedValue, Value};
use std::ptr::NonNull;

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

    pub fn invoke(&self, args: &[Value<'_>]) -> Result<OwnedValue<'ctx>, Error> {
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

        let result = OwnedValue::from_raw(self.module.context(), raw_result)?;
        Ok(result)
    }
}
