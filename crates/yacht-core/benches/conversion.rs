use std::{env, hint::black_box, io::Write, mem::size_of, time::Instant};
use yacht_core::{api::Engine, csv, files, html, preview, style::Style};

struct Measurement {
    warmups: usize,
    samples: usize,
    filter: String,
}
impl Measurement {
    fn from_env() -> Self {
        let count = |key: &str, default: usize| {
            env::var(key)
                .map(|value| {
                    value
                        .parse::<usize>()
                        .expect("benchmark count must be an integer")
                })
                .unwrap_or(default)
        };
        let samples = count("YACHT_BENCH_SAMPLES", 25);
        assert!(samples > 0, "YACHT_BENCH_SAMPLES must be positive");
        Self {
            warmups: count("YACHT_BENCH_WARMUPS", 3),
            samples,
            filter: env::var("YACHT_BENCH_FILTER").unwrap_or_default(),
        }
    }

    fn measure<T>(&self, name: &str, mut f: impl FnMut() -> T) {
        if !name.contains(&self.filter) {
            return;
        }
        for _ in 0..self.warmups {
            drop(black_box(f()));
        }
        let mut raw_ns = Vec::with_capacity(self.samples);
        for _ in 0..self.samples {
            let start = Instant::now();
            let result = black_box(f());
            raw_ns.push(start.elapsed().as_nanos());
            // Each sample includes construction/allocation, but destruction is
            // outside its timer. No prior result survives into the next sample.
            drop(result);
        }
        let mut sorted = raw_ns.clone();
        sorted.sort_unstable();
        let median_ns = sorted[sorted.len() / 2];
        let p95_ns = sorted[(sorted.len() * 95).div_ceil(100) - 1];
        println!(
            "{}",
            serde_json::json!({
                "kind": "measurement", "name": name, "unit": "ns",
                "warmups": self.warmups, "samples": self.samples,
                "median_ns": median_ns, "p95_ns": p95_ns,
                "min_ns": sorted[0], "max_ns": sorted[sorted.len() - 1],
                "raw_ns": raw_ns,
            })
        );
    }
}

struct Fixture {
    name: &'static str,
    definition: &'static str,
    bytes: Vec<u8>,
    delimiter: csv::Delimiter,
    rows: Option<usize>,
}
impl Fixture {
    fn csv(name: &'static str, definition: &'static str, text: String, rows: usize) -> Self {
        Self {
            name,
            definition,
            bytes: text.into_bytes(),
            delimiter: csv::Delimiter::Comma,
            rows: Some(rows),
        }
    }
}

fn fixtures() -> Vec<Fixture> {
    let mut out = vec![
        Fixture::csv(
            "100k rows (simple)",
            "A,B,C LF header; 100000 copies of 123456,Café,🛥 LF",
            format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(100_000)),
            100_000,
        ),
        Fixture::csv(
            "50 rows (small)",
            "A,B,C LF header; 50 copies of 123456,Café,🛥 LF",
            format!("A,B,C\n{}", "123456,Café,🛥\n".repeat(50)),
            50,
        ),
        Fixture::csv(
            "100k rows (single column)",
            "A LF header; 100000 copies of 123456 LF",
            format!("A\n{}", "123456\n".repeat(100_000)),
            100_000,
        ),
        Fixture::csv(
            "100k rows (blank)",
            "A LF header; 100000 blank LF records",
            format!("A\n{}", "\n".repeat(100_000)),
            100_000,
        ),
    ];
    for (name, rows) in [
        ("10k rows (typical quoted 6-col)", 10_000),
        ("50k rows (quoted 6-col)", 50_000),
    ] {
        out.push(Fixture::csv(
            name,
            "six columns; repeated quoted delimiter/doubled quote, ASCII number, text; LF",
            format!(
                "Name,Address,Notes,Amount,Status,Tag\n{}",
                "\"Smith, John\",\"123 Main St, Apt 4\",\"Special & quoted \"\"note\"\"\",1234.56,\"Active\",urgent\n".repeat(rows)
            ),
            rows,
        ));
    }
    let header = (0..256)
        .map(|i| format!("Column {i}"))
        .collect::<Vec<_>>()
        .join(",");
    out.push(Fixture::csv(
        "2k rows (wide 256-col)",
        "256 named columns; 2000 rows of 256 numeric cells (1234)",
        format!(
            "{header}\n{}",
            format!("{}\n", vec!["1234"; 256].join(",")).repeat(2_000)
        ),
        2_000,
    ));
    out.push(Fixture::csv(
        "2k rows (sparse 256-col)",
        "256 named columns; 1000 repetitions of a two-cell row then a blank record",
        format!("{header}\n{}", "Café,123\n\n".repeat(1_000)),
        2_000,
    ));
    let alternating_header = (0..128)
        .map(|i| format!("Column {i}"))
        .collect::<Vec<_>>()
        .join(",");
    let wide_row = vec!["1234"; 128].join(",");
    out.push(Fixture::csv(
        "2k rows (alternating 1 and 128-col)",
        "128 named columns; 1000 repetitions of one-cell Café row then 128 numeric cells (1234)",
        format!(
            "{alternating_header}\n{}",
            format!("Café\n{wide_row}\n").repeat(1_000)
        ),
        2_000,
    ));
    out.push(Fixture::csv(
        "16 rows (long Unicode multiline)",
        "one column; 16 quoted cells, each 8192 copies of 漢字,<&>é🛥️ CRLF",
        format!(
            "Text\n{}",
            format!("\"{}\"\n", "漢字,<&>é🛥️\r\n".repeat(8_192)).repeat(16)
        ),
        16,
    ));
    for (name, delimiter) in [
        ("1k rows (TSV multiline)", csv::Delimiter::Tab),
        ("1k rows (semicolon multiline)", csv::Delimiter::Semicolon),
        ("1k rows (pipe multiline)", csv::Delimiter::Pipe),
    ] {
        let sep = char::from(delimiter.byte());
        out.push(Fixture {
            name,
            definition: "BOM; two columns; 1000 quoted separator/CRLF/doubled-quote Unicode rows",
            bytes: format!(
                "\u{feff}名前{sep}Notes\r\n{}",
                format!("\"Café{sep}🛥\"{sep}\"line 1\r\nline \"\"2\"\"\"\r\n").repeat(1_000)
            )
            .into_bytes(),
            delimiter,
            rows: Some(1_000),
        });
    }
    for (name, bytes) in [
        ("malformed empty", b"".as_slice()),
        ("malformed NUL", b"A,B\n1,\0\n".as_slice()),
        ("malformed UTF-8", b"A,B\n1,\xff\n".as_slice()),
        ("malformed unterminated", b"A,B\n1,\"open".as_slice()),
        (
            "malformed unquoted quote",
            b"A,B\n1,bad\"quote\n".as_slice(),
        ),
        (
            "malformed after quote",
            b"A,B\n1,\"closed\"oops\n".as_slice(),
        ),
    ] {
        out.push(Fixture {
            name,
            definition: "literal synthetic malformed input (see conversion.rs)",
            bytes: bytes.to_vec(),
            delimiter: csv::Delimiter::Comma,
            rows: None,
        });
    }
    out
}

fn report_engine_case(
    engine: &Engine,
    name: &str,
    request: &serde_json::Value,
    request_bytes: &[u8],
) {
    // Equality/size checks are untimed; caller-side response decoding is not
    // included in the full JSON dispatch benchmark below.
    let expected = engine.request(request.clone(), &|| false).unwrap();
    let response = engine.dispatch(request_bytes, &|| false);
    let decoded: serde_json::Value = serde_json::from_slice(&response).unwrap();
    assert_eq!(decoded.get("ok"), Some(&expected));
    let payload_text_bytes = expected.as_str().map(str::len).unwrap_or_else(|| {
        ["html", "source"]
            .iter()
            .filter_map(|key| expected[*key].as_str())
            .map(str::len)
            .sum()
    });
    let string_capacity = |value: &serde_json::Value| match value {
        serde_json::Value::String(text) => text.capacity(),
        _ => 0,
    };
    let payload_text_capacity_bytes = if expected.is_string() {
        string_capacity(&expected)
    } else {
        ["html", "source"]
            .iter()
            .map(|key| string_capacity(&expected[*key]))
            .sum()
    };
    println!(
        "{}",
        serde_json::json!({"kind": "engine_case", "name": name,
            "request_bytes": request_bytes.len(), "response_bytes": response.len(),
            "response_capacity_bytes": response.capacity(),
            "payload_text_capacity_bytes": payload_text_capacity_bytes,
            "payload_text_bytes": payload_text_bytes, "payloads_equal": true,
            "request_scope": "micro request: small request Value clone, execution and result Value; no JSON transport",
            "dispatch_scope": "full JSON dispatch: request parsing, execution, result serialization and envelope; caller decoding excluded"})
    );
}

fn main() {
    let measurement = Measurement::from_env();
    println!(
        "{}",
        serde_json::json!({
            "kind": "configuration", "samples": measurement.samples,
            "warmups": measurement.warmups, "filter": measurement.filter,
            "timing": "allocation/construction included; result destruction excluded; prior sample dropped",
            "cache": "in-memory inputs; warmed file reads after creation; OS caches not flushed",
            "profile": "Cargo bench/release; workspace profile.release",
        })
    );
    let fixtures = fixtures();
    for fixture in &fixtures {
        let result = csv::parse(&fixture.bytes, fixture.delimiter, &|| false);
        let metadata = match (fixture.rows, result) {
            (Some(rows), Ok(table)) => {
                assert_eq!(table.rows.len(), rows);
                serde_json::json!({"rows": rows, "columns": table.header.len(),
                    "short_rows": table.short_row_count, "extra_rows": table.extra_row_count,
                    // Retained Vec storage only: excludes allocator bookkeeping
                    // and every String's separately allocated UTF-8 payload.
                    "body_cell_descriptor_capacity_bytes": table.rows.iter()
                        .map(|row| row.capacity() * size_of::<String>()).sum::<usize>(),
                    "body_cell_descriptor_logical_bytes": table.rows.iter()
                        .map(|row| row.len() * size_of::<String>()).sum::<usize>(),
                    "body_row_descriptor_capacity_bytes": table.rows.capacity() * size_of::<Vec<String>>(),
                    "body_row_descriptor_logical_bytes": table.rows.len() * size_of::<Vec<String>>()})
            }
            (None, Err(error)) => serde_json::json!({"error": error.to_string()}),
            _ => panic!("unexpected fixture result: {}", fixture.name),
        };
        println!(
            "{}",
            serde_json::json!({"kind": "fixture", "name": fixture.name,
            "definition": fixture.definition, "input_bytes": fixture.bytes.len(),
            "delimiter_byte": fixture.delimiter.byte(), "result": metadata})
        );
        let name = format!("parse {}", fixture.name);
        if fixture.rows.is_some() {
            measurement.measure(&name, || {
                csv::parse(black_box(&fixture.bytes), fixture.delimiter, &|| false).unwrap()
            });
        } else {
            measurement.measure(&name, || {
                csv::parse(black_box(&fixture.bytes), fixture.delimiter, &|| false).unwrap_err()
            });
        }
    }
    let input = &fixtures[0].bytes;
    let table = csv::parse(input, Default::default(), &|| false).unwrap();
    let small_table = csv::parse(&fixtures[1].bytes, Default::default(), &|| false).unwrap();
    let style = Style::default();
    let mut tmp = tempfile::NamedTempFile::new().unwrap();
    tmp.write_all(input).unwrap();
    tmp.flush().unwrap();
    measurement.measure("read 100k rows from disk", || {
        csv::read(tmp.path(), Default::default(), 65536, &|| false).unwrap()
    });
    measurement.measure("bounded preview and source (100k rows)", || {
        preview::generate(&table, &style, 200, &|| false).unwrap()
    });
    measurement.measure("bounded preview and source (50 rows)", || {
        preview::generate(&small_table, &style, 200, &|| false).unwrap()
    });
    for (name, table) in [("100k rows", &table), ("50 rows", &small_table)] {
        for inline in [false, true] {
            measurement.measure(
                &format!("table source preview ({name}, inline={inline})"),
                || {
                    preview::generate_with_renderer(
                        table,
                        &style,
                        200,
                        html::OutputMode::Table,
                        inline,
                        &|| false,
                    )
                    .unwrap()
                },
            );
        }
    }
    // Vec writes keep the generated bytes observable, unlike an optimized-away sink.
    measurement.measure("stream 100k rows to Vec (document)", || {
        let mut bytes = Vec::new();
        html::write(&table, &style, None, &mut bytes, &|| false).unwrap();
        black_box(bytes.len());
        bytes
    });
    measurement.measure("generic document 100k rows (string alloc)", || {
        html::output(&table, &style, None, html::OutputMode::Document, &|| false).unwrap()
    });
    // Untimed retained-capacity evidence for representative tables. Produce and
    // drop one output at a time, so the two APIs never retain both strings.
    for fixture in fixtures.iter().filter(|fixture| {
        matches!(
            fixture.name,
            "100k rows (simple)"
                | "50 rows (small)"
                | "2k rows (wide 256-col)"
                | "2k rows (sparse 256-col)"
                | "16 rows (long Unicode multiline)"
        )
    }) {
        let capacity_table = csv::parse(&fixture.bytes, fixture.delimiter, &|| false).unwrap();
        for api in ["generic_document", "document"] {
            let output = if api == "document" {
                html::document(&capacity_table, &style, None, &|| false).unwrap()
            } else {
                html::output(
                    &capacity_table,
                    &style,
                    None,
                    html::OutputMode::Document,
                    &|| false,
                )
                .unwrap()
            };
            println!(
                "{}",
                serde_json::json!({"kind": "output_capacity", "fixture": fixture.name,
                    "api": api, "rows": capacity_table.rows.len(),
                    "output_bytes": output.len(), "output_capacity_bytes": output.capacity()})
            );
            drop(output);
        }
    }
    for (name, mode) in [
        ("document", html::OutputMode::Document),
        ("table", html::OutputMode::Table),
    ] {
        let output = html::output(&table, &style, None, mode, &|| false).unwrap();
        println!(
            "{}",
            serde_json::json!({"kind":"output", "name": name,
            "rows":table.rows.len(), "output_bytes": output.len(),
            "output_capacity_bytes": output.capacity()})
        );
        drop(output);
        measurement.measure(&format!("{name} 100k rows (string alloc)"), || {
            if mode == html::OutputMode::Document {
                html::document(&table, &style, None, &|| false).unwrap()
            } else {
                html::output(&table, &style, None, mode, &|| false).unwrap()
            }
        });
        // Temporary directory setup is outside the timer; export still creates,
        // flushes, syncs and atomically publishes its own adjacent staging file.
        let directory = tempfile::tempdir().unwrap();
        let output_path = directory.path().join("output.html");
        measurement.measure(&format!("export 100k rows to file (atomic {name})"), || {
            files::export_with_mode(&table, &style, &output_path, true, mode, &|| false).unwrap()
        });
        measurement.measure(
            &format!("convert 100k rows end-to-end (atomic {name})"),
            || {
                files::convert_with_mode(
                    tmp.path(),
                    &output_path,
                    &style,
                    Default::default(),
                    true,
                    mode,
                    &|| false,
                )
                .unwrap()
            },
        );
    }
    measurement.measure("validate style (10,000 runs)", || {
        for _ in 0..10_000 {
            black_box(&style).validate().unwrap();
        }
    });
    let engine = Engine::default();
    let large_inserted = engine
        .request(
            serde_json::json!({"version": 1, "op": "read", "path": tmp.path()}),
            &|| false,
        )
        .unwrap();
    let large_handle = large_inserted["handle"].as_u64().unwrap();
    let records = std::iter::once(small_table.header.clone())
        .chain(small_table.rows.iter().cloned())
        .collect::<Vec<_>>();
    let inserted = engine
        .request(
            serde_json::json!({"version": 1, "op": "records", "records": records}),
            &|| false,
        )
        .unwrap();
    let handle = inserted["handle"].as_u64().unwrap();
    let req_bytes = serde_json::to_vec(
        &serde_json::json!({"version": 1, "op": "preview", "handle": handle, "style": &style}),
    )
    .unwrap();
    measurement.measure("engine dispatch preview 100x", || {
        for _ in 0..100 {
            black_box(engine.dispatch(black_box(&req_bytes), &|| false));
        }
    });
    for mode in ["table", "document"] {
        let request = serde_json::json!({"version": 1, "op": "html",
            "handle": large_handle, "style": &style, "output_mode": mode});
        let bytes = serde_json::to_vec(&request).unwrap();
        let label = format!("html 100k rows ({mode})");
        report_engine_case(&engine, &label, &request, &bytes);
        measurement.measure(&format!("engine request {label}"), || {
            engine
                .request(black_box(request.clone()), &|| false)
                .unwrap()
        });
        measurement.measure(&format!("engine dispatch {label}"), || {
            engine.dispatch(black_box(&bytes), &|| false)
        });
    }
    // Numeric classes and multiline cells can require more JSON escaping than
    // the simple fixture. Inspect their response growth independently of time.
    for fixture in fixtures.iter().filter(|fixture| {
        matches!(
            fixture.name,
            "2k rows (wide 256-col)" | "16 rows (long Unicode multiline)"
        )
    }) {
        let mut fixture_file = tempfile::NamedTempFile::new().unwrap();
        fixture_file.write_all(&fixture.bytes).unwrap();
        fixture_file.flush().unwrap();
        let inserted = engine
            .request(
                serde_json::json!({"version": 1, "op": "read", "path": fixture_file.path(),
                    "delimiter": fixture.delimiter}),
                &|| false,
            )
            .unwrap();
        let fixture_handle = inserted["handle"].as_u64().unwrap();
        let request = serde_json::json!({"version": 1, "op": "html",
            "handle": fixture_handle, "style": &style, "output_mode": "document"});
        let bytes = serde_json::to_vec(&request).unwrap();
        let label = format!("html {} (document)", fixture.name);
        report_engine_case(&engine, &label, &request, &bytes);
        if fixture.name == "2k rows (wide 256-col)" {
            measurement.measure(&format!("engine dispatch {label}"), || {
                engine.dispatch(black_box(&bytes), &|| false)
            });
        }
        engine
            .request(
                serde_json::json!({"version": 1, "op": "release", "handle": fixture_handle}),
                &|| false,
            )
            .unwrap();
    }
    for (name, preview_handle) in [("100k rows", large_handle), ("50 rows", handle)] {
        let request = serde_json::json!({"version": 1, "op": "preview",
            "handle": preview_handle, "style": &style, "output_mode": "table", "inline_preview": true});
        let bytes = serde_json::to_vec(&request).unwrap();
        let label = format!("preview {name} (table inline)");
        report_engine_case(&engine, &label, &request, &bytes);
        measurement.measure(&format!("engine request {label}"), || {
            engine
                .request(black_box(request.clone()), &|| false)
                .unwrap()
        });
        if preview_handle == large_handle {
            measurement.measure(&format!("engine dispatch {label}"), || {
                engine.dispatch(black_box(&bytes), &|| false)
            });
        }
    }
}
