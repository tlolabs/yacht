use crate::{invalid, Result};
use regex::Regex;
use serde::{Deserialize, Serialize};
use std::sync::LazyLock;
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub struct Style {
    pub table_class: String,
    pub font_family: String,
    pub font_size_px: i64,
    pub cell_padding_px: i64,
    pub border_width_px: i64,
    pub border_style: String,
    pub border_color: String,
    pub header_bg: String,
    pub header_text_color: String,
    pub body_bg: String,
    pub zebra_enabled: bool,
    pub zebra_bg: String,
    pub hover_enabled: bool,
    pub hover_bg: String,
    pub border_collapse: String,
    pub border_spacing_px: i64,
}
impl Default for Style {
    fn default() -> Self {
        Self {
            table_class: "table table-bordered table-hover table-condensed".into(),
            font_family: "\"Helvetica Neue\", Helvetica, Arial, sans-serif".into(),
            font_size_px: 14,
            cell_padding_px: 8,
            border_width_px: 1,
            border_style: "solid".into(),
            border_color: "#cccccc".into(),
            header_bg: "#f5f5f5".into(),
            header_text_color: "#222222".into(),
            body_bg: "#ffffff".into(),
            zebra_enabled: true,
            zebra_bg: "#fbfbfb".into(),
            hover_enabled: true,
            hover_bg: "#f2f8ff".into(),
            border_collapse: "collapse".into(),
            border_spacing_px: 0,
        }
    }
}
impl Style {
    pub fn unstyled() -> Self {
        Self {
            table_class: "".into(),
            font_family: "".into(),
            font_size_px: 16,
            cell_padding_px: 0,
            border_width_px: 0,
            border_style: "none".into(),
            border_color: "#000000".into(),
            header_bg: "transparent".into(),
            header_text_color: "#000000".into(),
            body_bg: "transparent".into(),
            zebra_enabled: false,
            zebra_bg: "transparent".into(),
            hover_enabled: false,
            hover_bg: "transparent".into(),
            border_collapse: "separate".into(),
            border_spacing_px: 0,
        }
    }
    /// Swift's decodeIfPresent historically treats null values as missing keys.
    pub fn from_value(mut v: serde_json::Value) -> Result<Self> {
        if let Some(o) = v.as_object_mut() {
            o.retain(|_, value| !value.is_null());
        }
        Ok(serde_json::from_value(v)?)
    }
    pub fn validate(&self) -> Result<()> {
        for (name, value, min) in [
            ("Font size", self.font_size_px, 1),
            ("Cell padding", self.cell_padding_px, 0),
            ("Border width", self.border_width_px, 0),
            ("Border spacing", self.border_spacing_px, 0),
        ] {
            if !(min..=10000).contains(&value) {
                return Err(invalid(format!(
                    "{name} must be between {min} and 10000 pixels."
                )));
            }
        }
        if ![
            "solid", "dashed", "dotted", "double", "none", "hidden", "groove", "ridge", "inset",
            "outset",
        ]
        .contains(&self.border_style.as_str())
            || !["collapse", "separate"].contains(&self.border_collapse.as_str())
        {
            return Err(invalid("Choose a valid border style and collapse mode."));
        }
        static FONT: LazyLock<Regex> = LazyLock::new(|| {
            Regex::new(r#"\A\s*(?:[\p{L}\p{N}_ -]+|"[\p{L}\p{N}_ ,.-]+"|'[\p{L}\p{N}_ ,.-]+')(?:\s*,\s*(?:[\p{L}\p{N}_ -]+|"[\p{L}\p{N}_ ,.-]+"|'[\p{L}\p{N}_ ,.-]+'))*\s*\z"#).unwrap()
        });
        if !self.font_family.is_empty() && !FONT.is_match(&self.font_family) {
            return Err(invalid("Enter a comma-separated list of font names, for example Helvetica, Arial, sans-serif."));
        }
        for color in [
            &self.border_color,
            &self.header_bg,
            &self.header_text_color,
            &self.body_bg,
            &self.zebra_bg,
            &self.hover_bg,
        ] {
            if !safe_color(color) {
                return Err(invalid(format!(
                    "Invalid CSS color: {color}. Use a hex color, CSS color name, rgb(), or hsl()."
                )));
            }
        }
        Ok(())
    }
}
pub fn safe_color(value: &str) -> bool {
    static COLOR: LazyLock<Regex> = LazyLock::new(|| {
        Regex::new(r"\A(?:#[0-9a-fA-F]{3}|#[0-9a-fA-F]{4}|#[0-9a-fA-F]{6}|#[0-9a-fA-F]{8}|[a-zA-Z]+|(?:rgb|hsl)a?\([0-9.,% /+\-]+\))\z").unwrap()
    });
    COLOR.is_match(value)
}
