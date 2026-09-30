use std::{hint::black_box, io::Write, time::Instant};
use yacht_core::{api::Engine, csv, files, html, preview, style::Style};

fn main() {
    // 1. Simple 3-col 100k rows parse
    let input = format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(100_000));
    let start = Instant::now();
    let table = csv::parse(black_box(input.as_bytes()), Default::default(), &|| false).unwrap();
    println!("parse 100k rows (simple): {:?}", start.elapsed());

    // 2. Quoted 6-col 50k rows parse
    let quoted_input = format!(
        "Name,Address,Notes,Amount,Status,Tag\n{}",
        "\"Smith, John\",\"123 Main St, Apt 4\",\"Special & quoted \"\"note\"\"\",1234.56,\"Active\",urgent\n".repeat(50_000)
    );
    let start = Instant::now();
    let _quoted_table = csv::parse(
        black_box(quoted_input.as_bytes()),
        Default::default(),
        &|| false,
    )
    .unwrap();
    println!("parse 50k rows (quoted 6-col): {:?}", start.elapsed());

    // 3. Disk read 100k rows
    let mut tmp = tempfile::NamedTempFile::new().unwrap();
    tmp.write_all(input.as_bytes()).unwrap();
    tmp.flush().unwrap();
    let start = Instant::now();
    let _read_table = csv::read(tmp.path(), Default::default(), 65536, &|| false).unwrap();
    println!("read 100k rows from disk: {:?}", start.elapsed());

    // 4. Bounded preview and source on 100k rows
    let style = Style::default();
    let start = Instant::now();
    black_box(preview::generate(&table, &style, 200, &|| false).unwrap());
    println!(
        "bounded preview and source (100k rows): {:?}",
        start.elapsed()
    );

    // 5. Bounded preview on small table (50 rows)
    let small_input = format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(50));
    let small_table = csv::parse(small_input.as_bytes(), Default::default(), &|| false).unwrap();
    let start = Instant::now();
    black_box(preview::generate(&small_table, &style, 200, &|| false).unwrap());
    println!(
        "bounded preview and source (50 rows): {:?}",
        start.elapsed()
    );

    // 6. Stream 100k rows to sink
    let start = Instant::now();
    html::write(&table, &style, None, &mut std::io::sink(), &|| false).unwrap();
    println!("stream 100k rows: {:?}", start.elapsed());

    // 7. Full document HTML string generation
    let start = Instant::now();
    let doc = html::document(&table, &style, None, &|| false).unwrap();
    println!(
        "document 100k rows (string alloc): {:?} (len: {} bytes)",
        start.elapsed(),
        doc.len()
    );

    // 8. Atomic file export of 100k rows
    let out_tmp = tempfile::NamedTempFile::new().unwrap();
    let start = Instant::now();
    files::export(&table, &style, out_tmp.path(), true, &|| false).unwrap();
    println!("export 100k rows to file (atomic): {:?}", start.elapsed());

    // 9. Style validation (10,000 iterations)
    let start = Instant::now();
    for _ in 0..10_000 {
        let res = style.validate();
        black_box(res).unwrap();
    }
    println!("validate style (10,000 runs): {:?}", start.elapsed());

    // 10. Engine API dispatch preview round trip
    let engine = Engine::default();
    let inserted = engine
        .request(
            serde_json::json!({"version": 1, "op": "records", "records": small_table.rows}),
            &|| false,
        )
        .unwrap();
    let handle = inserted["handle"].as_u64().unwrap();
    let req = serde_json::json!({"version": 1, "op": "preview", "handle": handle, "style": serde_json::to_value(&style).unwrap()});
    let req_bytes = serde_json::to_vec(&req).unwrap();
    let start = Instant::now();
    for _ in 0..100 {
        black_box(engine.dispatch(&req_bytes, &|| false));
    }
    println!("engine dispatch preview 100x: {:?}", start.elapsed());
}
