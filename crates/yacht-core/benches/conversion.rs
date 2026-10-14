use std::{hint::black_box, io::Write, time::Instant};
use yacht_core::{api::Engine, csv, files, html, preview, style::Style};

fn measure<T>(name: &str, warmup: usize, iters: usize, mut f: impl FnMut() -> T) -> T {
    for _ in 0..warmup {
        black_box(f());
    }
    let mut durations = Vec::with_capacity(iters);
    let mut last_result = None;
    for _ in 0..iters {
        let start = Instant::now();
        let res = black_box(f());
        durations.push(start.elapsed());
        last_result = Some(res);
    }
    durations.sort();
    let min = durations.first().copied().unwrap_or_default();
    let max = durations.last().copied().unwrap_or_default();
    let median = durations[durations.len() / 2];
    println!("{name}: {median:?} (min: {min:?}, max: {max:?})");
    last_result.expect("at least one iteration")
}

fn main() {
    // 1. Simple 3-col 100k rows parse
    let input = format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(100_000));
    let table = measure("parse 100k rows (simple)", 2, 7, || {
        csv::parse(input.as_bytes(), Default::default(), &|| false).unwrap()
    });

    // 2. Quoted 6-col 50k rows parse
    let quoted_input = format!(
        "Name,Address,Notes,Amount,Status,Tag\n{}",
        "\"Smith, John\",\"123 Main St, Apt 4\",\"Special & quoted \"\"note\"\"\",1234.56,\"Active\",urgent\n".repeat(50_000)
    );
    let _quoted_table = measure("parse 50k rows (quoted 6-col)", 2, 7, || {
        csv::parse(quoted_input.as_bytes(), Default::default(), &|| false).unwrap()
    });

    // 3. Disk read 100k rows
    let mut tmp = tempfile::NamedTempFile::new().unwrap();
    tmp.write_all(input.as_bytes()).unwrap();
    tmp.flush().unwrap();
    let _read_table = measure("read 100k rows from disk", 2, 7, || {
        csv::read(tmp.path(), Default::default(), 65536, &|| false).unwrap()
    });

    // 4. Bounded preview and source on 100k rows
    let style = Style::default();
    measure("bounded preview and source (100k rows)", 2, 7, || {
        preview::generate(&table, &style, 200, &|| false).unwrap()
    });

    // 5. Bounded preview on small table (50 rows)
    let small_input = format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(50));
    let small_table = csv::parse(small_input.as_bytes(), Default::default(), &|| false).unwrap();
    measure("bounded preview and source (50 rows)", 3, 10, || {
        preview::generate(&small_table, &style, 200, &|| false).unwrap()
    });

    // 6. Stream 100k rows to sink
    measure("stream 100k rows", 2, 7, || {
        html::write(&table, &style, None, &mut std::io::sink(), &|| false).unwrap()
    });

    // 7. Full document HTML string generation
    let mut doc_len = 0;
    measure("document 100k rows (string alloc)", 2, 7, || {
        let doc = html::document(&table, &style, None, &|| false).unwrap();
        doc_len = doc.len();
        doc
    });
    println!("  (document output size: {doc_len} bytes)");

    // 8. Atomic file export of 100k rows
    measure("export 100k rows to file (atomic)", 2, 5, || {
        let out_tmp = tempfile::NamedTempFile::new().unwrap();
        files::export(&table, &style, out_tmp.path(), true, &|| false).unwrap()
    });

    // 9. Style validation (10,000 runs)
    measure("validate style (10,000 runs)", 2, 7, || {
        for _ in 0..10_000 {
            black_box(style.validate().unwrap());
        }
    });

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
    measure("engine dispatch preview 100x", 3, 10, || {
        for _ in 0..100 {
            black_box(engine.dispatch(&req_bytes, &|| false));
        }
    });
}
