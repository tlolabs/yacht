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
fn api(engine: &Engine, op: &str, mut args: serde_json::Value) -> serde_json::Value {
    args["version"] = serde_json::json!(1);
    args["op"] = serde_json::json!(op);
    engine.request(args, &|| false).unwrap()
}
fn cell_attributes(html: &str) -> Vec<&str> {
    html.split("<td")
        .skip(1)
        .map(|cell| cell.split_once('>').unwrap().0)
        .collect()
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
fn sparse_rows_survive_changes_in_record_width() {
    let input = b"A,B,C\n\nsolo\n1,2,3\n\nx,y,z,extra\nlast\n";
    let expected = Table {
        header: ["A", "B", "C", "Column 4"].map(str::to_owned).to_vec(),
        rows: vec![
            vec![],
            vec!["solo".into()],
            vec!["1".into(), "2".into(), "3".into()],
            vec![],
            vec!["x".into(), "y".into(), "z".into(), "extra".into()],
            vec!["last".into()],
        ],
        short_row_count: 4,
        extra_row_count: 1,
    };
    assert_eq!(
        csv::parse(input, Delimiter::Comma, &|| false).unwrap(),
        expected
    );
    for chunk in [1, 2, 3, 7, 65536] {
        assert_eq!(
            csv::read_stream(Cursor::new(input), Delimiter::Comma, chunk, &|| false).unwrap(),
            expected,
            "chunk={chunk}"
        );
    }
}
#[test]
fn repeated_records_preserve_narrow_and_wide_unicode_cells() {
    for width in [1, 3, 128] {
        let header: Vec<String> = (0..width).map(|column| format!("H{column}")).collect();
        let rows: Vec<Vec<String>> = (0..4)
            .map(|row| {
                (0..width)
                    .map(|column| format!("Café 🛥 {row}:{column}"))
                    .collect()
            })
            .collect();
        let mut input = format!("{}\n", header.join(","));
        for row in &rows {
            input.push_str(&row.join(","));
            input.push('\n');
        }
        let expected = Table {
            header,
            rows,
            short_row_count: 0,
            extra_row_count: 0,
        };
        assert_eq!(parse(&input), expected, "width={width}");
        for chunk in [1, 7, 65536] {
            assert_eq!(
                csv::read_stream(
                    Cursor::new(input.as_bytes()),
                    Delimiter::Comma,
                    chunk,
                    &|| false
                )
                .unwrap(),
                expected,
                "width={width}, chunk={chunk}"
            );
        }
    }
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
        "\u{00a0}12",
        "12\u{00a0}",
        "\u{2003}42.5",
        "12\u{2003}",
        "\u{00a0}  +1,234.5\u{2003} ",
        "\u{2002}99",
    ] {
        assert!(html::is_numeric(value), "expected {value:?} to be numeric");
    }
    for value in [
        "",
        "17%",
        "1,23",
        "1,2345",
        "NaN",
        "1e4",
        "—",
        "3a",
        "\u{00a0}abc",
        "\u{2003}Café",
        "\u{00a0}",
    ] {
        assert!(
            !html::is_numeric(value),
            "expected {value:?} to not be numeric"
        );
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
        "color: #222222;",
    ] {
        assert!(doc.contains(token));
    }
    assert!(!doc.contains("nth-child"));
    assert!(!doc.contains("tr:hover"));

    // Default style has body, zebra and hover text colors specified with accessible contrast
    let default_doc = html::document(&table, &Style::default(), None, &|| false).unwrap();
    assert!(default_doc.contains("background: #ffffff;\n      color: #222222;"));
    assert!(default_doc.contains("background: #fbfbfb;\n      color: #222222;"));
    assert!(default_doc.contains("background: #f2f8ff;\n      color: #222222;"));

    // Dark body background selects light accessible text color
    let dark_styled = Style {
        body_bg: "#111111".into(),
        zebra_enabled: true,
        zebra_bg: "#222222".into(),
        hover_enabled: true,
        hover_bg: "#333333".into(),
        ..Default::default()
    };
    let dark_doc = html::document(&table, &dark_styled, None, &|| false).unwrap();
    assert!(dark_doc.contains("background: #111111;\n      color: #ffffff;"));
    assert!(dark_doc.contains("background: #222222;\n      color: #ffffff;"));
    assert!(dark_doc.contains("background: #333333;\n      color: #ffffff;"));

    // 8-character and 4-character hex colors, rgb/rgba, and dark color names select white accessible text color
    assert_eq!(html::accessible_text_color("#000000ff"), "#ffffff");
    assert_eq!(html::accessible_text_color("#000f"), "#ffffff");
    assert_eq!(html::accessible_text_color("black"), "#ffffff");
    assert_eq!(html::accessible_text_color("navy"), "#ffffff");
    assert_eq!(html::accessible_text_color("rgb(0, 0, 0)"), "#ffffff");
    assert_eq!(html::accessible_text_color("rgb(10, 10, 10)"), "#ffffff");
    assert_eq!(html::accessible_text_color("rgba(0, 0, 0, 1)"), "#ffffff");
    assert_eq!(
        html::accessible_text_color("rgb(100%, 100%, 100%)"),
        "#222222"
    );
    assert_eq!(html::accessible_text_color("rgb(255, 255, 255)"), "#222222");
    assert_eq!(html::accessible_text_color("#ffffffcc"), "#222222");

    // First column rule precedes hover rule so hover highlights the entire row
    let first_and_hover = Style {
        first_column_enabled: true,
        first_column_bg: "#dcdcdc".into(),
        hover_enabled: true,
        hover_bg: "#ffff99".into(),
        ..Default::default()
    };
    let hover_doc = html::document(&table, &first_and_hover, None, &|| false).unwrap();
    let first_idx = hover_doc.find("td:first-child").unwrap();
    let hover_idx = hover_doc.find("tr:hover td").unwrap();
    assert!(
        first_idx < hover_idx,
        "first-column rule must precede hover rule in CSS"
    );
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
fn numeric_document_dispatch_avoids_a_response_capacity_cliff() {
    // Numeric document cells add two JSON-escaped attribute quotes each. This
    // wide fixture crosses a small reservation hint without crossing the
    // predecessor's next power-of-two allocation boundary.
    let table = Table::from_parts(
        vec!["A".to_owned(); 128],
        vec![vec!["1".to_owned(); 128]; 512],
    )
    .unwrap();
    let expected = html::output(
        &table,
        &Style::default(),
        None,
        html::OutputMode::Document,
        &|| false,
    )
    .unwrap();
    let records: Vec<_> = std::iter::once(table.header.clone())
        .chain(table.rows.iter().cloned())
        .collect();
    let engine = Engine::default();
    let inserted = api(&engine, "records", serde_json::json!({"records": records}));
    let request = serde_json::json!({"version": 1, "op": "html",
        "handle": inserted["handle"], "output_mode": "document"});
    let response = engine.dispatch(&serde_json::to_vec(&request).unwrap(), &|| false);
    let decoded: serde_json::Value = serde_json::from_slice(&response).unwrap();
    assert_eq!(decoded["ok"].as_str(), Some(expected.as_str()));
    assert!(
        response.capacity() <= response.len().next_power_of_two(),
        "numeric document JSON retained {} bytes for {} output bytes; previous growth boundary is {}",
        response.capacity(),
        response.len(),
        response.len().next_power_of_two()
    );
}

#[test]
fn binding_strings_and_previews_match_core_and_exact_dispatch_json() {
    fn assert_response(engine: &Engine, request: serde_json::Value, expected: serde_json::Value) {
        assert_eq!(
            engine.request(request.clone(), &|| false).unwrap(),
            expected
        );
        assert_eq!(
            engine.dispatch(&serde_json::to_vec(&request).unwrap(), &|| false),
            serde_json::to_vec(&serde_json::json!({"ok": expected})).unwrap()
        );
    }
    fn insert(engine: &Engine, table: &Table) -> serde_json::Value {
        let records: Vec<_> = std::iter::once(table.header.clone())
            .chain(table.rows.iter().cloned())
            .collect();
        api(engine, "records", serde_json::json!({"records": records}))["handle"].clone()
    }

    let engine = Engine::default();
    let text = "Café 🛥 <&>\"'\n\t\0";
    assert_response(
        &engine,
        serde_json::json!({"version": 1, "op": "escape", "text": text}),
        serde_json::Value::String(html::escape(text)),
    );
    // JSON escaping can exceed the bounded response capacity hint. It must
    // still grow normally and return every byte, including escaped NULs.
    let controls = "\0\\\"\n\t".repeat(16_384);
    assert_response(
        &engine,
        serde_json::json!({"version": 1, "op": "escape", "text": controls}),
        serde_json::Value::String(html::escape(&controls)),
    );
    let table = Table::from_parts(
        vec!["<Name&>".into(), "Count".into(), "Notes".into()],
        vec![
            vec![text.into(), "-1,234.5".into(), "line 1\r\nline 2".into()],
            vec![],
            vec!["last".into(), "17%".into()],
        ],
    )
    .unwrap();
    let handle = insert(&engine, &table);
    let styled = Style {
        first_column_enabled: true,
        table_class: "x\"<&".into(),
        ..Default::default()
    };
    for (style, mode, limit) in [
        (&styled, html::OutputMode::Table, None),
        (&styled, html::OutputMode::Document, Some(1)),
        (&styled, html::OutputMode::Table, Some(0)),
        (&Style::unstyled(), html::OutputMode::Document, None),
    ] {
        assert_response(
            &engine,
            serde_json::json!({"version": 1, "op": "html", "handle": handle,
                "style": style, "output_mode": mode, "limit": limit}),
            serde_json::Value::String(html::output(&table, style, limit, mode, &|| false).unwrap()),
        );
    }
    for (mode, inline, limit) in [
        (html::OutputMode::Table, false, 0),
        (html::OutputMode::Table, true, 1),
        (html::OutputMode::Document, false, 200),
        (html::OutputMode::Document, true, 200),
    ] {
        let expected =
            preview::generate_with_renderer(&table, &styled, limit, mode, inline, &|| false)
                .unwrap();
        assert_response(
            &engine,
            serde_json::json!({"version": 1, "op": "preview", "handle": handle,
                "style": styled, "output_mode": mode, "inline_preview": inline, "limit": limit}),
            serde_json::to_value(expected).unwrap(),
        );
    }

    let large = Table::from_parts(vec!["Text".into()], vec![vec!["🛥️".repeat(200_000)]]).unwrap();
    let large_handle = insert(&engine, &large);
    let expected = preview::generate_with_renderer(
        &large,
        &styled,
        200,
        html::OutputMode::Table,
        false,
        &|| false,
    )
    .unwrap();
    assert!(expected.preview_unavailable);
    assert!(expected.source_truncated);
    assert!(expected.source.len() <= preview::SOURCE_BUDGET);
    assert_response(
        &engine,
        serde_json::json!({"version": 1, "op": "preview", "handle": large_handle,
            "style": styled, "output_mode": "table"}),
        serde_json::to_value(expected).unwrap(),
    );

    for op in ["escape", "html", "preview"] {
        let request = serde_json::json!({"version": 1, "op": op, "handle": handle, "text": text});
        assert!(matches!(
            engine.request(request.clone(), &|| true),
            Err(Error::Cancelled)
        ));
        assert_eq!(
            engine.dispatch(&serde_json::to_vec(&request).unwrap(), &|| true),
            br#"{"error":{"code":"cancelled","message":"Cancelled"}}"#
        );
    }
}

#[test]
fn binding_html_defaults_to_a_self_contained_table_fragment() {
    let engine = Engine::default();
    let table = api(
        &engine,
        "records",
        serde_json::json!({"records":[["<Item>","Count"],["north & south","12"],["Café",""]]}),
    );
    let handle = &table["handle"];
    let output = api(&engine, "html", serde_json::json!({"handle":handle}));
    let fragment = output.as_str().unwrap();
    assert!(fragment.trim_start().starts_with("<table"));
    assert!(fragment.trim_end().ends_with("</table>"));
    for wrapper in ["<!doctype", "<html", "<head", "<body", "<div", "<style"] {
        assert!(!fragment.contains(wrapper), "unexpected {wrapper}");
    }
    assert!(fragment.split_once('>').unwrap().0.contains("style="));
    assert_eq!(fragment.matches("scope=\"col\"").count(), 2);
    assert!(fragment.contains("&lt;Item&gt;</th>"));
    assert!(fragment.contains("north &amp; south</td>"));
    assert!(fragment.contains("Café</td>"));
    let cells = cell_attributes(fragment);
    assert_eq!(cells.len(), 4);
    assert!(cells.iter().all(|attrs| attrs.contains("style=")));
    assert!(!cells[0].contains("text-align: right"));
    assert!(cells[1].contains("text-align: right"));
    assert!(!cells[2].contains("text-align: right"));
    assert!(!cells[3].contains("text-align: right"));

    let preview = api(
        &engine,
        "preview",
        serde_json::json!({"handle":handle,"limit":1}),
    );
    assert_eq!(preview["source"], output);
    assert_eq!(preview["row_count"], 1);
    assert!(!preview["html"].as_str().unwrap().contains("Café"));

    let full = api(
        &engine,
        "html",
        serde_json::json!({"handle":handle,"output_mode":"document"}),
    );
    assert!(full.as_str().unwrap().starts_with("<!doctype html>"));
    assert!(full.as_str().unwrap().contains("<style>"));
    assert_eq!(
        api(
            &engine,
            "preview",
            serde_json::json!({"handle":handle,"limit":1,"output_mode":"document"}),
        )["source"],
        full
    );

    let unstyled = api(&engine, "unstyled", serde_json::json!({}));
    let bare = api(
        &engine,
        "html",
        serde_json::json!({"handle":handle,"style":unstyled}),
    );
    let bare = bare.as_str().unwrap();
    assert!(bare.trim_start().starts_with("<table>"));
    assert!(bare.trim_end().ends_with("</table>"));
    assert!(!bare.contains("style="));
    assert!(!bare.contains("class="));
    assert!(bare.contains("north &amp; south</td>"));
}

#[test]
fn inline_cells_match_original_output_and_propagate_sink_errors() {
    // Captured from the original release library, before the inline writer
    // optimization. Includes numeric/text cells, both stripes, a first-column
    // override, missing cells, Unicode and escaped attribute/text characters.
    let table = Table::from_parts(
        vec!["<H>&".into(), "Count".into(), "Notes".into()],
        vec![
            vec!["12".into(), "word".into(), "é<&>\"'".into()],
            vec!["Café".into(), "-1,234.5".into()],
            vec![],
        ],
    )
    .unwrap();
    let style = Style {
        first_column_enabled: true,
        first_column_bg: "navy".into(),
        zebra_bg: "black".into(),
        font_family: "\"Times New Roman\", serif".into(),
        table_class: "x\"<&".into(),
        ..Default::default()
    };
    for (style, limit, expected) in [
        (&style, None, include_str!("fixtures/inline-styled.html")),
        (
            &style,
            Some(1),
            include_str!("fixtures/inline-styled-one-row.html"),
        ),
        (
            &Style::unstyled(),
            None,
            include_str!("fixtures/inline-unstyled.html"),
        ),
    ] {
        let output =
            html::output(&table, style, limit, html::OutputMode::Table, &|| false).unwrap();
        assert_eq!(output, expected);
        struct FailAfter(usize);
        impl std::io::Write for FailAfter {
            fn write(&mut self, bytes: &[u8]) -> std::io::Result<usize> {
                if self.0 == 0 {
                    return Err(std::io::Error::other("full"));
                }
                let n = bytes.len().min(self.0);
                self.0 -= n;
                Ok(n)
            }
            fn flush(&mut self) -> std::io::Result<()> {
                Ok(())
            }
        }
        for budget in [0, 64, 500, expected.len() - 1] {
            if budget < expected.len() {
                assert!(matches!(
                    html::write_output(
                        &table,
                        style,
                        limit,
                        html::OutputMode::Table,
                        &mut FailAfter(budget),
                        &|| false
                    ),
                    Err(Error::Io(_))
                ));
            }
        }
        let checks = Cell::new(0);
        assert!(matches!(
            html::output(&table, style, limit, html::OutputMode::Table, &|| {
                checks.set(checks.get() + 1);
                checks.get() >= 2
            }),
            Err(Error::Cancelled)
        ));
    }
}

#[test]
fn table_fragment_keeps_quoted_fonts_and_untrusted_classes_in_their_attributes() {
    let engine = Engine::default();
    let table = api(&engine, "sample", serde_json::json!({}));
    let html = api(
        &engine,
        "html",
        serde_json::json!({
            "handle":table["handle"],
            "style":{
                "table_class":"\" onmouseover=\"alert(1)\"><script>alert(1)</script>&",
                "font_family":"\"Times New Roman\", serif"
            }
        }),
    );
    let html = html.as_str().unwrap();
    let opening = html.split_once('>').unwrap().0;
    assert!(opening.contains("class=\"csv-table &quot; onmouseover=&quot;alert(1)&quot;&gt;&lt;script&gt;alert(1)&lt;/script&gt;&amp;\""));
    assert!(opening.contains("style=\"font-family: &quot;Times New Roman&quot;, serif;"));
    // Only class and style delimiters remain as literal quotes in the tag.
    assert_eq!(opening.matches('"').count(), 4);
    assert_eq!(html.matches("<table").count(), 1);
    assert!(!html.contains("<script"));
    assert!(!html.contains(" onmouseover=\""));
}

#[test]
fn table_mode_bounds_utf8_source_and_cancels_during_a_large_field() {
    let engine = Engine::default();
    let table = api(
        &engine,
        "records",
        serde_json::json!({"records":[["A"],["🛥️".repeat(200_000)]]}),
    );
    let args = serde_json::json!({"handle":table["handle"],"output_mode":"table"});
    let full = api(&engine, "html", args.clone());
    let full = full.as_str().unwrap();
    let p = api(&engine, "preview", args.clone());
    let source = p["source"].as_str().unwrap();
    assert!(p["source_truncated"].as_bool().unwrap());
    assert!(p["preview_unavailable"].as_bool().unwrap());
    assert!(source.starts_with("<table"));
    assert!(full.len() > preview::SOURCE_BUDGET);
    assert!(source.len() <= preview::SOURCE_BUDGET);
    assert!(full.starts_with(source));
    assert!(!source.contains('\u{fffd}'));
    // The omitted suffix begins at a scalar boundary and includes actual cell data.
    assert!(full[source.len()..].contains('🛥'));
    for op in ["html", "preview"] {
        let mut request = args.clone();
        request["version"] = serde_json::json!(1);
        request["op"] = serde_json::json!(op);
        let checks = Cell::new(0);
        let result = engine.request(request, &|| {
            checks.set(checks.get() + 1);
            checks.get() >= 8
        });
        assert!(matches!(result, Err(Error::Cancelled)), "{op}");
        assert_eq!(checks.get(), 8);
    }
    // Cancellation during preview HTML rendering (where count > 0) aborts immediately
    let normal_table = api(
        &engine,
        "records",
        serde_json::json!({"records":[["A"],["B"],["C"],["D"],["E"]]}),
    );
    let req = serde_json::json!({"version":1,"op":"preview","handle":normal_table["handle"]});
    let checks = Cell::new(0);
    let result = engine.request(req, &|| {
        checks.set(checks.get() + 1);
        checks.get() >= 2
    });
    assert!(matches!(result, Err(Error::Cancelled)));
}

#[test]
fn binding_output_modes_apply_to_all_file_exports() {
    let engine = Engine::default();
    let dir = tempfile::tempdir().unwrap();
    let input = dir.path().join("source.csv");
    std::fs::write(&input, "Item,Count\nCompass,12\nCafé,3\n").unwrap();
    let table = api(&engine, "read", serde_json::json!({"path":input}));
    for mode in ["table", "document"] {
        let expected = api(
            &engine,
            "html",
            serde_json::json!({"handle":table["handle"],"output_mode":mode}),
        );
        let export = dir.path().join(format!("export-{mode}.html"));
        api(
            &engine,
            "export",
            serde_json::json!({"handle":table["handle"],"path":export,"output_mode":mode}),
        );
        assert_eq!(
            std::fs::read_to_string(export).unwrap(),
            expected.as_str().unwrap()
        );
        let convert = dir.path().join(format!("convert-{mode}.html"));
        api(
            &engine,
            "convert",
            serde_json::json!({"input":input,"output":convert,"output_mode":mode}),
        );
        assert_eq!(
            std::fs::read_to_string(convert).unwrap(),
            expected.as_str().unwrap()
        );
        let batch = api(
            &engine,
            "batch",
            serde_json::json!({"inputs":[input],"output_mode":mode,"overwrite":true}),
        );
        assert!(batch[0]["error"].is_null());
        assert_eq!(
            std::fs::read_to_string(batch[0]["output"].as_str().unwrap()).unwrap(),
            expected.as_str().unwrap()
        );
    }
    let default_export = dir.path().join("default.html");
    api(
        &engine,
        "export",
        serde_json::json!({"handle":table["handle"],"path":default_export}),
    );
    assert!(std::fs::read_to_string(&default_export)
        .unwrap()
        .trim_start()
        .starts_with("<table"));
    let before = std::fs::read(&default_export).unwrap();
    assert!(engine.request(serde_json::json!({"version":1,"op":"export","handle":table["handle"],"path":default_export,"overwrite":true,"output_mode":"invalid"}), &|| false).is_err());
    assert_eq!(std::fs::read(&default_export).unwrap(), before);
    assert_eq!(
        std::fs::read_to_string(input).unwrap(),
        "Item,Count\nCompass,12\nCafé,3\n"
    );
}

#[test]
fn named_styles_preserve_the_default_and_reference_column_treatments() {
    let engine = Engine::default();
    let presets = api(&engine, "builtin_presets", serde_json::json!({}));
    let names: Vec<_> = presets
        .as_object()
        .unwrap()
        .keys()
        .map(String::as_str)
        .collect();
    assert_eq!(
        names,
        [
            "G-Blue",
            "G-Blue Gray",
            "G-Brown",
            "G-Cyan",
            "G-Gray",
            "G-Green",
            "G-Light Green",
            "G-Orange",
            "G-Purple",
            "G-Teal",
            "G-Yellow",
            "N-Blue",
            "N-Gray",
            "N-Green",
            "N-Orange",
            "N-Red",
            "N-White",
            "Unstyled",
            "Y-Default"
        ]
    );
    assert_eq!(
        presets["Y-Default"],
        api(&engine, "defaults", serde_json::json!({}))
    );
    assert_eq!(
        presets["Unstyled"],
        api(&engine, "unstyled", serde_json::json!({}))
    );
    let table = api(
        &engine,
        "records",
        serde_json::json!({"records":[["Item","Count"],["One","1"],["Two","2"]]}),
    );
    for name in [
        "N-Blue", "N-Orange", "N-Green", "N-Red", "N-Gray", "N-White",
    ] {
        let style = &presets[name];
        api(
            &engine,
            "validate_style",
            serde_json::json!({"style":style}),
        );
        assert_eq!(style["first_column_enabled"], true, "{name}");
        assert_eq!(style["zebra_enabled"], name != "N-Gray", "{name}");
        let rendered = api(
            &engine,
            "html",
            serde_json::json!({"handle":table["handle"],"style":style}),
        );
        let cells = cell_attributes(rendered.as_str().unwrap());
        let first_column_bg = style["first_column_bg"].as_str().unwrap();
        assert!(cells[0].contains(first_column_bg), "{name}");
        assert!(
            cells[2].contains(first_column_bg),
            "{name}: stripe overrides first column"
        );
        assert!(
            cells[1].contains(style["body_bg"].as_str().unwrap()),
            "{name}"
        );
        let second_row_bg = if name == "N-Gray" {
            "body_bg"
        } else {
            "zebra_bg"
        };
        assert!(
            cells[3].contains(style[second_row_bg].as_str().unwrap()),
            "{name}"
        );
        if name == "N-Gray" {
            assert_ne!(style["first_column_bg"], style["body_bg"]);
        } else {
            assert_ne!(style["zebra_bg"], style["body_bg"]);
        }
    }
}

#[test]
fn google_presets_keep_reference_colors_and_stripe_the_entire_row() {
    let engine = Engine::default();
    let presets = api(&engine, "builtin_presets", serde_json::json!({}));
    let table = api(
        &engine,
        "records",
        serde_json::json!({"records":[["Item","Count"],["One","1"],["Two","2"]]}),
    );
    for (name, header, stripe) in [
        ("G-Gray", "#bdbdbd", "#f3f3f3"),
        ("G-Cyan", "#4dd0e1", "#e0f7fa"),
        ("G-Green", "#63d297", "#e7f9ef"),
        ("G-Yellow", "#f7cb4c", "#fef8e3"),
        ("G-Orange", "#f46524", "#ffe6dd"),
        ("G-Blue", "#5c95f9", "#e8f0fe"),
        ("G-Teal", "#26a69a", "#ddf2f0"),
        ("G-Blue Gray", "#78909c", "#ebeff1"),
        ("G-Brown", "#cca677", "#f8f2eb"),
        ("G-Light Green", "#8bc34a", "#eef7e3"),
        ("G-Purple", "#8989eb", "#e8e7fc"),
    ] {
        let style = &presets[name];
        api(
            &engine,
            "validate_style",
            serde_json::json!({"style":style}),
        );
        assert_eq!(style["header_bg"], header, "{name}");
        assert_eq!(style["body_bg"], "#ffffff", "{name}");
        assert_eq!(style["zebra_bg"], stripe, "{name}");
        assert_eq!(style["first_column_enabled"], false, "{name}");
        assert_eq!(style["zebra_enabled"], true, "{name}");
        assert_eq!(style["hover_enabled"], false, "{name}");
        for output_mode in ["table", "document"] {
            let rendered = api(
                &engine,
                "html",
                serde_json::json!({"handle":table["handle"],"style":style,"output_mode":output_mode}),
            );
            let rendered = rendered.as_str().unwrap();
            assert!(rendered.contains(header), "{name}: {output_mode}");
            if output_mode == "table" {
                let cells = cell_attributes(rendered);
                assert_eq!(cells.len(), 4, "{name}");
                for cell in &cells[..2] {
                    assert!(cell.contains("background: #ffffff;"), "{name}");
                }
                for cell in &cells[2..] {
                    assert!(cell.contains(&format!("background: {stripe};")), "{name}");
                }
            } else {
                assert!(
                    rendered.contains(&format!(
                        ".csv-table tbody tr:nth-child(even) td {{\n      background: {stripe};"
                    )),
                    "{name}"
                );
                assert!(!rendered.contains("td:first-child"), "{name}");
                assert!(!rendered.contains("tr:hover"), "{name}");
            }
        }
    }
}

#[test]
fn legacy_preferences_and_presets_get_safe_new_defaults() {
    let engine = Engine::default();
    let legacy = serde_json::json!({"font_size_px":22,"header_bg":"#123456"});
    let normalized = api(
        &engine,
        "normalize_style",
        serde_json::json!({"style":legacy}),
    );
    assert_eq!(normalized["font_size_px"], 22);
    assert_eq!(normalized["header_bg"], "#123456");
    assert_eq!(normalized["first_column_enabled"], false);
    assert_eq!(normalized["first_column_bg"], "#ffffff");
    let unstyled = api(&engine, "unstyled", serde_json::json!({}));
    let mut legacy_unstyled = unstyled.clone();
    let fields = legacy_unstyled.as_object_mut().unwrap();
    fields.remove("first_column_enabled");
    fields.remove("first_column_bg");
    assert_eq!(
        api(
            &engine,
            "normalize_style",
            serde_json::json!({"style":legacy_unstyled}),
        ),
        unstyled
    );
    assert_eq!(
        api(
            &engine,
            "is_unstyled",
            serde_json::json!({"style":legacy_unstyled}),
        ),
        true
    );
    let settings = api(
        &engine,
        "settings",
        serde_json::json!({"settings":{"preview_rows":50,"last_style":legacy}}),
    );
    assert_eq!(settings["settings"]["output_mode"], "table");
    assert_eq!(settings["initial_style"], normalized);
    let document_settings = api(
        &engine,
        "settings",
        serde_json::json!({"settings":{"output_mode":"document"}}),
    );
    assert_eq!(document_settings["settings"]["output_mode"], "document");
    for invalid in [
        serde_json::json!("complete"),
        serde_json::json!(false),
        serde_json::json!(0),
    ] {
        assert!(engine
            .request(
                serde_json::json!({"version":1,"op":"settings","settings":{"output_mode":invalid}}),
                &|| false
            )
            .is_err());
    }
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("legacy-presets.json");
    std::fs::write(
        &path,
        serde_json::to_vec(&serde_json::json!({"Teaching":legacy})).unwrap(),
    )
    .unwrap();
    let loaded = api(&engine, "load_presets", serde_json::json!({"path":path}));
    assert_eq!(loaded["Teaching"], normalized);
    for attack in [
        "</style><script>alert(1)</script>",
        "red; background:url(x)",
    ] {
        assert!(engine.request(serde_json::json!({"version":1,"op":"validate_style","style":{"first_column_enabled":true,"first_column_bg":attack}}), &|| false).is_err());
    }
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

    // Symlink to a regular file is supported
    let regular = directory.path().join("valid.csv");
    std::fs::write(&regular, "h1,h2\nv1,v2\n").unwrap();
    #[cfg(unix)]
    {
        let symlink_path = directory.path().join("symlink_valid.csv");
        std::os::unix::fs::symlink(&regular, &symlink_path).unwrap();
        let table = csv::read(&symlink_path, Delimiter::Comma, 65536, &|| false).unwrap();
        assert_eq!(table.rows.len(), 1);
        assert_eq!(table.header, vec!["h1", "h2"]);

        // FIFO is rejected immediately without hanging
        let fifo_path = directory.path().join("test.fifo");
        let c_path = std::ffi::CString::new(fifo_path.to_str().unwrap()).unwrap();
        let ret = unsafe { libc::mkfifo(c_path.as_ptr(), 0o600) };
        assert_eq!(ret, 0);

        let err = csv::read(&fifo_path, Delimiter::Comma, 65536, &|| false);
        assert!(err.is_err(), "Opening FIFO must fail");
        assert!(
            err.unwrap_err()
                .to_string()
                .contains("Choose a regular CSV or TSV file."),
            "Must reject FIFO with regular file message"
        );

        // Symlink to FIFO is also rejected without hanging
        let symlink_fifo = directory.path().join("symlink.fifo");
        std::os::unix::fs::symlink(&fifo_path, &symlink_fifo).unwrap();
        let err2 = csv::read(&symlink_fifo, Delimiter::Comma, 65536, &|| false);
        assert!(err2.is_err(), "Opening symlink to FIFO must fail");
    }
}

#[test]
fn cancellation_across_long_ordinary_byte_run() {
    use std::sync::atomic::{AtomicUsize, Ordering};

    // Construct a long unquoted and long quoted field (> 150 KB)
    let long_unquoted = "a".repeat(180_000);
    let long_quoted = "b".repeat(180_000);
    let csv_content = format!("H1,H2\n{},\"{}\"\n", long_unquoted, long_quoted);

    // Verify uncancelled parse succeeds and preserves full content
    let table = csv::parse(csv_content.as_bytes(), Delimiter::Comma, &|| false).unwrap();
    assert_eq!(table.rows[0][0].len(), 180_000);
    assert_eq!(table.rows[0][1].len(), 180_000);

    // Verify cancellation during long unquoted run halts quickly
    let counter = AtomicUsize::new(0);
    let res = csv::parse(csv_content.as_bytes(), Delimiter::Comma, &|| {
        let count = counter.fetch_add(1, Ordering::SeqCst);
        count >= 2 // cancel after at most 2 checkpoints
    });
    assert!(matches!(res, Err(Error::Cancelled)));

    // Verify prompt cancellation via read() from disk as well
    let dir = tempfile::tempdir().unwrap();
    let file_path = dir.path().join("long.csv");
    std::fs::write(&file_path, &csv_content).unwrap();

    let counter2 = AtomicUsize::new(0);
    let res2 = csv::read(&file_path, Delimiter::Comma, 65536, &|| {
        let count = counter2.fetch_add(1, Ordering::SeqCst);
        count >= 2
    });
    assert!(matches!(res2, Err(Error::Cancelled)));
}
