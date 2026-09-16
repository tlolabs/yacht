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
fn text<'a>(v: &'a Value, key: &str) -> Result<&'a str> {
    v[key]
        .as_str()
        .ok_or_else(|| invalid(format!("Missing string: {key}")))
}
fn style(v: &Value) -> Result<Style> {
    Style::from_value(v.get("style").cloned().unwrap_or(json!({})))
}
fn delimiter(v: &Value) -> Result<Delimiter> {
    Ok(serde_json::from_value(
        v.get("delimiter").cloned().unwrap_or(json!("comma")),
    )?)
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
    pub fn request(&self, v: Value, cancel: Cancel<'_>) -> Result<Value> {
        crate::check(cancel)?;
        if v["version"] != 1 {
            return Err(invalid("Unsupported binding protocol version."));
        }
        let out = match text(&v, "op")? {
            "info" => json!({"version":env!("CARGO_PKG_VERSION"),"protocol":1}),
            "settings" => {
                let settings: crate::settings::Settings =
                    serde_json::from_value(v.get("settings").cloned().unwrap_or(json!({})))?;
                settings.validate()?;
                json!({"settings":settings,"initial_style":settings.initial_style()})
            }
            "defaults" => json!(Style::default()),
            "unstyled" => json!(Style::unstyled()),
            "normalize_style" => json!(style(&v)?),
            "validate_style" => {
                style(&v)?.validate()?;
                Value::Null
            }
            "safe_color" => json!(style::safe_color(text(&v, "text")?)),
            "is_unstyled" => json!(style(&v)? == Style::unstyled()),
            "escape" => json!(html::escape(text(&v, "text")?)),
            "numeric" => json!(html::is_numeric(text(&v, "text")?)),
            "sample" => self.insert(Table::sample()),
            "records" => self.insert(Table::new(serde_json::from_value(v["records"].clone())?)?),
            "parse" => {
                let bytes: Vec<u8> = serde_json::from_value(v["bytes"].clone())?;
                self.insert(csv::parse(&bytes, delimiter(&v)?, cancel)?)
            }
            "read" => self.insert(csv::read(
                Path::new(text(&v, "path")?),
                delimiter(&v)?,
                count(&v, "chunk_size", 65536)?,
                cancel,
            )?),
            "release" => {
                self.tables
                    .lock()
                    .unwrap_or_else(|e| e.into_inner())
                    .remove(&v["handle"].as_u64().unwrap_or(0));
                Value::Null
            }
            "rows" => json!(self.table(&v)?.rows),
            "preview" => json!(preview::generate(
                self.table(&v)?.as_ref(),
                &style(&v)?,
                count(&v, "limit", 200)?,
                cancel
            )?),
            "html" => json!(html::document(
                self.table(&v)?.as_ref(),
                &style(&v)?,
                Some(count(&v, "limit", usize::MAX)?),
                cancel
            )?),
            "default_output" => json!(files::default_output(Path::new(text(&v, "path")?))),
            "export" => {
                files::export(
                    self.table(&v)?.as_ref(),
                    &style(&v)?,
                    Path::new(text(&v, "path")?),
                    v["overwrite"].as_bool().unwrap_or(false),
                    cancel,
                )?;
                Value::Null
            }
            "convert" => {
                files::convert(
                    Path::new(text(&v, "input")?),
                    Path::new(text(&v, "output")?),
                    &style(&v)?,
                    delimiter(&v)?,
                    v["overwrite"].as_bool().unwrap_or(false),
                    cancel,
                )?;
                Value::Null
            }
            "batch" => json!(files::batch(
                &serde_json::from_value::<Vec<std::path::PathBuf>>(v["inputs"].clone())?,
                &style(&v)?,
                delimiter(&v)?,
                v["infer_tsv"].as_bool().unwrap_or(true),
                v["overwrite"].as_bool().unwrap_or(false),
                cancel,
                |_| {}
            )?),
            "load_presets" => json!(files::load_presets(Path::new(text(&v, "path")?))?),
            "save_presets" => {
                files::save_presets(
                    Path::new(text(&v, "path")?),
                    &files::decode_presets(v["presets"].clone())?,
                    cancel,
                )?;
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
