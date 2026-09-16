use crate::{invalid, Result};
use serde::Serialize;
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Table {
    pub header: Vec<String>,
    pub rows: Vec<Vec<String>>,
    pub short_row_count: usize,
    pub extra_row_count: usize,
}
impl Table {
    pub fn new(mut records: Vec<Vec<String>>) -> Result<Self> {
        if records.is_empty() {
            return Err(invalid("CSV file is empty."));
        }
        let mut header = records.remove(0);
        let original = header.len();
        let width = records
            .iter()
            .map(Vec::len)
            .max()
            .unwrap_or(0)
            .max(original)
            .max(1);
        let short_row_count = records.iter().filter(|r| r.len() < original).count();
        let extra_row_count = records.iter().filter(|r| r.len() > original).count();
        for i in original..width {
            header.push(format!("Column {}", i + 1));
        }
        Ok(Self {
            header,
            rows: records,
            short_row_count,
            extra_row_count,
        })
    }
    pub fn warnings(&self) -> Vec<String> {
        let mut out = vec![];
        if self.short_row_count > 0 {
            out.push(format!(
                "{} row(s) have missing cells; exported cells will be blank.",
                self.short_row_count
            ));
        }
        if self.extra_row_count > 0 {
            out.push(format!(
                "{} row(s) have extra cells; additional columns are preserved.",
                self.extra_row_count
            ));
        }
        out
    }
    pub fn sample() -> Self {
        Self::new(
            [
                ["Year", "Album details", "Peak chart position"],
                ["1990", "Surprise", "—"],
                ["1993", "Deluxe", "35"],
                ["1996", "Friction, Baby", "64"],
                ["1998", "How Does Your Garden Grow?", "129"],
                ["2001", "Closer", "110"],
                ["2005", "Before the Robots", "84"],
                ["2009", "Paper Empire", "62"],
                ["2014", "All Together Now", "43"],
                ["2024", "Super Magick", "—"],
            ]
            .into_iter()
            .map(|r| r.into_iter().map(str::to_owned).collect())
            .collect(),
        )
        .expect("sample")
    }
}
