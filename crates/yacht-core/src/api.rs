//! Version 1 JSON command boundary. Handles are process-local and never persisted.
use crate::{
    csv::{self, Delimiter},
    files, html, invalid, preview,
    style::{self, Style},
    table::Table,
    Cancel, Result,
};
use serde_json::{json, Value};
use std::{
    collections::HashMap,
    path::Path,
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc, Mutex,
    },
};
#[derive(Default)]
pub struct Engine {
    tables: Mutex<HashMap<u64, Arc<Table>>>,
    next: AtomicU64,
}
fn take_field(v: &mut Value, key: &str) -> Value {
    v.as_object_mut()
        .and_then(|obj| obj.remove(key))
        .unwrap_or(Value::Null)
}
fn text<'a>(v: &'a Value, key: &str) -> Result<&'a str> {
    v[key]
        .as_str()
        .ok_or_else(|| invalid(format!("Missing string: {key}")))
}
fn style(v: &mut Value) -> Result<Style> {
    match take_field(v, "style") {
        Value::Null => Ok(Style::default()),
        val => Style::from_value(val),
    }
}
fn delimiter(v: &mut Value) -> Result<Delimiter> {
    match take_field(v, "delimiter") {
        Value::Null => Ok(Delimiter::default()),
        val => Ok(serde_json::from_value(val)?),
    }
}
fn output_mode(v: &Value) -> Result<html::OutputMode> {
    match v.get("output_mode") {
        None | Some(Value::Null) => Ok(html::OutputMode::default()),
        Some(value) => Ok(serde_json::from_value(value.clone())?),
    }
}
fn count(v: &Value, key: &str, default: usize) -> Result<usize> {
    match v.get(key) {
        None | Some(Value::Null) => Ok(default),
        Some(x) => x
            .as_u64()
            .and_then(|n| usize::try_from(n).ok())
            .ok_or_else(|| invalid(format!("{key} must be nonnegative."))),
    }
}
fn string_value(text: String) -> Value {
    // Keep the exact-length storage formerly produced by serde's string copy,
    // while allowing the allocator to trim/reuse the generated allocation.
    Value::String(text.into_boxed_str().into_string())
}
fn response_capacity(value: &Value) -> usize {
    let text_bytes = match value {
        Value::String(text) => text.len(),
        Value::Object(fields) => ["html", "source"]
            .iter()
            .filter_map(|key| fields.get(*key).and_then(Value::as_str))
            .map(str::len)
            .fold(0usize, usize::saturating_add),
        _ => 0,
    };
    // Numeric document cells add about 10% in escaped attribute quotes. Keep
    // enough slack to avoid doubling these responses at the end of serialization.
    // A bounded hint, not a JSON size calculation: quote/control-heavy output
    // can still grow normally. Avoid repeatedly copying large HTML responses.
    text_bytes
        .saturating_add(text_bytes / 8)
        .saturating_add(1024)
        .min(128 * 1024 * 1024)
}
impl Engine {
    fn insert(&self, table: Table) -> Value {
        let id = self.next.fetch_add(1, Ordering::Relaxed) + 1;
        let info = json!({"handle":id,"header":table.header,"row_count":table.rows.len(),"short_row_count":table.short_row_count,"extra_row_count":table.extra_row_count,"warnings":table.warnings()});
        self.tables
            .lock()
            .unwrap_or_else(|e| e.into_inner())
            .insert(id, Arc::new(table));
        info
    }
    fn table(&self, v: &Value) -> Result<Arc<Table>> {
        self.tables
            .lock()
            .unwrap_or_else(|e| e.into_inner())
            .get(
                &v["handle"]
                    .as_u64()
                    .ok_or_else(|| invalid("Missing table handle."))?,
            )
            .cloned()
            .ok_or_else(|| invalid("Unknown or released table handle."))
    }
    pub fn request(&self, mut v: Value, cancel: Cancel<'_>) -> Result<Value> {
        crate::check(cancel)?;
        if v["version"] != 1 {
            return Err(invalid("Unsupported binding protocol version."));
        }
        let op = text(&v, "op")?.to_owned();
        let out = match op.as_str() {
            "info" => json!({"version":env!("CARGO_PKG_VERSION"),"protocol":1}),
            "settings" => {
                let settings: crate::settings::Settings = match take_field(&mut v, "settings") {
                    Value::Null => Default::default(),
                    val => serde_json::from_value(val)?,
                };
                settings.validate()?;
                json!({"settings":settings,"initial_style":settings.initial_style()})
            }
            "defaults" => json!(Style::default()),
            "unstyled" => json!(Style::unstyled()),
            "builtin_presets" => json!(style::builtin_presets()),
            "normalize_style" => json!(style(&mut v)?),
            "validate_style" => {
                style(&mut v)?.validate()?;
                Value::Null
            }
            "safe_color" => json!(style::safe_color(text(&v, "text")?)),
            "is_unstyled" => json!(style(&mut v)? == Style::unstyled()),
            "escape" => string_value(html::escape(text(&v, "text")?)),
            "numeric" => json!(html::is_numeric(text(&v, "text")?)),
            "sample" => self.insert(Table::sample()),
            "records" => self.insert(Table::new(serde_json::from_value(take_field(
                &mut v, "records",
            ))?)?),
            "parse" => {
                let bytes: Vec<u8> = serde_json::from_value(take_field(&mut v, "bytes"))?;
                let d = delimiter(&mut v)?;
                self.insert(csv::parse(&bytes, d, cancel)?)
            }
            "read" => {
                let path = text(&v, "path")?.to_owned();
                let d = delimiter(&mut v)?;
                let chunk_size = count(&v, "chunk_size", 65536)?;
                self.insert(csv::read(Path::new(&path), d, chunk_size, cancel)?)
            }
            "release" => {
                self.tables
                    .lock()
                    .unwrap_or_else(|e| e.into_inner())
                    .remove(&v["handle"].as_u64().unwrap_or(0));
                Value::Null
            }
            "rows" => json!(self.table(&v)?.rows),
            "preview" => {
                let s = style(&mut v)?;
                let result = preview::generate_with_renderer(
                    self.table(&v)?.as_ref(),
                    &s,
                    count(&v, "limit", 200)?,
                    output_mode(&v)?,
                    v["inline_preview"].as_bool().unwrap_or(false),
                    cancel,
                )?;
                Value::Object(
                    [
                        ("html".into(), string_value(result.html)),
                        ("source".into(), string_value(result.source)),
                        ("row_count".into(), Value::from(result.row_count)),
                        (
                            "source_truncated".into(),
                            Value::Bool(result.source_truncated),
                        ),
                        (
                            "preview_unavailable".into(),
                            Value::Bool(result.preview_unavailable),
                        ),
                    ]
                    .into_iter()
                    .collect(),
                )
            }
            "html" => {
                let s = style(&mut v)?;
                string_value(html::output(
                    self.table(&v)?.as_ref(),
                    &s,
                    Some(count(&v, "limit", usize::MAX)?),
                    output_mode(&v)?,
                    cancel,
                )?)
            }
            "default_output" => json!(files::default_output(Path::new(text(&v, "path")?))),
            "export" => {
                let s = style(&mut v)?;
                files::export_with_mode(
                    self.table(&v)?.as_ref(),
                    &s,
                    Path::new(text(&v, "path")?),
                    v["overwrite"].as_bool().unwrap_or(false),
                    output_mode(&v)?,
                    cancel,
                )?;
                Value::Null
            }
            "convert" => {
                let s = style(&mut v)?;
                let d = delimiter(&mut v)?;
                files::convert_with_mode(
                    Path::new(text(&v, "input")?),
                    Path::new(text(&v, "output")?),
                    &s,
                    d,
                    v["overwrite"].as_bool().unwrap_or(false),
                    output_mode(&v)?,
                    cancel,
                )?;
                Value::Null
            }
            "batch" => {
                let inputs: Vec<std::path::PathBuf> =
                    serde_json::from_value(take_field(&mut v, "inputs"))?;
                let s = style(&mut v)?;
                let d = delimiter(&mut v)?;
                json!(files::batch_with_options(
                    &inputs,
                    &s,
                    files::BatchOptions {
                        delimiter: d,
                        infer_tsv: v["infer_tsv"].as_bool().unwrap_or(true),
                        overwrite: v["overwrite"].as_bool().unwrap_or(false),
                        output_mode: output_mode(&v)?,
                    },
                    cancel,
                    |_| {}
                )?)
            }
            "load_presets" => json!(files::load_presets(Path::new(text(&v, "path")?))?),
            "save_presets" => {
                let path = text(&v, "path")?.to_owned();
                let presets = files::decode_presets(take_field(&mut v, "presets"))?;
                files::save_presets(Path::new(&path), &presets, cancel)?;
                Value::Null
            }
            _ => return Err(invalid("Unknown binding operation.")),
        };
        Ok(out)
    }
    pub fn dispatch(&self, bytes: &[u8], cancel: Cancel<'_>) -> Vec<u8> {
        let result = serde_json::from_slice(bytes)
            .map_err(crate::Error::from)
            .and_then(|v| self.request(v, cancel));
        let capacity = result.as_ref().map(response_capacity).unwrap_or(1024);
        let mut out = Vec::with_capacity(capacity);
        match result {
            Ok(value) => {
                out.extend_from_slice(b"{\"ok\":");
                let _ = serde_json::to_writer(&mut out, &value);
                out.push(b'}');
            }
            Err(e) => {
                let failure = crate::Failure::from(e);
                out.extend_from_slice(b"{\"error\":");
                let _ = serde_json::to_writer(&mut out, &failure);
                out.push(b'}');
            }
        }
        out
    }
}
