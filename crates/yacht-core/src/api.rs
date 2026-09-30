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
fn count(v: &Value, key: &str, default: usize) -> Result<usize> {
    match v.get(key) {
        None | Some(Value::Null) => Ok(default),
        Some(x) => x
            .as_u64()
            .and_then(|n| usize::try_from(n).ok())
            .ok_or_else(|| invalid(format!("{key} must be nonnegative."))),
    }
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
            "normalize_style" => json!(style(&mut v)?),
            "validate_style" => {
                style(&mut v)?.validate()?;
                Value::Null
            }
            "safe_color" => json!(style::safe_color(text(&v, "text")?)),
            "is_unstyled" => json!(style(&mut v)? == Style::unstyled()),
            "escape" => json!(html::escape(text(&v, "text")?)),
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
                json!(preview::generate(
                    self.table(&v)?.as_ref(),
                    &s,
                    count(&v, "limit", 200)?,
                    cancel
                )?)
            }
            "html" => {
                let s = style(&mut v)?;
                json!(html::document(
                    self.table(&v)?.as_ref(),
                    &s,
                    Some(count(&v, "limit", usize::MAX)?),
                    cancel
                )?)
            }
            "default_output" => json!(files::default_output(Path::new(text(&v, "path")?))),
            "export" => {
                let s = style(&mut v)?;
                files::export(
                    self.table(&v)?.as_ref(),
                    &s,
                    Path::new(text(&v, "path")?),
                    v["overwrite"].as_bool().unwrap_or(false),
                    cancel,
                )?;
                Value::Null
            }
            "convert" => {
                let s = style(&mut v)?;
                let d = delimiter(&mut v)?;
                files::convert(
                    Path::new(text(&v, "input")?),
                    Path::new(text(&v, "output")?),
                    &s,
                    d,
                    v["overwrite"].as_bool().unwrap_or(false),
                    cancel,
                )?;
                Value::Null
            }
            "batch" => {
                let inputs: Vec<std::path::PathBuf> =
                    serde_json::from_value(take_field(&mut v, "inputs"))?;
                let s = style(&mut v)?;
                let d = delimiter(&mut v)?;
                json!(files::batch(
                    &inputs,
                    &s,
                    d,
                    v["infer_tsv"].as_bool().unwrap_or(true),
                    v["overwrite"].as_bool().unwrap_or(false),
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
        let response = match serde_json::from_slice(bytes)
            .map_err(crate::Error::from)
            .and_then(|v| self.request(v, cancel))
        {
            Ok(value) => json!({"ok":value}),
            Err(e) => json!({"error":crate::Failure::from(e)}),
        };
        serde_json::to_vec(&response).expect("JSON response")
    }
}
