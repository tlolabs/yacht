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
    let mut style = Style::default();
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
                style = Style::unstyled();
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
            delimiter = match value.as_str() {
                "comma" => Delimiter::Comma,
                "tab" => Delimiter::Tab,
                "semicolon" => Delimiter::Semicolon,
                "pipe" => Delimiter::Pipe,
                _ => return Err(invalid(format!("Unknown delimiter: {value}"))),
            };
            continue;
        }
        match flag.as_str() {
            "--font-size" => {
                style.font_size_px = value
                    .parse::<i64>()
                    .map_err(|_| invalid(format!("{flag} requires an integer.")))?;
            }
            "--cell-padding" => {
                style.cell_padding_px = value
                    .parse::<i64>()
                    .map_err(|_| invalid(format!("{flag} requires an integer.")))?;
            }
            "--border-width" => {
                style.border_width_px = value
                    .parse::<i64>()
                    .map_err(|_| invalid(format!("{flag} requires an integer.")))?;
            }
            "--border-spacing" => {
                style.border_spacing_px = value
                    .parse::<i64>()
                    .map_err(|_| invalid(format!("{flag} requires an integer.")))?;
            }
            "--zebra" => {
                style.zebra_enabled = match value.to_lowercase().as_str() {
                    "1" | "true" | "yes" | "y" | "on" => true,
                    "0" | "false" | "no" | "n" | "off" => false,
                    _ => return Err(invalid(format!("Invalid boolean: {value}"))),
                };
            }
            "--hover" => {
                style.hover_enabled = match value.to_lowercase().as_str() {
                    "1" | "true" | "yes" | "y" | "on" => true,
                    "0" | "false" | "no" | "n" | "off" => false,
                    _ => return Err(invalid(format!("Invalid boolean: {value}"))),
                };
            }
            "--table-class" => style.table_class = value,
            "--font-family" => style.font_family = value,
            "--border-style" => style.border_style = value,
            "--border-color" => style.border_color = value,
            "--header-bg" => style.header_bg = value,
            "--header-text-color" => style.header_text_color = value,
            "--body-bg" => style.body_bg = value,
            "--zebra-bg" => style.zebra_bg = value,
            "--hover-bg" => style.hover_bg = value,
            "--border-collapse" => style.border_collapse = value,
            _ => return Err(invalid(format!("Unknown option: {flag}. See --help."))),
        }
    }
    if inputs.is_empty() {
        return Err(invalid("Choose at least one input CSV."));
    }
    if output.is_some() && inputs.len() != 1 {
        return Err(invalid("--output can only be used with one input CSV."));
    }
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
