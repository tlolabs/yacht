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
        let remaining = self.budget.saturating_sub(self.bytes.len());
        let n = bytes.len().min(remaining);
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
        // SAFETY: self.bytes has been verified and truncated to valid_up_to().
        unsafe { String::from_utf8_unchecked(self.bytes) }
    }
}

/// Keep complete rows within the actual rendered-byte budget.
/// The HTML writers emit each body-row closing tag as one structural write;
/// cell text is escaped, so it cannot be mistaken for this marker.
struct PreviewExcerpt {
    excerpt: Excerpt,
    footer: &'static [u8],
    row_count: usize,
    row_end: usize,
}
impl Write for PreviewExcerpt {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        let n = self.excerpt.write(bytes)?;
        if bytes == b"</tr>\n"
            && self.excerpt.bytes.len() + self.footer.len() <= self.excerpt.budget
        {
            self.row_count += 1;
            self.row_end = self.excerpt.bytes.len();
        }
        Ok(n)
    }
    fn flush(&mut self) -> io::Result<()> {
        self.excerpt.flush()
    }
}
pub fn generate(table: &Table, style: &Style, limit: usize, cancel: Cancel<'_>) -> Result<Preview> {
    generate_with_mode(table, style, limit, html::OutputMode::Document, cancel)
}
pub fn generate_with_mode(
    table: &Table,
    style: &Style,
    limit: usize,
    mode: html::OutputMode,
    cancel: Cancel<'_>,
) -> Result<Preview> {
    generate_with_renderer(table, style, limit, mode, false, cancel)
}

/// Static native renderers can request inline cell styles instead of CSS selectors.
/// The source excerpt continues to use the user's independently selected output mode.
pub fn generate_with_renderer(
    table: &Table,
    style: &Style,
    limit: usize,
    mode: html::OutputMode,
    inline: bool,
    cancel: Cancel<'_>,
) -> Result<Preview> {
    style.validate()?;
    check(cancel)?;
    let mut count = limit.min(table.rows.len());
    // An allocation hint only: escaping and inline styles vary by cell. The
    // bounded sink determines how many complete rows actually fit, without an
    // extra pass over every cell or rejecting text that needs little escaping.
    let cost = table
        .header
        .len()
        .saturating_mul(count.saturating_add(1))
        .saturating_mul(100);
    let mut unavailable = count == 0 && !table.rows.is_empty();
    let mut html = String::new();
    let render_mode = if inline {
        html::OutputMode::Table
    } else {
        html::OutputMode::Document
    };
    let footer: &[u8] = if inline {
        b"  </tbody>\n</table>\n"
    } else {
        b"      </tbody>\n    </table>\n  </div>\n</body>\n</html>\n"
    };
    if !unavailable {
        let mut sink = PreviewExcerpt {
            excerpt: Excerpt {
                bytes: Vec::with_capacity(cost.clamp(1024, PREVIEW_BUDGET)),
                truncated: false,
                budget: PREVIEW_BUDGET,
            },
            footer,
            row_count: 0,
            row_end: 0,
        };
        let result = html::write_output(table, style, Some(count), render_mode, &mut sink, cancel);
        if let Err(crate::Error::Cancelled) = result {
            return Err(crate::Error::Cancelled);
        }
        if sink.excerpt.truncated {
            if sink.row_count == 0 {
                unavailable = true;
                count = 0;
            } else {
                // Drop the partial row and reuse the rows already rendered,
                // rather than rendering the same large table again at a lower limit.
                count = sink.row_count;
                sink.excerpt.bytes.truncate(sink.row_end);
                sink.excerpt.bytes.extend_from_slice(sink.footer);
                html = sink.excerpt.text();
            }
        } else {
            result?;
            html = sink.excerpt.text();
        }
    }
    let (source, truncated) = if mode == render_mode
        && !unavailable
        && (table.rows.len() <= count || html.len().saturating_sub(footer.len()) >= SOURCE_BUDGET)
    {
        if html.len() <= SOURCE_BUDGET {
            (html.clone(), false)
        } else {
            let mut bytes = html.as_bytes()[..SOURCE_BUDGET].to_vec();
            if let Err(e) = std::str::from_utf8(&bytes) {
                bytes.truncate(e.valid_up_to());
            }
            (unsafe { String::from_utf8_unchecked(bytes) }, true)
        }
    } else {
        let mut sink = Excerpt {
            bytes: Vec::with_capacity(SOURCE_BUDGET.min(cost.saturating_mul(4)).max(65536)),
            truncated: false,
            budget: SOURCE_BUDGET,
        };
        let result = html::write_output(table, style, None, mode, &mut sink, cancel);
        if let Err(crate::Error::Cancelled) = result {
            return Err(crate::Error::Cancelled);
        }
        let truncated = sink.truncated;
        if !truncated {
            result?;
        }
        (sink.text(), truncated)
    };
    check(cancel)?;
    Ok(Preview {
        html,
        source,
        row_count: count,
        source_truncated: truncated,
        preview_unavailable: unavailable,
    })
}
