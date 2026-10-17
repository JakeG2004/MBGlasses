use std::future::pending;
use std::time::{Duration, Instant};

use anyhow::{Context, Result};
use crossterm::event::{self, Event, KeyCode, KeyEventKind, KeyModifiers};
use nusb::DeviceInfo;
use tokio::sync::{mpsc, watch};

use crate::catalog::Catalog;
use crate::engine::Player;
use crate::esp32h2::Esp32H2;
use crate::link::{BroadcastLink, DeviceKind, LinkChoice, resolve};
use crate::patterns::{DARK, Packet};
use crate::term::{LoopStatus, Terminal, UiState, get_categories};
use crate::xbee::Xbee;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyCmd {
    Char(char),
    Quit,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum UsbReport {
    Ok,
    Err(String),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SeqOutcome {
    Continue,
    Quit,
}

/// Owns show playback, brightness, and the dashboard snapshot.
pub struct Sequencer {
    catalog: Catalog,
    player: Player,
    ui: UiState,
}

impl Sequencer {
    #[must_use]
    pub fn new(catalog: Catalog, device_connected: bool, mut ui: UiState) -> Self {
        ui.device_connected = device_connected;
        ui.categories = get_categories(&catalog);
        if catalog.is_empty() {
            ui.status_message = Some("No shows loaded — idle".to_owned());
        }
        Self {
            catalog,
            player: Player::new(),
            ui,
        }
    }

    #[must_use]
    pub fn snapshot(&self) -> UiState {
        self.ui.clone()
    }

    #[must_use]
    pub fn deadline(&self) -> Option<Instant> {
        self.player.deadline()
    }

    /// Handle a key immediately, even mid-cue.
    pub fn on_key(&mut self, cmd: KeyCmd, now: Instant) -> (SeqOutcome, Option<Packet>) {
        match cmd {
            KeyCmd::Quit => {
                self.player.stop();
                self.idle_ui(None);
                (SeqOutcome::Quit, None)
            }
            KeyCmd::Char('.') => {
                self.player.stop();
                self.ui.last_key = Some('.');
                self.idle_ui(Some("Shutting down...".to_owned()));
                (SeqOutcome::Quit, None)
            }
            KeyCmd::Char(',') => {
                self.ui.last_key = Some(',');
                self.player.stop();
                self.idle_ui(Some("Stopped".to_owned()));
                (SeqOutcome::Continue, Some(self.note_packet(DARK)))
            }
            KeyCmd::Char('<') => {
                self.ui.last_key = Some('<');
                self.player.dimmer();
                self.ui.status_message = Some(format!("Brightness {}", self.player.brightness()));
                self.ui.is_error = false;
                (SeqOutcome::Continue, None)
            }
            KeyCmd::Char('>') => {
                self.ui.last_key = Some('>');
                self.player.brighter();
                self.ui.status_message = Some(format!("Brightness {}", self.player.brightness()));
                self.ui.is_error = false;
                (SeqOutcome::Continue, None)
            }
            KeyCmd::Char(key) => {
                self.ui.last_key = Some(key);
                self.start_show(key, now)
            }
        }
    }

    /// Advance the player when its deadline is reached.
    pub fn on_tick(&mut self, now: Instant) -> Option<Packet> {
        match self.player.advance(&self.catalog, now) {
            Ok(Some(step)) => {
                self.sync_routine_ui();
                Some(self.note_packet(step.packet))
            }
            Ok(None) => {
                if !self.player.is_running() {
                    self.idle_ui(self.ui.status_message.clone());
                }
                None
            }
            Err(err) => {
                self.ui.is_error = true;
                self.ui.status_message = Some(err.to_string());
                None
            }
        }
    }

    pub fn on_usb(&mut self, report: UsbReport) {
        match report {
            UsbReport::Ok => {
                if self.ui.is_error {
                    self.ui.is_error = false;
                    self.ui.status_message = Some("USB recovered".to_owned());
                }
            }
            UsbReport::Err(msg) => {
                self.ui.is_error = true;
                self.ui.status_message = Some(msg);
            }
        }
    }

    fn start_show(&mut self, key: char, now: Instant) -> (SeqOutcome, Option<Packet>) {
        let Some(show) = self.catalog.show_for_key(key).cloned() else {
            return (SeqOutcome::Continue, None);
        };
        match self.player.start(&show, &self.catalog, now) {
            Ok(Some(step)) => {
                self.sync_routine_ui();
                self.ui.status_message = Some(format!("Playing {}", show.name));
                self.ui.is_error = false;
                (SeqOutcome::Continue, Some(self.note_packet(step.packet)))
            }
            Ok(None) => {
                self.idle_ui(Some(format!("{} produced no packets", show.name)));
                (SeqOutcome::Continue, None)
            }
            Err(err) => {
                self.ui.is_error = true;
                self.ui.status_message = Some(err.to_string());
                (SeqOutcome::Continue, None)
            }
        }
    }

    fn sync_routine_ui(&mut self) {
        self.ui.current_routine = self.player.show_name().to_owned();
        self.ui.loop_status = if self.player.is_looping() {
            LoopStatus::Looping
        } else {
            LoopStatus::Idle
        };
    }

    fn idle_ui(&mut self, message: Option<String>) {
        "IDLE".clone_into(&mut self.ui.current_routine);
        self.ui.loop_status = LoopStatus::Idle;
        self.ui.status_message =
            message.or_else(|| Some("System Ready - Listening for commands".to_owned()));
    }

    fn note_packet(&mut self, packet: Packet) -> Packet {
        self.ui.last_packet = packet;
        self.ui.packet_count = self.ui.packet_count.saturating_add(1);
        packet
    }
}

/// Print FTDI and ESP32-H2 broadcast devices for `--list-devices`.
///
/// # Errors
///
/// Returns an error if either USB enumeration fails.
pub fn list_devices() -> Result<()> {
    let ftdi = Xbee::enumerate()?;
    let esp = Esp32H2::enumerate()?;

    println!(
        "FTDI FT232 (VID {vid:04x} PID {pid:04x}):",
        vid = crate::xbee::VID,
        pid = crate::xbee::PID
    );
    if ftdi.is_empty() {
        println!("  (none)");
    }
    for (index, dev) in ftdi.iter().enumerate() {
        print_device(index, dev);
    }

    println!(
        "ESP32-H2 USB (VID {vid:04x} PID {pid:04x}):",
        vid = crate::esp32h2::VID,
        pid = crate::esp32h2::PID
    );
    if esp.is_empty() {
        println!("  (none)");
    }
    for (index, port) in esp.iter().enumerate() {
        println!(
            "  [{index}] {}  {} {} serial={}",
            port.path, port.manufacturer, port.product, port.serial_number
        );
    }
    Ok(())
}

/// Open the broadcaster selected by `choice` at `index` within that kind.
///
/// # Errors
///
/// Returns an error if enumeration fails, the requested device is missing, or
/// the index is out of range.
pub async fn open_device(choice: LinkChoice, index: usize) -> Result<BroadcastLink> {
    let ftdi = match Xbee::enumerate() {
        Ok(devices) => devices,
        Err(err) if choice == LinkChoice::Esp32H2 => {
            eprintln!("warning: FTDI enumeration failed: {err}");
            Vec::new()
        }
        Err(err) => return Err(err),
    };
    let esp = match Esp32H2::enumerate() {
        Ok(devices) => devices,
        Err(err) if choice == LinkChoice::Ftdi => {
            eprintln!("warning: ESP32-H2 enumeration failed: {err}");
            Vec::new()
        }
        Err(err) => return Err(err),
    };

    let kind = resolve(choice, ftdi.len(), esp.len())?;
    if choice == LinkChoice::Auto && !ftdi.is_empty() && !esp.is_empty() {
        eprintln!(
            "both an FTDI FT232 and an ESP32-H2 are connected; using the FTDI. Pass --link esp32h2 to broadcast from the module."
        );
    }

    match kind {
        DeviceKind::Ftdi => {
            let count = ftdi.len();
            let info = ftdi.into_iter().nth(index).with_context(|| {
                format!("FTDI device index {index} is out of range ({count} found)")
            })?;
            Ok(BroadcastLink::Ftdi(Box::new(Xbee::open(info).await?)))
        }
        DeviceKind::Esp32H2 => {
            let port = esp.get(index).with_context(|| {
                format!(
                    "ESP32-H2 device index {index} is out of range ({} found)",
                    esp.len()
                )
            })?;
            Ok(BroadcastLink::Esp32H2(Esp32H2::open(&port.path)?))
        }
    }
}

fn print_device(index: usize, dev: &DeviceInfo) {
    let manufacturer = dev.manufacturer_string().unwrap_or("");
    let description = dev.product_string().unwrap_or("");
    println!("{index}: Manufacturer: {manufacturer}, Description: {description}");
}

/// Blocking keyboard loop. Only this task reads crossterm events.
pub fn input_loop(tx: &mpsc::Sender<KeyCmd>) {
    loop {
        let ready = event::poll(Duration::from_millis(50)).unwrap_or(false);
        if !ready {
            if tx.is_closed() {
                break;
            }
            continue;
        }
        let Ok(Event::Key(key_event)) = event::read() else {
            continue;
        };
        if key_event.kind != KeyEventKind::Press {
            continue;
        }
        let cmd = if key_event.modifiers.contains(KeyModifiers::CONTROL)
            && key_event.code == KeyCode::Char('c')
        {
            KeyCmd::Quit
        } else if let KeyCode::Char(c) = key_event.code {
            KeyCmd::Char(c)
        } else {
            continue;
        };
        if tx.blocking_send(cmd).is_err() {
            break;
        }
    }
}

/// Draw-only task. Owns the terminal so stdout is not shared.
pub async fn tui_task(
    mut term: Terminal,
    mut ui_rx: watch::Receiver<UiState>,
    mut shutdown: watch::Receiver<bool>,
) {
    let _ = term.draw(&ui_rx.borrow());
    loop {
        tokio::select! {
            changed = ui_rx.changed() => {
                if changed.is_err() {
                    break;
                }
                let state = ui_rx.borrow().clone();
                let _ = term.draw(&state);
            }
            _ = shutdown.changed() => {
                if *shutdown.borrow() {
                    break;
                }
            }
        }
    }
}

/// Only writer to the broadcast link. `None` is the `--dry-run` sink.
pub async fn usb_task(
    mut device: Option<BroadcastLink>,
    mut packets: mpsc::Receiver<Packet>,
    reports: mpsc::Sender<UsbReport>,
) {
    while let Some(packet) = packets.recv().await {
        let report = match device.as_mut() {
            Some(link) => match link.send(&packet).await {
                Ok(()) => UsbReport::Ok,
                Err(err) => UsbReport::Err(err.to_string()),
            },
            None => UsbReport::Ok,
        };
        if reports.send(report).await.is_err() {
            break;
        }
    }
    if let Some(link) = device.take() {
        link.close().await;
    }
}

/// Drive the player from keys and deadlines; never draws or writes USB.
pub async fn sequencer_task(
    mut sequencer: Sequencer,
    mut keys: mpsc::Receiver<KeyCmd>,
    packets: mpsc::Sender<Packet>,
    mut usb_reports: mpsc::Receiver<UsbReport>,
    ui: watch::Sender<UiState>,
    shutdown: watch::Sender<bool>,
) {
    let _ = ui.send(sequencer.snapshot());
    loop {
        let wait = sequencer
            .deadline()
            .map(|deadline| deadline.saturating_duration_since(Instant::now()));

        tokio::select! {
            cmd = keys.recv() => {
                let Some(cmd) = cmd else { break; };
                let (outcome, packet) = sequencer.on_key(cmd, Instant::now());
                send_packet(&packets, packet);
                let _ = ui.send(sequencer.snapshot());
                if outcome == SeqOutcome::Quit {
                    let _ = shutdown.send(true);
                    break;
                }
            }
            Some(report) = usb_reports.recv() => {
                sequencer.on_usb(report);
                let _ = ui.send(sequencer.snapshot());
            }
            () = sleep_optional(wait) => {
                let packet = sequencer.on_tick(Instant::now());
                send_packet(&packets, packet);
                let _ = ui.send(sequencer.snapshot());
            }
        }
    }
}

fn send_packet(tx: &mpsc::Sender<Packet>, packet: Option<Packet>) {
    if let Some(packet) = packet {
        let _ = tx.try_send(packet);
    }
}

async fn sleep_optional(wait: Option<Duration>) {
    match wait {
        Some(duration) => tokio::time::sleep(duration).await,
        None => pending::<()>().await,
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use super::*;
    use crate::catalog::{Op, Show, Wait};
    use crate::patterns::WHITE;

    fn looping_white() -> (Catalog, Show) {
        let show = Show {
            key: 'w',
            name: "White Loop".to_owned(),
            category: "Flashes & Solids".to_owned(),
            desc: String::new(),
            looping: true,
            program: vec![Op::Hold {
                frame: "white".to_owned(),
                wait: Wait::Ms(10_000),
            }],
        };
        let frames = HashMap::from([("white".to_owned(), WHITE), ("dark".to_owned(), DARK)]);
        let catalog = Catalog::new(frames, vec![show.clone()]).expect("catalog");
        (catalog, show)
    }

    #[test]
    fn comma_stops_immediately() {
        let (catalog, _) = looping_white();
        let mut seq = Sequencer::new(catalog, true, UiState::default());
        let t0 = Instant::now();
        let (outcome, packet) = seq.on_key(KeyCmd::Char('w'), t0);
        assert_eq!(outcome, SeqOutcome::Continue);
        assert_eq!(packet, Some(WHITE));
        assert_eq!(seq.snapshot().current_routine, "White Loop");

        let (outcome, packet) = seq.on_key(KeyCmd::Char(','), t0);
        assert_eq!(outcome, SeqOutcome::Continue);
        assert_eq!(packet, Some(DARK));
        let snap = seq.snapshot();
        assert_eq!(snap.current_routine, "IDLE");
        assert_eq!(snap.loop_status, LoopStatus::Idle);
        assert!(seq.deadline().is_none());
    }

    #[test]
    fn period_quits_without_waiting() {
        let (catalog, _) = looping_white();
        let mut seq = Sequencer::new(catalog, true, UiState::default());
        let t0 = Instant::now();
        let _ = seq.on_key(KeyCmd::Char('w'), t0);
        let (outcome, packet) = seq.on_key(KeyCmd::Char('.'), t0);
        assert_eq!(outcome, SeqOutcome::Quit);
        assert!(packet.is_none());
        assert_eq!(seq.snapshot().current_routine, "IDLE");
    }

    #[test]
    fn unknown_key_is_ignored() {
        let (catalog, _) = looping_white();
        let mut seq = Sequencer::new(catalog, false, UiState::default());
        let (outcome, packet) = seq.on_key(KeyCmd::Char('z'), Instant::now());
        assert_eq!(outcome, SeqOutcome::Continue);
        assert!(packet.is_none());
        assert_eq!(seq.snapshot().current_routine, "IDLE");
    }

    #[tokio::test]
    async fn dry_run_sink_acks_packets() {
        let (pkt_tx, pkt_rx) = mpsc::channel(4);
        let (rep_tx, mut rep_rx) = mpsc::channel(4);
        let usb = tokio::spawn(usb_task(None, pkt_rx, rep_tx));
        pkt_tx.send(WHITE).await.expect("send");
        let report = rep_rx.recv().await.expect("report");
        assert_eq!(report, UsbReport::Ok);
        drop(pkt_tx);
        usb.await.expect("usb task");
    }
}
