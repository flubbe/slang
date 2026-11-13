use crate::context::Context;
use crate::error::Error;
use crate::function::Function;
use crate::sys;
use std::ffi::CString;
use std::ptr::NonNull;

pub struct Module<'ctx> {
    context: &'ctx Context,
    raw: NonNull<sys::slang_module_impl>,
}

impl<'ctx> Module<'ctx> {
    pub(crate) fn from_raw(context: &'ctx Context, raw: NonNull<sys::slang_module_impl>) -> Self {
        Self { context, raw }
    }

    pub(crate) fn context_as_raw(&self) -> *mut sys::slang_context {
        self.context.as_raw()
    }

    pub(crate) fn context(&self) -> &'ctx Context {
        self.context
    }

    pub(crate) fn check_status(&self, status: sys::slang_status) -> Result<(), Error> {
        self.context.check_status(status)
    }

    pub fn has_function(&self, name: &str) -> Result<bool, Error> {
        let name = CString::new(name).map_err(|_| Error::InvalidFunctionName)?;
        let mut has_function: bool = false;

        let status = unsafe {
            sys::slang_module_has_function(
                self.context.as_raw(),
                self.raw.as_ptr(),
                name.as_ptr(),
                &mut has_function,
            )
        };
        self.context.check_status(status)?;

        Ok(has_function)
    }

    pub fn get_function(&self, name: &str) -> Result<Function<'_, 'ctx>, Error> {
        let name = CString::new(name).map_err(|_| Error::InvalidFunctionName)?;
        let mut raw_function = std::ptr::null_mut();

        let status = unsafe {
            sys::slang_module_get_function(
                self.context.as_raw(),
                self.raw.as_ptr(),
                name.as_ptr(),
                &mut raw_function,
            )
        };
        self.context.check_status(status)?;

        let raw_function = NonNull::new(raw_function).ok_or(Error::NullPointer)?;
        Ok(Function::from_raw(self, raw_function))
    }
}
