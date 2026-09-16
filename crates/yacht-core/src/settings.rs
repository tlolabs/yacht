//! Behavior preferences; storage location and appearance/layout remain native.
use crate::{invalid, style::Style, Result};
use serde::{Deserialize, Serialize};
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub remember_style: bool,
    pub preview_rows: usize,
    pub last_style: Style,
}
impl Default for Settings {
    fn default() -> Self {
        Self {
            remember_style: true,
            preview_rows: 200,
            last_style: Style::default(),
        }
    }
}
impl Settings {
    pub fn validate(&self) -> Result<()> {
        if self.preview_rows == 0 || self.preview_rows > 1000 {
            return Err(invalid("Preview rows must be between 1 and 1000."));
        }
        self.last_style.validate()
    }
    pub fn initial_style(&self) -> Style {
        if self.remember_style {
            self.last_style.clone()
        } else {
            Style::default()
        }
    }
}
