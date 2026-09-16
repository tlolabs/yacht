use crate::{check, invalid, table::Table, Cancel, Result};
use serde::{Deserialize, Serialize};
use std::{
    io::{BufReader, Read},
    path::Path,
};
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
    records: Vec<Vec<String>>,
    started: bool,
    skip_lf: bool,
    previous_cr: bool,
    line: usize,
}
impl Parser {
    fn new(d: Delimiter) -> Self {
        Self {
            delimiter: d.byte(),
            state: State::Start,
            field: vec![],
            row: vec![],
            records: vec![],
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
            self.records.len() + 1,
            why
        ))
    }
    fn field(&mut self) -> Result<()> {
        let value = std::str::from_utf8(&self.field)
            .map_err(|_| self.fail("input is not valid UTF-8. Save the file as UTF-8 CSV."))?
            .to_owned();
        self.row.push(value);
        self.field.clear();
        Ok(())
    }
    fn row(&mut self) -> Result<()> {
        if self.started {
            self.field()?;
        }
        self.records.push(std::mem::take(&mut self.row));
        self.started = false;
        self.state = State::Start;
        Ok(())
    }
    fn consume(&mut self, bytes: &[u8], cancel: Cancel<'_>) -> Result<()> {
        for (index, &b) in bytes.iter().enumerate() {
            if index % 65536 == 0 {
                check(cancel)?;
            }
            if b == 0 {
                return Err(self
                    .fail("NUL bytes are unsupported. Save the file as UTF-8 CSV (not UTF-16)."));
            }
            if self.skip_lf {
                self.skip_lf = false;
                if b == b'\n' {
                    self.previous_cr = false;
                    continue;
                }
            }
            if self.state == State::Quoted {
                if b == b'"' {
                    self.state = State::AfterQuote;
                } else {
                    self.field.push(b);
                }
            } else if b == self.delimiter {
                self.field()?;
                self.state = State::Start;
                self.started = true;
            } else if b == b'\n' || b == b'\r' {
                self.row()?;
                self.skip_lf = b == b'\r';
            } else {
                match self.state {
                    State::Start => {
                        self.started = true;
                        if b == b'"' {
                            self.state = State::Quoted;
                        } else {
                            self.state = State::Unquoted;
                            self.field.push(b);
                        }
                    }
                    State::Unquoted => {
                        if b == b'"' {
                            return Err(self.fail("quote inside an unquoted field; enclose the whole field in quotes."));
                        }
                        self.field.push(b);
                    }
                    State::AfterQuote => {
                        if b != b'"' {
                            return Err(
                                self.fail("expected a delimiter or newline after closing quote.")
                            );
                        }
                        self.field.push(b);
                        self.state = State::Quoted;
                    }
                    State::Quoted => unreachable!(),
                }
            }
            if b == b'\r' || (b == b'\n' && !self.previous_cr) {
                self.line += 1;
            }
            self.previous_cr = b == b'\r';
        }
        Ok(())
    }
    fn finish(mut self) -> Result<Table> {
        if self.state == State::Quoted {
            return Err(self.fail("unterminated quoted field."));
        }
        if self.started {
            self.row()?;
        }
        Table::new(self.records)
    }
}
pub fn parse(bytes: &[u8], delimiter: Delimiter, cancel: Cancel<'_>) -> Result<Table> {
    let mut parser = Parser::new(delimiter);
    parser.consume(
        bytes.strip_prefix(&[0xef, 0xbb, 0xbf]).unwrap_or(bytes),
        cancel,
    )?;
    parser.finish()
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
    // Check the opened descriptor again to cover an ordinary path replacement.
    if !std::fs::metadata(path)?.is_file() {
        return Err(invalid("Choose a regular CSV or TSV file."));
    }
    let file = std::fs::File::open(path)?;
    if !file.metadata()?.is_file() {
        return Err(invalid("Choose a regular CSV or TSV file."));
    }
    read_stream(file, delimiter, chunk_size, cancel)
}
pub fn read_stream(
    mut reader: impl Read,
    delimiter: Delimiter,
    chunk_size: usize,
    cancel: Cancel<'_>,
) -> Result<Table> {
    if chunk_size == 0 {
        return Err(invalid("CSV read buffer must be positive."));
    }
    let mut parser = Parser::new(delimiter);
    let mut prefix = vec![];
    reader.by_ref().take(3).read_to_end(&mut prefix)?;
    if prefix != [0xef, 0xbb, 0xbf] {
        parser.consume(&prefix, cancel)?;
    }
    let mut reader = BufReader::with_capacity(chunk_size.min(1024 * 1024), reader);
    let mut buffer = vec![0; chunk_size.min(1024 * 1024)];
    loop {
        check(cancel)?;
        let n = reader.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        parser.consume(&buffer[..n], cancel)?;
    }
    parser.finish()
}
