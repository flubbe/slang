use crate::sys::slang_status;

pub const STATUS_OK: slang_status = crate::sys::slang_status_SLANG_STATUS_OK;
pub const STATUS_ERROR: slang_status = crate::sys::slang_status_SLANG_STATUS_ERROR;
pub const STATUS_INVALID_ARGUMENT: slang_status =
    crate::sys::slang_status_SLANG_STATUS_INVALID_ARGUMENT;
pub const STATUS_FILESYSTEM_ERROR: slang_status =
    crate::sys::slang_status_SLANG_STATUS_FILESYSTEM_ERROR;
