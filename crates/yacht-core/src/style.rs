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
    pub first_column_enabled: bool,
    pub first_column_bg: String,
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
            first_column_enabled: false,
            first_column_bg: "#ffffff".into(),
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
            first_column_enabled: false,
            first_column_bg: "#ffffff".into(),
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
            &self.first_column_bg,
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
/// Built-in styles live in the shared engine so every native host uses the same palette.
pub fn builtin_presets() -> std::collections::BTreeMap<String, Style> {
    let mut presets = std::collections::BTreeMap::from([
        ("Y-Default".into(), Style::default()),
        ("Unstyled".into(), Style::unstyled()),
    ]);
    for (name, header, first, zebra, striped) in [
        ("N-Blue", "#00a2ff", "#ffffff", "#f7f7f7", true),
        ("N-Orange", "#ffb000", "#ffffff", "#f7f7f7", true),
        ("N-Green", "#18b000", "#ffffff", "#f7f7f7", true),
        ("N-Red", "#ff2600", "#ffffff", "#f7f7f7", true),
        ("N-Gray", "#bfc1c1", "#dcdcdc", "#ffffff", false),
        ("N-White", "#ffffff", "#ffffff", "#f3f8f7", true),
    ] {
        presets.insert(
            name.into(),
            Style {
                table_class: String::new(),
                font_family: "Helvetica, Arial, sans-serif".into(),
                border_color: "#d9d9d9".into(),
                header_bg: header.into(),
                header_text_color: "#222222".into(),
                first_column_enabled: true,
                first_column_bg: first.into(),
                zebra_enabled: striped,
                zebra_bg: zebra.into(),
                hover_enabled: false,
                ..Style::default()
            },
        );
    }
    // Google Sheets alternating-color references, converted from the screenshots'
    // embedded display profile to sRGB. The two gray references share a palette.
    for (name, header, zebra) in [
        ("G-Gray", "#bdbdbd", "#f3f3f3"),
        ("G-Cyan", "#4dd0e1", "#e0f7fa"),
        ("G-Green", "#63d297", "#e7f9ef"),
        ("G-Yellow", "#f7cb4c", "#fef8e3"),
        ("G-Orange", "#f46524", "#ffe6dd"),
        ("G-Blue", "#5c95f9", "#e8f0fe"),
        ("G-Teal", "#26a69a", "#ddf2f0"),
        ("G-Blue Gray", "#78909c", "#ebeff1"),
        ("G-Brown", "#cca677", "#f8f2eb"),
        ("G-Light Green", "#8bc34a", "#eef7e3"),
        ("G-Purple", "#8989eb", "#e8e7fc"),
    ] {
        presets.insert(
            name.into(),
            Style {
                table_class: String::new(),
                font_family: "Arial, sans-serif".into(),
                border_color: "#c4c7c5".into(),
                header_bg: header.into(),
                zebra_bg: zebra.into(),
                hover_enabled: false,
                ..Style::default()
            },
        );
    }
    presets
}
pub fn safe_color(value: &str) -> bool {
    let bytes = value.as_bytes();
    // Fast path: standard hex colors (#RGB, #RGBA, #RRGGBB, #RRGGBBAA)
    if bytes.first() == Some(&b'#') {
        let len = bytes.len();
        if (len == 4 || len == 5 || len == 7 || len == 9)
            && bytes[1..].iter().all(|b| b.is_ascii_hexdigit())
        {
            return true;
        }
    }
    // Fast path: pure ASCII alphabetic keywords (e.g. "transparent", "white", "black")
    if !bytes.is_empty() && bytes.iter().all(|b| b.is_ascii_alphabetic()) {
        return true;
    }
    static COLOR: LazyLock<Regex> = LazyLock::new(|| {
        Regex::new(r"\A(?:#[0-9a-fA-F]{3}|#[0-9a-fA-F]{4}|#[0-9a-fA-F]{6}|#[0-9a-fA-F]{8}|[a-zA-Z]+|(?:rgb|hsl)a?\([0-9.,% /+\-]+\))\z").unwrap()
    });
    COLOR.is_match(value)
}
