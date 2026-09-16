//! Authoritative, platform-independent YACHT behavior. No UI or OS shell APIs.
pub mod api;
pub mod csv;
pub mod files;
pub mod html;
pub mod preview;
pub mod settings;
pub mod style;
pub mod table;
use serde::Serialize;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("{0}")]
    Invalid(String),
    #[error("Cancelled")]
    Cancelled,
    #[error("{0}")]
    Io(#[from] std::io::Error),
    #[error("Invalid JSON: {0}")]
    Json(#[from] serde_json::Error),
}
pub type Result<T> = std::result::Result<T, Error>;
pub fn invalid(text: impl Into<String>) -> Error {
    Error::Invalid(text.into())
}
pub type Cancel<'a> = &'a dyn Fn() -> bool;
pub fn check(cancel: Cancel<'_>) -> Result<()> {
    if cancel() {
        Err(Error::Cancelled)
    } else {
        Ok(())
    }
}
#[derive(Serialize)]
pub struct Failure {
    pub code: &'static str,
    pub message: String,
}
impl From<Error> for Failure {
    fn from(e: Error) -> Self {
        Self {
            code: match e {
                Error::Cancelled => "cancelled",
                Error::Io(_) => "io_error",
                _ => "invalid_input",
            },
            message: e.to_string(),
        }
    }
}
