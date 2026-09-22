use std::path::PathBuf;

use band_lighthshow::catalog::Catalog;
use band_lighthshow::patterns::Packet;
use band_lighthshow::runtime::{self, Sequencer};
use band_lighthshow::term::{Terminal, UiState};
use clap::Parser;
use tokio::sync::{mpsc, watch};

#[derive(Parser, Debug)]
#[command(version, about = "Marching band light-show operator console")]
struct Args {
    /// List matching FTDI devices and exit
    #[arg(long)]
    list_devices: bool,

    /// Index into the enumerated FTDI list
    #[arg(long, default_value_t = 0)]
    device: usize,

    /// Directory containing `frames/` and `shows/`
    #[arg(long, default_value = "data")]
    data_dir: PathBuf,

    /// Run the sequencer without opening FTDI
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
        Some(runtime::open_device(args.device).await?)
    };
    let device_connected = device.is_some();

    let term = match Terminal::new() {
        Ok(term) => term,
        Err(err) => {
            if let Some(xbee) = device {
                xbee.close().await;
            }
            return Err(err);
        }
    };

    let ui = UiState {
        device_connected,
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
