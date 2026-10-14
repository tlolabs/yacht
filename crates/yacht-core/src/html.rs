use crate::{check, style::Style, table::Table, Cancel, Result};
use std::io::Write;

pub fn escape(text: &str) -> String {
    let bytes = text.as_bytes();
    if !bytes
        .iter()
        .any(|&b| matches!(b, b'&' | b'<' | b'>' | b'"' | b'\''))
    {
        return text.to_owned();
    }
    let mut out = Vec::with_capacity(text.len() + 16);
    let mut start = 0;
    for (i, &b) in bytes.iter().enumerate() {
        let replacement: &[u8] = match b {
            b'&' => b"&amp;",
            b'<' => b"&lt;",
            b'>' => b"&gt;",
            b'"' => b"&quot;",
            b'\'' => b"&#x27;",
            _ => continue,
        };
        out.extend_from_slice(&bytes[start..i]);
        out.extend_from_slice(replacement);
        start = i + 1;
    }
    out.extend_from_slice(&bytes[start..]);
    // SAFETY: replacing ASCII characters with valid ASCII entity strings preserves valid UTF-8.
    unsafe { String::from_utf8_unchecked(out) }
}

#[inline]
pub fn is_numeric(text: &str) -> bool {
    let bytes = text.as_bytes();
    if bytes.is_empty() {
        return false;
    }
    let first = bytes[0];
    let last = bytes[bytes.len() - 1];
    // Fast path: if bounded by ASCII digits/signs and ending in an ASCII digit,
    // skip trim() completely.
    let bytes = if (first.is_ascii_digit() || first == b'+' || first == b'-') && last.is_ascii_digit() {
        bytes
    } else if first < 128 && !first.is_ascii_whitespace() && !first.is_ascii_digit() && first != b'+' && first != b'-' {
        // Fast rejection: leading ASCII byte that is neither digit, sign, nor whitespace
        return false;
    } else {
        text.trim().as_bytes()
    };
    if bytes.is_empty() {
        return false;
    }
    let first = bytes[0];
    if !first.is_ascii_digit() && first != b'+' && first != b'-' {
        return false;
    }
    let mut digits = 0;
    let mut grouped = false;
    let mut fraction = false;
    for (index, &b) in bytes.iter().enumerate() {
        if index == 0 && (b == b'+' || b == b'-') {
            continue;
        }
        if b.is_ascii_digit() {
            digits += 1;
            continue;
        }
        if b == b',' {
            if fraction || digits == 0 || (if grouped { digits != 3 } else { digits > 3 }) {
                return false;
            }
            grouped = true;
            digits = 0;
        } else if b == b'.' {
            if fraction || digits == 0 || (grouped && digits != 3) {
                return false;
            }
            fraction = true;
            digits = 0;
        } else {
            return false;
        }
    }
    digits > 0 && (fraction || !grouped || digits == 3)
}

/// Writes directly to the caller's buffered sink, including very large cells.
fn escaped(text: &str, out: &mut impl Write, cancel: Cancel<'_>) -> Result<()> {
    let bytes = text.as_bytes();
    if !bytes
        .iter()
        .any(|&b| matches!(b, b'&' | b'<' | b'>' | b'"' | b'\''))
    {
        if bytes.len() <= 65536 {
            out.write_all(bytes)?;
            return Ok(());
        }
        for chunk in bytes.chunks(65536) {
            check(cancel)?;
            out.write_all(chunk)?;
        }
        return Ok(());
    }
    let mut start = 0;
    for (i, &b) in bytes.iter().enumerate() {
        if (i & 0xffff) == 0 && i > 0 {
            check(cancel)?;
        }
        let replacement: &[u8] = match b {
            b'&' => b"&amp;",
            b'<' => b"&lt;",
            b'>' => b"&gt;",
            b'"' => b"&quot;",
            b'\'' => b"&#x27;",
            _ => continue,
        };
        out.write_all(&bytes[start..i])?;
        out.write_all(replacement)?;
        start = i + 1;
    }
    out.write_all(&bytes[start..])?;
    Ok(())
}

pub fn accessible_text_color(bg: &str) -> &'static str {
    let hex = bg.trim().strip_prefix('#').unwrap_or(bg.trim());
    if hex.len() == 6 {
        if let (Ok(r), Ok(g), Ok(b)) = (
            u8::from_str_radix(&hex[0..2], 16),
            u8::from_str_radix(&hex[2..4], 16),
            u8::from_str_radix(&hex[4..6], 16),
        ) {
            let lum = 0.2126 * (r as f32) + 0.7152 * (g as f32) + 0.0722 * (b as f32);
            return if lum < 128.0 { "#ffffff" } else { "#222222" };
        }
    } else if hex.len() == 3 {
        if let (Ok(r), Ok(g), Ok(b)) = (
            u8::from_str_radix(&hex[0..1], 16),
            u8::from_str_radix(&hex[1..2], 16),
            u8::from_str_radix(&hex[2..3], 16),
        ) {
            let lum = 0.2126 * ((r * 17) as f32) + 0.7152 * ((g * 17) as f32) + 0.0722 * ((b * 17) as f32);
            return if lum < 128.0 { "#ffffff" } else { "#222222" };
        }
    }
    "#222222"
}

pub fn write(
    table: &Table,
    s: &Style,
    row_limit: Option<usize>,
    out: &mut impl Write,
    cancel: Cancel<'_>,
) -> Result<()> {
    s.validate()?;
    check(cancel)?;
    let unstyled = *s == Style::unstyled();
    out.write_all(b"<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n  <title>YACHT Table</title>\n")?;
    if !unstyled {
        let body_fg = accessible_text_color(&s.body_bg);
        let mut css = format!(".csv-table-wrap {{\n  font-family: {};\n  font-size: {}px;\n}}\n.csv-table {{\n  width: 100%;\n  border-collapse: {};\n  border-spacing: {}px;\n}}\n.csv-table th, .csv-table td {{\n  border: {}px {} {};\n  padding: {}px;\n  vertical-align: top;\n}}\n.csv-table th {{\n  background: {};\n  color: {};\n  text-align: left;\n}}\n.csv-table td {{\n  background: {};\n  color: {};\n}}\n.csv-table td.num {{\n  text-align: right;\n}}\n", if s.font_family.is_empty() { "inherit" } else { &s.font_family }, s.font_size_px, s.border_collapse, s.border_spacing_px, s.border_width_px, s.border_style, s.border_color, s.cell_padding_px, s.header_bg, s.header_text_color, s.body_bg, body_fg);
        if s.zebra_enabled {
            let zebra_fg = accessible_text_color(&s.zebra_bg);
            css.push_str(&format!(
                ".csv-table tbody tr:nth-child(even) td {{\n  background: {};\n  color: {};\n}}\n",
                s.zebra_bg, zebra_fg
            ));
        }
        if s.hover_enabled {
            let hover_fg = accessible_text_color(&s.hover_bg);
            css.push_str(&format!(
                ".csv-table tbody tr:hover td {{\n  background: {};\n  color: {};\n}}\n",
                s.hover_bg, hover_fg
            ));
        }
        out.write_all(b"  <style>\n")?;
        for line in css.lines() {
            writeln!(out, "    {line}")?;
        }
        out.write_all(b"  </style>\n")?;
    }
    out.write_all(b"</head>\n<body>\n")?;
    if unstyled {
        out.write_all(b"  <div>\n    <table>\n")?;
    } else {
        out.write_all(b"  <div class=\"csv-table-wrap\">\n    <table class=\"csv-table")?;
        if !s.table_class.is_empty() {
            out.write_all(b" ")?;
            escaped(&s.table_class, out, cancel)?;
        }
        out.write_all(b"\">\n")?;
    }
    out.write_all(b"      <thead><tr>")?;
    for (i, col) in table.header.iter().enumerate() {
        write!(out, "<th scope=\"col\" title=\"Field #{}\">", i + 1)?;
        escaped(col, out, cancel)?;
        out.write_all(b"</th>")?;
    }
    out.write_all(b"</tr></thead>\n      <tbody>\n")?;

    let header_len = table.header.len();
    for row in table.rows.iter().take(row_limit.unwrap_or(usize::MAX)) {
        check(cancel)?;
        out.write_all(b"        <tr>")?;
        for i in 0..header_len {
            let cell = row.get(i).map(String::as_str).unwrap_or("");
            out.write_all(if !unstyled && is_numeric(cell) {
                b"<td class=\"num\">"
            } else {
                b"<td>"
            })?;
            escaped(cell, out, cancel)?;
            out.write_all(b"</td>")?;
        }
        out.write_all(b"</tr>\n")?;
    }
    out.write_all(b"      </tbody>\n    </table>\n  </div>\n</body>\n</html>\n")?;
    Ok(())
}

pub fn document(
    table: &Table,
    s: &Style,
    row_limit: Option<usize>,
    cancel: Cancel<'_>,
) -> Result<String> {
    let rows_to_render = row_limit.unwrap_or(table.rows.len()).min(table.rows.len());
    let cols = table.header.len().max(1);
    let estimated =
        1024 + rows_to_render.saturating_mul(cols.saturating_mul(16).saturating_add(24));
    let mut out = Vec::with_capacity(estimated.min(128 * 1024 * 1024));
    write(table, s, row_limit, &mut out, cancel)?;
    // SAFETY: write only outputs valid UTF-8 HTML structure, escaped cell content, and validated style properties.
    Ok(unsafe { String::from_utf8_unchecked(out) })
}
