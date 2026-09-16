use std::{cell::Cell, io::Cursor};
use yacht_core::{
    api::Engine,
    csv::{self, Delimiter},
    files, html, preview,
    style::Style,
    table::Table,
    Error,
};
fn parse(s: &str) -> Table {
    csv::parse(s.as_bytes(), Delimiter::Comma, &|| false).unwrap()
}
#[test]
fn csv_record_rules() {
    assert_eq!(
        parse("A,B,C\r\n\"a,b\",\"a\"\"b\",\"x\r\ny\nz\rw\"\r\n").rows,
        vec![vec!["a,b", "a\"b", "x\r\ny\nz\rw"]]
    );
    for nl in ["\r", "\n", "\r\n"] {
        assert_eq!(
            parse(&format!("A,B{nl}1,{nl}{nl},{nl}")).rows,
            vec![vec!["1", ""], vec![], vec!["", ""]]
        );
    }
    assert_eq!(parse("A,B\n1,").rows, vec![vec!["1", ""]]);
    assert_eq!(parse("\"\"").header, [""]);
    assert_eq!(parse("\n").header, ["Column 1"]);
    assert_eq!(parse("A,B\n1\n2,3,4\n\n").warnings().len(), 2);
}
#[test]
fn utf8_bom_chunks_delimiters_and_errors() {
    for d in [
        Delimiter::Comma,
        Delimiter::Tab,
        Delimiter::Semicolon,
        Delimiter::Pipe,
    ] {
        let sep = char::from(d.byte());
        let input = format!("\u{feff}名前{sep}Emoji\r\n\"🛥️\r\n漢字{sep}x\"{sep}\"a\"\"b\"\r\n");
        let table = csv::parse(input.as_bytes(), d, &|| false).unwrap();
        for chunk in [1, 2, 3, 7, 65536] {
            assert_eq!(
                table,
                csv::read_stream(Cursor::new(input.as_bytes()), d, chunk, &|| false).unwrap()
            );
        }
    }
    for input in [
        b"".as_slice(),
        b"A\n\"unclosed",
        b"A\n\"closed\"oops",
        b"A\nbad\"quote",
        b"A\n\0",
        b"\xff\xfeA\0",
        b"A\n\xff",
    ] {
        assert!(csv::parse(input, Delimiter::Comma, &|| false).is_err());
    }
    assert_eq!(parse("A\nCafe\u{301}🛥️").rows[0][0], "Cafe\u{301}🛥️");
}
#[test]
fn current_document_contract() {
    let table = parse("Item,Count,Notes\nCompass,12,\nCafé,3,\"north & south\"\n");
    for style in [Style::default(), Style::unstyled()] {
        let doc = html::document(&table, &style, None, &|| false).unwrap();
        assert!(doc.starts_with("<!doctype html>"));
        assert!(doc.contains("<title>YACHT Table</title>"));
        assert!(doc.contains("name=\"viewport\""));
        assert_eq!(doc.matches("scope=\"col\"").count(), 3);
        for cell in [
            "Compass</td>",
            "Café</td>",
            "north &amp; south</td>",
            "<td></td>",
        ] {
            assert!(doc.contains(cell), "{cell}");
        }
    }
}
#[test]
fn escaping_and_css() {
    assert_eq!(html::escape("&<>\"'"), "&amp;&lt;&gt;&quot;&#x27;");
    assert_eq!(
        html::escape("<\u{301}&\u{fe0f}'\u{301}"),
        "&lt;\u{301}&amp;\u{fe0f}&#x27;\u{301}"
    );
    let table = parse("<img onerror=alert(1)>\n</td><script>alert('x')</script>");
    let style = Style {
        table_class: "\"\u{fe0f} onmouseover=\"alert(1)".into(),
        ..Default::default()
    };
    let doc = html::document(&table, &style, None, &|| false).unwrap();
    assert!(!doc.contains("<script"));
    assert!(!doc.contains("<img"));
    assert!(doc.contains("&quot;\u{fe0f} onmouseover=&quot;"));
    for attack in [
        "</style><script>alert(1)</script>",
        "red; background:url(https://example.com)",
        "url(file:///etc/passwd)",
        "red\\3c",
        "red\n",
    ] {
        assert!(Style {
            header_bg: attack.into(),
            ..Default::default()
        }
        .validate()
        .is_err());
    }
    for attack in ["</style>", "url(x)", "Arial; color:red", "'unmatched"] {
        assert!(Style {
            font_family: attack.into(),
            ..Default::default()
        }
        .validate()
        .is_err());
    }
    for value in [
        "0",
        "-4",
        "+3",
        "1234",
        "12345",
        "1,234.5",
        " 12.0 ",
        "123456789",
    ] {
        assert!(html::is_numeric(value));
    }
    for value in ["", "17%", "1,23", "1,2345", "NaN", "1e4", "—", "3a"] {
        assert!(!html::is_numeric(value));
    }
    let bare = html::document(&table, &Style::unstyled(), None, &|| false).unwrap();
    assert!(!bare.contains("<style>"));
    assert!(!bare.contains("class="));
    let styled = Style {
        font_family: "'Times New Roman', serif".into(),
        font_size_px: 20,
        cell_padding_px: 12,
        border_width_px: 3,
        border_style: "dashed".into(),
        border_color: "rebeccapurple".into(),
        header_bg: "rgb(1, 2, 3)".into(),
        header_text_color: "white".into(),
        body_bg: "#ffeecc".into(),
        zebra_enabled: false,
        hover_enabled: false,
        border_collapse: "separate".into(),
        border_spacing_px: 4,
        ..Default::default()
    };
    let doc = html::document(&table, &styled, None, &|| false).unwrap();
    for token in [
        "20px",
        "12px",
        "3px dashed rebeccapurple",
        "rgb(1, 2, 3)",
        "#ffeecc",
        "border-spacing: 4px",
    ] {
        assert!(doc.contains(token));
    }
    assert!(!doc.contains("nth-child"));
    assert!(!doc.contains("tr:hover"));
}
#[test]
fn preview_and_complete_exports() {
    let table = Table::sample();
    let style = Style::default();
    let p = preview::generate(&table, &style, 2, &|| false).unwrap();
    assert_eq!(
        p.html,
        html::document(&table, &style, Some(2), &|| false).unwrap()
    );
    assert_eq!(
        p.source,
        html::document(&table, &style, None, &|| false).unwrap()
    );
    let huge = parse(&format!("A\n{}", "🛥️".repeat(200_000)));
    let p = preview::generate(&huge, &Style::unstyled(), 200, &|| false).unwrap();
    assert!(p.preview_unavailable && p.source_truncated);
    assert!(p.source.len() <= 1_000_000);
    assert!(!p.source.contains('\u{fffd}'));
    let wide = Table::new(
        std::iter::once(vec!["A".into(); 10000])
            .chain(vec![vec![]; 200])
            .collect(),
    )
    .unwrap();
    assert!(
        preview::generate(&wide, &style, 200, &|| false)
            .unwrap()
            .row_count
            < 3
    );
    let large = parse(&format!("A,B\n{}", "123456,hello\n".repeat(100_000)));
    assert_eq!(large.rows.len(), 100_000);
    assert_eq!(
        html::document(&large, &style, None, &|| false)
            .unwrap()
            .matches("        <tr>")
            .count(),
        100_000
    );
}
#[test]
fn atomic_export_presets_batch_and_cancellation() {
    let dir = tempfile::tempdir().unwrap();
    let output = dir.path().join("out.html");
    let style = Style::default();
    let table = Table::sample();
    files::export(&table, &style, &output, false, &|| false).unwrap();
    let original = std::fs::read(&output).unwrap();
    assert!(files::export(&table, &Style::unstyled(), &output, false, &|| false).is_err());
    let checks = Cell::new(0);
    assert!(matches!(
        files::export(&table, &Style::unstyled(), &output, true, &|| {
            checks.set(checks.get() + 1);
            checks.get() > 6
        }),
        Err(Error::Cancelled)
    ));
    assert_eq!(original, std::fs::read(&output).unwrap());
    files::export(&table, &Style::unstyled(), &output, true, &|| false).unwrap();
    assert_eq!(std::fs::read_dir(dir.path()).unwrap().count(), 1);
    let good = dir.path().join("good.tsv");
    let bad = dir.path().join("bad.csv");
    std::fs::write(&good, "A\tB\n1\t2").unwrap();
    std::fs::write(&bad, "A\n\"bad").unwrap();
    assert!(files::convert(&good, &good, &style, Delimiter::Tab, true, &|| false).is_err());
    let results = files::batch(
        &[bad, good],
        &style,
        Delimiter::Comma,
        true,
        false,
        &|| false,
        |_| {},
    )
    .unwrap();
    assert!(results[0].error.is_some());
    assert!(results[1].error.is_none());
    assert!(std::fs::read_to_string(&results[1].output)
        .unwrap()
        .contains("Field #2"));
    let presets_path = dir.path().join("presets.json");
    assert!(files::load_presets(&presets_path).unwrap().is_empty());
    let presets = [("Teaching".into(), Style::unstyled())]
        .into_iter()
        .collect();
    files::save_presets(&presets_path, &presets, &|| false).unwrap();
    assert_eq!(files::load_presets(&presets_path).unwrap(), presets);
    let invalid = [("Unstyled".into(), Style::default())]
        .into_iter()
        .collect();
    assert!(files::save_presets(&presets_path, &invalid, &|| false).is_err());
    assert_eq!(
        Style::from_value(serde_json::json!({"font_size_px":22,"border_color":null}))
            .unwrap()
            .border_color,
        "#cccccc"
    );
    assert_eq!(Style::from_value(serde_json::json!({})).unwrap(), style);
}
#[test]
fn bindings_handle_lifetime_and_cancellation() {
    let engine = Engine::default();
    let table = engine
        .request(serde_json::json!({"version":1,"op":"sample"}), &|| false)
        .unwrap();
    let handle = table["handle"].clone();
    let request = serde_json::json!({"version":1,"op":"preview","handle":handle});
    assert!(engine.request(request.clone(), &|| false).unwrap()["html"]
        .as_str()
        .unwrap()
        .contains("Friction, Baby"));
    assert!(matches!(
        engine.request(request.clone(), &|| true),
        Err(Error::Cancelled)
    ));
    engine
        .request(
            serde_json::json!({"version":1,"op":"release","handle":handle}),
            &|| false,
        )
        .unwrap();
    assert!(engine.request(request, &|| false).is_err());
}

#[test]
fn regular_file_and_invalid_style_safeguards() {
    let directory = tempfile::tempdir().unwrap();
    assert!(csv::read(directory.path(), Delimiter::Comma, 65536, &|| false).is_err());
    let output = directory.path().join("existing.html");
    std::fs::write(&output, "KEEP").unwrap();
    let invalid = Style {
        font_size_px: 10001,
        ..Default::default()
    };
    assert!(files::export(&Table::sample(), &invalid, &output, true, &|| false).is_err());
    assert_eq!(std::fs::read_to_string(&output).unwrap(), "KEEP");
    assert_eq!(std::fs::read_dir(directory.path()).unwrap().count(), 1);
}
