use crate::{check, invalid, table::Table, Cancel, Result};
use serde::{Deserialize, Serialize};
use std::{io::Read, path::Path};
#[derive(Debug, Clone, Copy, Default, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Delimiter {
    #[default]
    Comma,
    Tab,
    Semicolon,
    Pipe,
}
impl Delimiter {
    pub fn byte(self) -> u8 {
        match self {
            Self::Comma => b',',
            Self::Tab => b'\t',
            Self::Semicolon => b';',
            Self::Pipe => b'|',
        }
    }
    pub fn for_path(self, path: &Path) -> Self {
        if path
            .extension()
            .is_some_and(|x| x.eq_ignore_ascii_case("tsv"))
        {
            Self::Tab
        } else {
            self
        }
    }
}
#[derive(PartialEq)]
enum State {
    Start,
    Unquoted,
    Quoted,
    AfterQuote,
}
struct Parser {
    delimiter: u8,
    state: State,
    field: Vec<u8>,
    row: Vec<String>,
    header: Option<Vec<String>>,
    records: Vec<Vec<String>>,
    started: bool,
    skip_lf: bool,
    previous_cr: bool,
    line: usize,
}
impl Parser {
    fn new(d: Delimiter, row_cap_hint: usize) -> Self {
        Self {
            delimiter: d.byte(),
            state: State::Start,
            field: Vec::with_capacity(64),
            row: Vec::with_capacity(8),
            header: None,
            records: Vec::with_capacity(row_cap_hint),
            started: false,
            skip_lf: false,
            previous_cr: false,
            line: 1,
        }
    }
    fn fail(&self, why: &str) -> crate::Error {
        invalid(format!(
            "Malformed CSV at line {}, record {}: {}",
            self.line,
            self.records.len() + if self.header.is_some() { 2 } else { 1 },
            why
        ))
    }
    fn field(&mut self) -> Result<()> {
        let value = if self.field.is_empty() {
            String::new()
        } else {
            std::str::from_utf8(&self.field)
                .map_err(|_| self.fail("input is not valid UTF-8. Save the file as UTF-8 CSV."))?
                .to_owned()
        };
        self.row.push(value);
        self.field.clear();
        Ok(())
    }
    fn row(&mut self) -> Result<()> {
        if self.started {
            self.field()?;
        }
        let row = if self.row.is_empty() {
            // Blank records need no cell storage. Reuse the scratch vector for
            // the next nonblank record instead of retaining it in an empty row.
            Vec::new()
        } else {
            let cap = self.row.len();
            std::mem::replace(&mut self.row, Vec::with_capacity(cap))
        };
        if self.header.is_none() {
            self.header = Some(row);
        } else {
            self.records.push(row);
        }
        self.started = false;
        self.state = State::Start;
        Ok(())
    }
    fn consume(&mut self, bytes: &[u8], cancel: Cancel<'_>) -> Result<()> {
        let mut i = 0;
        let len = bytes.len();
        let mut next_cancel = 65536;

        let mut is_special_unquoted = [false; 256];
        is_special_unquoted[0] = true;
        is_special_unquoted[b'"' as usize] = true;
        is_special_unquoted[self.delimiter as usize] = true;
        is_special_unquoted[b'\n' as usize] = true;
        is_special_unquoted[b'\r' as usize] = true;

        let mut is_special_quoted = [false; 256];
        is_special_quoted[0] = true;
        is_special_quoted[b'"' as usize] = true;
        is_special_quoted[b'\n' as usize] = true;
        is_special_quoted[b'\r' as usize] = true;

        while i < len {
            if i >= next_cancel {
                check(cancel)?;
                next_cancel = i.saturating_add(65536);
            }
            let b = bytes[i];
            if b == 0 {
                return Err(self
                    .fail("NUL bytes are unsupported. Save the file as UTF-8 CSV (not UTF-16)."));
            }
            if self.skip_lf {
                self.skip_lf = false;
                if b == b'\n' {
                    self.previous_cr = false;
                    i += 1;
                    continue;
                }
            }
            if self.state == State::Quoted {
                if b == b'"' {
                    self.state = State::AfterQuote;
                    i += 1;
                } else if b == b'\r' || b == b'\n' {
                    self.field.push(b);
                    if b == b'\r' || !self.previous_cr {
                        self.line += 1;
                    }
                    self.previous_cr = b == b'\r';
                    i += 1;
                } else {
                    let start = i;
                    let end_bound = (i + 65536).min(len);
                    while i < end_bound && !is_special_quoted[bytes[i] as usize] {
                        i += 1;
                    }
                    if i > start {
                        check(cancel)?;
                        self.field.extend_from_slice(&bytes[start..i]);
                        self.previous_cr = false;
                    }
                }
            } else if b == self.delimiter {
                self.field()?;
                self.state = State::Start;
                self.started = true;
                self.previous_cr = false;
                i += 1;
            } else if b == b'\n' || b == b'\r' {
                self.row()?;
                self.skip_lf = b == b'\r';
                self.line += 1;
                self.previous_cr = b == b'\r';
                i += 1;
            } else {
                match self.state {
                    State::Start => {
                        self.started = true;
                        if b == b'"' {
                            self.state = State::Quoted;
                            i += 1;
                        } else {
                            self.state = State::Unquoted;
                            let start = i;
                            let end_bound = (i + 65536).min(len);
                            while i < end_bound && !is_special_unquoted[bytes[i] as usize] {
                                i += 1;
                            }
                            if i > start {
                                check(cancel)?;
                                self.field.extend_from_slice(&bytes[start..i]);
                                self.previous_cr = false;
                            }
                        }
                    }
                    State::Unquoted => {
                        if b == b'"' {
                            return Err(self.fail("quote inside an unquoted field; enclose the whole field in quotes."));
                        }
                        let start = i;
                        let end_bound = (i + 65536).min(len);
                        while i < end_bound && !is_special_unquoted[bytes[i] as usize] {
                            i += 1;
                        }
                        if i > start {
                            check(cancel)?;
                            self.field.extend_from_slice(&bytes[start..i]);
                            self.previous_cr = false;
                        }
                    }
                    State::AfterQuote => {
                        if b != b'"' {
                            return Err(
                                self.fail("expected a delimiter or newline after closing quote.")
                            );
                        }
                        self.field.push(b);
                        self.state = State::Quoted;
                        self.previous_cr = false;
                        i += 1;
                    }
                    State::Quoted => unreachable!(),
                }
            }
        }
        Ok(())
    }
    fn finish(mut self, cancel: Cancel<'_>) -> Result<Table> {
        check(cancel)?;
        if self.state == State::Quoted {
            return Err(self.fail("unterminated quoted field."));
        }
        if self.started {
            self.row()?;
        }
        let Some(header) = self.header else {
            return Err(invalid("CSV file is empty."));
        };
        check(cancel)?;
        Table::from_parts(header, self.records)
    }
}
pub fn parse(bytes: &[u8], delimiter: Delimiter, cancel: Cancel<'_>) -> Result<Table> {
    let unbom = bytes.strip_prefix(&[0xef, 0xbb, 0xbf]).unwrap_or(bytes);
    let row_cap_hint = (unbom.len() / 32).clamp(16, 500_000);
    let mut parser = Parser::new(delimiter, row_cap_hint);
    parser.consume(unbom, cancel)?;
    parser.finish(cancel)
}
pub fn read(
    path: &Path,
    delimiter: Delimiter,
    chunk_size: usize,
    cancel: Cancel<'_>,
) -> Result<Table> {
    check(cancel)?;
    if chunk_size == 0 {
        return Err(invalid("CSV read buffer must be positive."));
    }
    // Reject special files before opening: opening a FIFO can block indefinitely.
    // Check the opened descriptor again with O_NONBLOCK protection against TOCTOU path replacement.
    if !std::fs::metadata(path)?.is_file() {
        return Err(invalid("Choose a regular CSV or TSV file."));
    }
    let file = open_regular_file(path)?;
    let metadata = file.metadata()?;
    let hint = (metadata.len() / 32).clamp(16, 500_000) as usize;
    read_stream_with_hint(file, delimiter, chunk_size, hint, cancel)
}

#[cfg(unix)]
fn open_regular_file(path: &Path) -> Result<std::fs::File> {
    use std::os::unix::fs::OpenOptionsExt;
    use std::os::unix::io::AsRawFd;

    // On Unix, opening a FIFO in read-only mode blocks until a writer connects.
    // Opening with O_NONBLOCK prevents hanging if the path is replaced with a FIFO.
    let mut options = std::fs::OpenOptions::new();
    options.read(true);
    options.custom_flags(libc::O_NONBLOCK);
    let file = options.open(path)?;
    let metadata = file.metadata()?;
    if !metadata.is_file() {
        return Err(invalid("Choose a regular CSV or TSV file."));
    }
    // Restore blocking mode so downstream operations behave normally
    unsafe {
        let fd = file.as_raw_fd();
        let flags = libc::fcntl(fd, libc::F_GETFL);
        if flags != -1 {
            libc::fcntl(fd, libc::F_SETFL, flags & !libc::O_NONBLOCK);
        }
    }
    Ok(file)
}

#[cfg(not(unix))]
fn open_regular_file(path: &Path) -> Result<std::fs::File> {
    let file = std::fs::File::open(path)?;
    let metadata = file.metadata()?;
    if !metadata.is_file() {
        return Err(invalid("Choose a regular CSV or TSV file."));
    }
    Ok(file)
}

pub fn read_stream(
    reader: impl Read,
    delimiter: Delimiter,
    chunk_size: usize,
    cancel: Cancel<'_>,
) -> Result<Table> {
    read_stream_with_hint(reader, delimiter, chunk_size, 0, cancel)
}
fn read_stream_with_hint(
    mut reader: impl Read,
    delimiter: Delimiter,
    chunk_size: usize,
    hint: usize,
    cancel: Cancel<'_>,
) -> Result<Table> {
    if chunk_size == 0 {
        return Err(invalid("CSV read buffer must be positive."));
    }
    let mut parser = Parser::new(delimiter, hint);
    let mut prefix = [0u8; 3];
    let mut prefix_len = 0;
    while prefix_len < 3 {
        let n = reader.read(&mut prefix[prefix_len..])?;
        if n == 0 {
            break;
        }
        prefix_len += n;
    }
    if prefix_len < 3 || prefix != [0xef, 0xbb, 0xbf] {
        parser.consume(&prefix[..prefix_len], cancel)?;
    }
    let mut buffer = vec![0; chunk_size.min(1024 * 1024)];
    loop {
        check(cancel)?;
        let n = reader.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        parser.consume(&buffer[..n], cancel)?;
    }
    parser.finish(cancel)
}
