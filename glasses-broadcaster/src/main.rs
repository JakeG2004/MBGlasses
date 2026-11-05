use std::path::{Path, PathBuf};

use anyhow::{Context, bail};
use clap::Parser;
use glasses_broadcaster::catalog::Catalog;
use glasses_broadcaster::link::LinkChoice;
use glasses_broadcaster::patterns::Packet;
use glasses_broadcaster::runtime::{self, Sequencer};
use glasses_broadcaster::term::{Terminal, UiState};
use tokio::sync::{mpsc, watch};

#[derive(Copy, Clone, Debug, PartialEq, Eq, clap::ValueEnum)]
enum LinkArg {
    /// Use the FTDI FT232 if one is connected, otherwise an ESP32-H2
    Auto,
    /// Original FTDI FT232 UART broadcaster at 57600 8N1
    Ftdi,
    /// ESP32-H2-MINI-1 native USB (VID 0x303A PID 0x1001)
    Esp32h2,
}

impl From<LinkArg> for LinkChoice {
    fn from(value: LinkArg) -> Self {
        match value {
            LinkArg::Auto => Self::Auto,
            LinkArg::Ftdi => Self::Ftdi,
            LinkArg::Esp32h2 => Self::Esp32H2,
        }
    }
}

#[derive(Parser, Debug)]
#[command(version, about = "Marching band light-show operator console")]
struct Args {
    /// List FTDI and ESP32-H2 broadcast devices and exit
    #[arg(long)]
    list_devices: bool,

    /// Which USB broadcaster to open
    #[arg(long, value_enum, default_value_t = LinkArg::Auto)]
    link: LinkArg,

    /// Index into the selected link's device list
    #[arg(long, default_value_t = 0)]
    device: usize,

    /// Override the installed directory containing `frames/` and `shows/`
    #[arg(long)]
    data_dir: Option<PathBuf>,

    /// Validate show/frame data and exit without opening USB or the terminal
    #[arg(long)]
    check_data: bool,

    /// Run the sequencer without opening a broadcast device
    #[arg(long)]
    dry_run: bool,
}

fn installed_data_dir(executable: &Path, os: &str) -> Option<PathBuf> {
    let directory = executable.parent()?;
    match os {
        "linux" => Some(directory.parent()?.join("share/glasses-broadcaster/data")),
        "macos" => Some(directory.parent()?.join("Resources/data")),
        "windows" => Some(directory.join("data")),
        _ => None,
    }
}

fn resolve_default_data_dir(
    executable: &Path,
    working_dir: &Path,
    os: &str,
) -> anyhow::Result<PathBuf> {
    let installed = installed_data_dir(executable, os);
    if let Some(directory) = &installed
        && directory.is_dir()
    {
        return Ok(directory.clone());
    }

    let development = working_dir.join("data");
    if development.is_dir() {
        return Ok(development);
    }

    bail!(
        "show/frame data not found (installed: {}; development: {}). Reinstall the application or use --data-dir DIRECTORY",
        installed.as_ref().map_or_else(
            || "unsupported installation layout".to_owned(),
            |directory| directory.display().to_string()
        ),
        development.display()
    );
}

fn resolve_data_dir(explicit: Option<&Path>) -> anyhow::Result<PathBuf> {
    if let Some(directory) = explicit {
        return Ok(directory.to_owned());
    }

    resolve_default_data_dir(
        &std::env::current_exe().context("locating the application executable")?,
        &std::env::current_dir().context("locating the current working directory")?,
        std::env::consts::OS,
    )
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    run(Args::parse()).await
}

async fn run(args: Args) -> anyhow::Result<()> {
    if args.list_devices {
        runtime::list_devices()?;
        return Ok(());
    }

    let data_dir = resolve_data_dir(args.data_dir.as_deref())?;
    let catalog = Catalog::load(&data_dir)
        .with_context(|| format!("loading show/frame data from {}", data_dir.display()))?;
    if args.check_data {
        println!("Show/frame data is valid: {}", data_dir.display());
        return Ok(());
    }

    let device = if args.dry_run {
        None
    } else {
        Some(runtime::open_device(args.link.into(), args.device).await?)
    };
    let device_connected = device.is_some();
    let port_label = device
        .as_ref()
        .map_or("DRY RUN", glasses_broadcaster::link::BroadcastLink::label)
        .to_owned();

    let term = match Terminal::new() {
        Ok(term) => term,
        Err(err) => {
            if let Some(link) = device {
                link.close().await;
            }
            return Err(err);
        }
    };

    let ui = UiState {
        device_connected,
        port_label,
        ..UiState::default()
    };

    let (shutdown_tx, shutdown_rx) = watch::channel(false);
    let (key_tx, key_rx) = mpsc::channel(32);
    let (pkt_tx, pkt_rx) = mpsc::channel::<Packet>(8);
    let (usb_tx, usb_rx) = mpsc::channel(8);
    let (ui_tx, ui_rx) = watch::channel(ui.clone());

    let input = tokio::task::spawn_blocking(move || runtime::input_loop(&key_tx));
    let tui = tokio::spawn(runtime::tui_task(term, ui_rx, shutdown_rx));
    let usb = tokio::spawn(runtime::usb_task(device, pkt_rx, usb_tx));

    let sequencer = Sequencer::new(catalog, device_connected, ui);
    runtime::sequencer_task(sequencer, key_rx, pkt_tx, usb_rx, ui_tx, shutdown_tx).await;

    let _ = usb.await;
    let _ = tui.await;
    let _ = input.await;
    println!("End of program.");
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::fs;
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::time::{SystemTime, UNIX_EPOCH};

    use super::*;

    static NEXT_TEST_DIRECTORY: AtomicU64 = AtomicU64::new(0);

    struct TestDir(PathBuf);

    impl TestDir {
        fn new() -> Self {
            Self::with_timestamp(
                SystemTime::now()
                    .duration_since(UNIX_EPOCH)
                    .unwrap()
                    .as_nanos(),
            )
        }

        fn with_timestamp(timestamp: u128) -> Self {
            loop {
                let directory = std::env::temp_dir().join(format!(
                    "glasses-broadcaster-data-{}-{timestamp}-{}",
                    std::process::id(),
                    NEXT_TEST_DIRECTORY.fetch_add(1, Ordering::Relaxed)
                ));
                match fs::create_dir(&directory) {
                    Ok(()) => return Self(directory),
                    Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => {}
                    Err(error) => panic!("creating temporary data directory: {error}"),
                }
            }
        }
    }

    impl Drop for TestDir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    #[test]
    fn temporary_data_directories_do_not_collide_when_clock_values_match() {
        let first = TestDir::with_timestamp(0);
        let second = TestDir::with_timestamp(0);
        assert_ne!(first.0, second.0);
    }

    #[test]
    fn link_flag_parses() {
        let args = Args::parse_from(["band-lighthshow", "--link", "esp32h2", "--dry-run"]);
        assert_eq!(args.link, LinkArg::Esp32h2);
        assert!(args.dry_run);

        let auto = Args::parse_from(["band-lighthshow"]);
        assert_eq!(auto.link, LinkArg::Auto);
        assert_eq!(LinkChoice::from(auto.link), LinkChoice::Auto);
        assert_eq!(auto.data_dir, None);
        assert!(!auto.check_data);
    }

    #[test]
    fn explicit_data_override_preserves_paths_with_spaces() {
        let args = Args::parse_from([
            "glasses-broadcaster",
            "--data-dir",
            "custom shows/my data",
            "--check-data",
        ]);
        assert!(args.check_data);
        assert_eq!(
            resolve_data_dir(args.data_dir.as_deref()).unwrap(),
            PathBuf::from("custom shows/my data")
        );
    }

    #[test]
    fn installed_layouts_take_precedence_over_working_directory() {
        let root = TestDir::new();
        let working_dir = root.0.join("unrelated directory");
        fs::create_dir_all(working_dir.join("data")).unwrap();
        for (os, executable, resources) in [
            (
                "linux",
                "linux prefix/bin/glasses-broadcaster",
                "linux prefix/share/glasses-broadcaster/data",
            ),
            (
                "macos",
                "Glasses Broadcaster.app/Contents/MacOS/glasses-broadcaster",
                "Glasses Broadcaster.app/Contents/Resources/data",
            ),
            (
                "windows",
                "Program Files/Glasses Broadcaster/glasses-broadcaster.exe",
                "Program Files/Glasses Broadcaster/data",
            ),
        ] {
            let resources = root.0.join(resources);
            fs::create_dir_all(&resources).unwrap();
            assert_eq!(
                resolve_default_data_dir(&root.0.join(executable), &working_dir, os).unwrap(),
                resources
            );
        }
    }

    #[test]
    fn development_data_is_a_fallback_only() {
        let root = TestDir::new();
        let resources = root.0.join("data");
        fs::create_dir_all(&resources).unwrap();
        assert_eq!(
            resolve_default_data_dir(&root.0.join("target/debug/app"), &root.0, "linux").unwrap(),
            resources
        );
    }

    #[test]
    fn missing_resources_explain_the_override() {
        let root = TestDir::new();
        let error =
            resolve_default_data_dir(&root.0.join("usr/bin/app"), &root.0, "linux").unwrap_err();
        assert!(error.to_string().contains("--data-dir"));
        assert!(error.to_string().contains("usr"));
    }

    #[tokio::test]
    async fn catalog_check_is_headless() {
        run(Args::parse_from([
            "glasses-broadcaster",
            "--check-data",
            "--data-dir",
            concat!(env!("CARGO_MANIFEST_DIR"), "/data"),
        ]))
        .await
        .unwrap();
    }

    #[tokio::test]
    async fn catalog_check_rejects_invalid_and_missing_data() {
        let root = TestDir::new();
        let args = || {
            Args::parse_from([
                "glasses-broadcaster".as_ref(),
                "--check-data".as_ref(),
                "--data-dir".as_ref(),
                root.0.as_os_str(),
            ])
        };
        assert!(run(args()).await.is_err());

        fs::create_dir_all(root.0.join("frames")).unwrap();
        fs::create_dir_all(root.0.join("shows")).unwrap();
        fs::write(root.0.join("frames/invalid.frame"), "invalid frame").unwrap();
        assert!(run(args()).await.is_err());
    }
}
