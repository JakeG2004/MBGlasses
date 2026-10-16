use std::path::PathBuf;

use band_lighthshow::catalog::Catalog;
use band_lighthshow::link::LinkChoice;
use band_lighthshow::patterns::Packet;
use band_lighthshow::runtime::{self, Sequencer};
use band_lighthshow::term::{Terminal, UiState};
use clap::Parser;
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

    /// Directory containing `frames/` and `shows/`
    #[arg(long, default_value = "data")]
    data_dir: PathBuf,

    /// Run the sequencer without opening a broadcast device
    #[arg(long)]
    dry_run: bool,
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    if args.list_devices {
        runtime::list_devices()?;
        return Ok(());
    }

    let catalog = Catalog::load(&args.data_dir)?;

    let device = if args.dry_run {
        None
    } else {
        Some(runtime::open_device(args.link.into(), args.device).await?)
    };
    let device_connected = device.is_some();
    let port_label = device
        .as_ref()
        .map(band_lighthshow::link::BroadcastLink::label)
        .unwrap_or("DRY RUN")
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

    let input = tokio::task::spawn_blocking(move || runtime::input_loop(key_tx));
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
    use super::*;

    #[test]
    fn link_flag_parses() {
        let args = Args::parse_from(["band-lighthshow", "--link", "esp32h2", "--dry-run"]);
        assert_eq!(args.link, LinkArg::Esp32h2);
        assert!(args.dry_run);

        let auto = Args::parse_from(["band-lighthshow"]);
        assert_eq!(auto.link, LinkArg::Auto);
        assert_eq!(LinkChoice::from(auto.link), LinkChoice::Auto);
    }
}
