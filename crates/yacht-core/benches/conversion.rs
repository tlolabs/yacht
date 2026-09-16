use std::{hint::black_box, time::Instant};
use yacht_core::{csv, html, preview, style::Style};
fn main() {
    let input = format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(100_000));
    let start = Instant::now();
    let table = csv::parse(black_box(input.as_bytes()), Default::default(), &|| false).unwrap();
    println!("parse 100k rows: {:?}", start.elapsed());
    let style = Style::default();
    let start = Instant::now();
    black_box(preview::generate(&table, &style, 200, &|| false).unwrap());
    println!("bounded preview and source: {:?}", start.elapsed());
    let start = Instant::now();
    html::write(&table, &style, None, &mut std::io::sink(), &|| false).unwrap();
    println!("stream 100k rows: {:?}", start.elapsed());
}
