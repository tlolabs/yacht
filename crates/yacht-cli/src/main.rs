use std::{
    path::PathBuf,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
};
use yacht_core::{csv::Delimiter, files, invalid, style::Style, Result};
const HELP: &str = "YACHT — Yet Another CSV HTML Translator
Usage: yacht input.csv [more.csv ...] [options]
  -o, --output PATH       Output for one input (default: beside CSV)
  --overwrite            Explicitly allow replacing existing HTML
  --delimiter VALUE      comma, tab, semicolon, pipe (default: comma)
  --unstyled             Omit all styling
  --table-class TEXT      CSS classes
  --font-family TEXT      Comma-separated font names
  --font-size N           Font size in pixels
  --cell-padding N        Cell padding in pixels
  --border-width N        Border width in pixels
  --border-style STYLE    solid, dashed, dotted, double, none, ...
  --border-color COLOR    CSS color
  --header-bg COLOR       Header background
  --header-text-color COLOR
  --body-bg COLOR         Body background
  --zebra BOOL            true/false, yes/no, y/n, on/off, 1/0
  --zebra-bg COLOR        Alternating row color
  --hover BOOL            Hover highlight
  --hover-bg COLOR        Hover background
  --border-collapse MODE  collapse or separate
  --border-spacing N      Border spacing in pixels
  --                     End options; remaining arguments are paths";
fn path(text: &str) -> PathBuf {
    let home = std::env::var_os("HOME").or_else(|| std::env::var_os("USERPROFILE"));
    if text == "~" {
        if let Some(home) = home {
            return home.into();
        }
    } else if let Some(rest) = text.strip_prefix("~/") {
        if let Some(home) = home {
            return PathBuf::from(home).join(rest);
        }
    }
    PathBuf::from(text)
}
fn run(args: Vec<String>, cancel: &dyn Fn() -> bool) -> Result<()> {
    if args.is_empty() || args.iter().any(|a| a == "--help" || a == "-h") {
        println!("{HELP}");
        return Ok(());
    }
    let mut style = serde_json::to_value(Style::default())?;
    let (mut inputs, mut output, mut delimiter, mut overwrite, mut positional) =
        (vec![], None, Delimiter::Comma, false, false);
    let mut args = args.into_iter();
    while let Some(arg) = args.next() {
        if positional || !arg.starts_with('-') {
            inputs.push(arg);
            continue;
        }
        match arg.as_str() {
            "--" => {
                positional = true;
                continue;
            }
            "--overwrite" => {
                overwrite = true;
                continue;
            }
            "--unstyled" => {
                style = serde_json::to_value(Style::unstyled())?;
                continue;
            }
            _ => {}
        }
        let (flag, value) = if let Some((flag, value)) = arg.split_once('=') {
            (flag.to_owned(), value.to_owned())
        } else {
            let value = args
                .next()
                .ok_or_else(|| invalid(format!("Missing value for {arg}.")))?;
            (arg, value)
        };
        if flag == "-o" || flag == "--output" {
            output = Some(value);
            continue;
        }
        if flag == "--delimiter" {
            delimiter = serde_json::from_value(serde_json::json!(value))
                .map_err(|_| invalid(format!("Unknown delimiter: {value}")))?;
            continue;
        }
        let key = match flag.as_str() {
            "--font-size" => "font_size_px",
            "--cell-padding" => "cell_padding_px",
            "--border-width" => "border_width_px",
            "--border-spacing" => "border_spacing_px",
            "--zebra" => "zebra_enabled",
            "--hover" => "hover_enabled",
            "--table-class" => "table_class",
            "--font-family" => "font_family",
            "--border-style" => "border_style",
            "--border-color" => "border_color",
            "--header-bg" => "header_bg",
            "--header-text-color" => "header_text_color",
            "--body-bg" => "body_bg",
            "--zebra-bg" => "zebra_bg",
            "--hover-bg" => "hover_bg",
            "--border-collapse" => "border_collapse",
            _ => return Err(invalid(format!("Unknown option: {flag}. See --help."))),
        };
        style[key] = if key.ends_with("_px") {
            serde_json::json!(value
                .parse::<i64>()
                .map_err(|_| invalid(format!("{flag} requires an integer.")))?)
        } else if key.ends_with("_enabled") {
            serde_json::json!(match value.to_lowercase().as_str() {
                "1" | "true" | "yes" | "y" | "on" => true,
                "0" | "false" | "no" | "n" | "off" => false,
                _ => return Err(invalid(format!("Invalid boolean: {value}"))),
            })
        } else {
            serde_json::json!(value)
        };
    }
    if inputs.is_empty() {
        return Err(invalid("Choose at least one input CSV."));
    }
    if output.is_some() && inputs.len() != 1 {
        return Err(invalid("--output can only be used with one input CSV."));
    }
    let style = Style::from_value(style)?;
    style.validate()?;
    let mut failures = 0;
    for input in inputs {
        yacht_core::check(cancel)?;
        let source = path(&input);
        let target = output
            .as_deref()
            .map(path)
            .unwrap_or_else(|| files::default_output(&source));
        match files::convert(&source, &target, &style, delimiter, overwrite, cancel) {
            Ok(()) => println!(
                "Converted: {} -> {}",
                source.canonicalize().unwrap_or(source).display(),
                target.canonicalize().unwrap_or(target).display()
            ),
            Err(yacht_core::Error::Cancelled) => return Err(yacht_core::Error::Cancelled),
            Err(e) => {
                failures += 1;
                eprintln!("{input}: {e}");
            }
        }
    }
    if failures > 0 {
        return Err(invalid(format!("{failures} file(s) failed.")));
    }
    Ok(())
}
fn main() {
    let cancelled = Arc::new(AtomicBool::new(false));
    let signal = cancelled.clone();
    if let Err(e) = ctrlc::set_handler(move || signal.store(true, Ordering::Relaxed)) {
        eprintln!("yacht: Cannot install cancellation handler: {e}");
        std::process::exit(1);
    }
    if let Err(e) = run(std::env::args().skip(1).collect(), &|| {
        cancelled.load(Ordering::Relaxed)
    }) {
        eprintln!("yacht: {e}");
        std::process::exit(if matches!(e, yacht_core::Error::Cancelled) {
            130
        } else {
            1
        });
    }
}
