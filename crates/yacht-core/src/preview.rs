use crate::{check, html, style::Style, table::Table, Cancel, Result};
use serde::Serialize;
use std::io::{self, Write};
pub const PREVIEW_BUDGET: usize = 2_000_000;
pub const SOURCE_BUDGET: usize = 1_000_000;
#[derive(Debug, Serialize)]
pub struct Preview {
    pub html: String,
    pub source: String,
    pub row_count: usize,
    pub source_truncated: bool,
    pub preview_unavailable: bool,
}
struct Excerpt {
    bytes: Vec<u8>,
    truncated: bool,
    budget: usize,
}
impl Write for Excerpt {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        let n = bytes.len().min(self.budget - self.bytes.len());
        self.bytes.extend_from_slice(&bytes[..n]);
        if n < bytes.len() {
            self.truncated = true;
            return Err(io::Error::other("excerpt complete"));
        }
        Ok(n)
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
impl Excerpt {
    fn text(mut self) -> String {
        // A sink may stop within a scalar. Discard only its incomplete suffix.
        if let Err(e) = std::str::from_utf8(&self.bytes) {
            self.bytes.truncate(e.valid_up_to());
        }
        String::from_utf8(self.bytes).expect("UTF-8 prefix")
    }
}
pub fn generate(table: &Table, style: &Style, limit: usize, cancel: Cancel<'_>) -> Result<Preview> {
    style.validate()?;
    check(cancel)?;
    let mut cost = table
        .header
        .iter()
        .map(|s| s.len().saturating_mul(6).saturating_add(100))
        .fold(0usize, usize::saturating_add);
    let mut count = 0;
    for row in table.rows.iter().take(limit) {
        check(cancel)?;
        let next = row.iter().map(|s| s.len().saturating_mul(6)).fold(
            table.header.len().saturating_mul(100),
            usize::saturating_add,
        );
        if cost.saturating_add(next) > PREVIEW_BUDGET {
            break;
        }
        cost += next;
        count += 1;
    }
    let mut unavailable = cost > PREVIEW_BUDGET || (count == 0 && !table.rows.is_empty());
    let mut html = String::new();
    if !unavailable {
        let mut sink = Excerpt {
            bytes: vec![],
            truncated: false,
            budget: PREVIEW_BUDGET,
        };
        let result = html::write(table, style, Some(count), &mut sink, cancel);
        if sink.truncated {
            unavailable = true;
        } else {
            result?;
            html = sink.text();
        }
    }
    let mut sink = Excerpt {
        bytes: vec![],
        truncated: false,
        budget: SOURCE_BUDGET,
    };
    let result = html::write(table, style, None, &mut sink, cancel);
    let truncated = sink.truncated;
    if !truncated {
        result?;
    }
    Ok(Preview {
        html,
        source: sink.text(),
        row_count: count,
        source_truncated: truncated,
        preview_unavailable: unavailable,
    })
}
