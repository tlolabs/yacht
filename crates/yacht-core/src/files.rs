use crate::{
    check,
    csv::{self, Delimiter},
    html, invalid,
    style::Style,
    table::Table,
    Cancel, Result,
};
use serde::Serialize;
use std::{
    collections::BTreeMap,
    io::{BufWriter, Write},
    path::{Path, PathBuf},
};
pub type Presets = BTreeMap<String, Style>;
pub fn default_output(input: &Path) -> PathBuf {
    input.with_extension("html")
}
/// All publication is staged on the destination filesystem. persist_noclobber
/// enforces no replacement even if another writer wins a race after the dialog.
fn atomic(
    path: &Path,
    overwrite: bool,
    cancel: Cancel<'_>,
    write: impl FnOnce(&mut BufWriter<&mut std::fs::File>) -> Result<()>,
) -> Result<()> {
    check(cancel)?;
    let parent = path
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."));
    std::fs::create_dir_all(parent)?;
    let mut temp = tempfile::Builder::new()
        .prefix(".yacht-")
        .suffix(".tmp")
        .tempfile_in(parent)?;
    {
        let mut sink = BufWriter::with_capacity(65536, temp.as_file_mut());
        write(&mut sink)?;
        sink.flush()?;
    }
    temp.as_file().sync_all()?;
    check(cancel)?;
    let result = if overwrite {
        temp.persist(path)
    } else {
        temp.persist_noclobber(path)
    };
    result.map_err(|e| {
        if e.error.kind() == std::io::ErrorKind::AlreadyExists {
            invalid(format!(
                "{} already exists. Choose another name or explicitly allow replacement.",
                path.file_name().unwrap_or_default().to_string_lossy()
            ))
        } else {
            crate::Error::Io(e.error)
        }
    })?;
    Ok(())
}
pub fn export(
    table: &Table,
    style: &Style,
    path: &Path,
    overwrite: bool,
    cancel: Cancel<'_>,
) -> Result<()> {
    style.validate()?;
    atomic(path, overwrite, cancel, |sink| {
        html::write(table, style, None, sink, cancel)
    })
}
pub fn convert(
    input: &Path,
    output: &Path,
    style: &Style,
    delimiter: Delimiter,
    overwrite: bool,
    cancel: Cancel<'_>,
) -> Result<()> {
    // Canonicalization follows symlinks; hard-link replacement changes only the
    // destination directory entry and therefore never truncates the source inode.
    let source = input.canonicalize()?;
    let target = output.canonicalize().ok().or_else(|| {
        output
            .parent()
            .unwrap_or(Path::new("."))
            .canonicalize()
            .ok()
            .map(|p| p.join(output.file_name().unwrap_or_default()))
    });
    if target.as_ref() == Some(&source) {
        return Err(invalid("The output must not replace the input CSV."));
    }
    let table = csv::read(input, delimiter, 65536, cancel)?;
    export(&table, style, output, overwrite, cancel)
}
#[derive(Serialize, Debug)]
pub struct BatchResult {
    pub input: PathBuf,
    pub output: PathBuf,
    pub error: Option<String>,
}
pub fn batch(
    inputs: &[PathBuf],
    style: &Style,
    delimiter: Delimiter,
    infer_tsv: bool,
    overwrite: bool,
    cancel: Cancel<'_>,
    mut progress: impl FnMut(&BatchResult),
) -> Result<Vec<BatchResult>> {
    style.validate()?;
    let mut results = vec![];
    for input in inputs {
        check(cancel)?;
        let output = default_output(input);
        let result = convert(
            input,
            &output,
            style,
            if infer_tsv {
                delimiter.for_path(input)
            } else {
                delimiter
            },
            overwrite,
            cancel,
        );
        if matches!(result, Err(crate::Error::Cancelled)) {
            return Err(crate::Error::Cancelled);
        }
        let item = BatchResult {
            input: input.clone(),
            output,
            error: result.err().map(|e| e.to_string()),
        };
        progress(&item);
        results.push(item);
    }
    Ok(results)
}
pub fn decode_presets(v: serde_json::Value) -> Result<Presets> {
    let object = v
        .as_object()
        .ok_or_else(|| invalid("Presets must be a JSON object."))?;
    object
        .iter()
        .map(|(name, value)| Ok((name.clone(), Style::from_value(value.clone())?)))
        .collect()
}
pub fn load_presets(path: &Path) -> Result<Presets> {
    match std::fs::read(path) {
        Ok(bytes) => decode_presets(serde_json::from_slice(&bytes)?),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(BTreeMap::new()),
        Err(e) => Err(e.into()),
    }
}
pub fn save_presets(path: &Path, presets: &Presets, cancel: Cancel<'_>) -> Result<()> {
    for (name, style) in presets {
        if name.trim().is_empty() || ["Default (Styled)", "Unstyled"].contains(&name.as_str()) {
            return Err(invalid(
                "Choose a nonempty name other than the built-in preset names.",
            ));
        }
        style.validate()?;
    }
    atomic(path, true, cancel, |sink| {
        serde_json::to_writer_pretty(sink, presets)?;
        Ok(())
    })
}
