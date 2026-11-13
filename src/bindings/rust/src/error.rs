use std::fmt;

#[derive(Debug)]
pub enum Error {
    Filesystem(String),
    FunctionNotFound(String),
    GarbageCollector(String),
    Generic(String),
    InvalidArgument,
    InvalidFunctionName,
    InvalidModuleName,
    InvalidPath,
    InvalidTypeName,
    InvalidUtf8,
    NullPointer,
    UnknownStatus(i32, String),
    UnsupportedValue,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{self:?}")
    }
}

impl std::error::Error for Error {}
