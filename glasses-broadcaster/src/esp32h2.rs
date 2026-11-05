use std::time::Duration;

use anyhow::{Context, Result};
use serialport::{DataBits, FlowControl, Parity, SerialPort, SerialPortType, StopBits};

use crate::patterns::{PACKET_LEN, Packet};

/// Espressif USB vendor ID. The ESP32-H2-MINI-1 native USB Serial/JTAG uses this.
pub const VID: u16 = 0x303A;
/// USB Serial/JTAG product ID presented by the ESP32-H2-MINI-1 USB pins.
pub const PID: u16 = 0x1001;
/// Line rate requested on open. The USB Serial/JTAG controller ignores it.
pub const BAUD: u32 = 115_200;

/// First sync byte of a host-to-module frame. Keep in step with the sketch.
pub const SYNC0: u8 = 0xA5;
/// Second sync byte of a host-to-module frame.
pub const SYNC1: u8 = 0x5A;
/// Payload length byte. Always 96 RGB bytes, one triplet per glasses channel.
pub const PAYLOAD_LEN: u8 = 96;
/// `SYNC0 SYNC1 LEN payload[96] xor`.
pub const WIRE_LEN: usize = 3 + PACKET_LEN + 1;

/// One ESP32-H2 USB Serial/JTAG port the host can open.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PortInfo {
    pub path: String,
    pub manufacturer: String,
    pub product: String,
    pub serial_number: String,
}

/// Open CDC-ACM link to an ESP32-H2-MINI-1 running the broadcaster sketch.
pub struct Esp32H2 {
    port: Box<dyn SerialPort>,
    path: String,
}

impl Esp32H2 {
    /// Serial ports whose USB identity is the ESP32-H2-MINI-1 native controller.
    ///
    /// An empty list is success. macOS dial-in `/dev/tty.*` nodes are skipped
    /// in favor of the matching `/dev/cu.*` callout node.
    ///
    /// # Errors
    ///
    /// Returns an error if the OS serial-port list cannot be read.
    pub fn enumerate() -> Result<Vec<PortInfo>> {
        let ports = serialport::available_ports().context("failed to enumerate serial ports")?;
        let mut found = Vec::new();
        for port in ports {
            let SerialPortType::UsbPort(info) = port.port_type else {
                continue;
            };
            if !is_esp32h2(info.vid, info.pid) || is_dialin_node(&port.port_name) {
                continue;
            }
            found.push(PortInfo {
                path: port.port_name,
                manufacturer: info.manufacturer.unwrap_or_default(),
                product: info.product.unwrap_or_default(),
                serial_number: info.serial_number.unwrap_or_default(),
            });
        }
        found.sort_by(|left, right| left.path.cmp(&right.path));
        Ok(found)
    }

    /// Open `path` at 8N1 and drop DTR/RTS so the USB Serial/JTAG port is less
    /// likely to hold the module in download mode.
    ///
    /// # Errors
    ///
    /// Returns an error if the serial port cannot be opened.
    pub fn open(path: &str) -> Result<Self> {
        let mut port = serialport::new(path, BAUD)
            .data_bits(DataBits::Eight)
            .parity(Parity::None)
            .stop_bits(StopBits::One)
            .flow_control(FlowControl::None)
            .timeout(Duration::from_millis(500))
            .open()
            .with_context(|| format!("failed to open ESP32-H2 USB port {path}"))?;
        let _ = port.write_data_terminal_ready(false);
        let _ = port.write_request_to_send(false);
        eprintln!("ESP32-H2 USB {path} opened");
        Ok(Self {
            port,
            path: path.to_owned(),
        })
    }

    /// Write one framed color packet.
    ///
    /// # Errors
    ///
    /// Returns an error if the serial write or flush fails.
    pub fn send(&mut self, packet: &Packet) -> Result<()> {
        let wire = frame(packet);
        let port = &mut self.port;
        tokio::task::block_in_place(|| {
            port.write_all(&wire)?;
            port.flush()?;
            Ok(())
        })
    }

    /// Check whether this ESP32-H2 serial port is still enumerated.
    ///
    /// # Errors
    ///
    /// Returns an error if the OS serial-port list cannot be read.
    pub fn is_connected(&self) -> Result<bool> {
        Ok(Self::enumerate()?.iter().any(|port| port.path == self.path))
    }

    /// Flush and drop the port.
    pub fn close(mut self) {
        let _ = self.port.flush();
    }
}

/// True for the ESP32-H2-MINI-1 native USB Serial/JTAG identity.
#[must_use]
pub const fn is_esp32h2(vid: u16, pid: u16) -> bool {
    vid == VID && pid == PID
}

/// XOR of the 96 payload bytes. The sketch rejects a frame whose checksum differs.
#[must_use]
pub const fn checksum(packet: &[u8]) -> u8 {
    let mut acc = 0u8;
    let mut index = 0;
    while index < packet.len() {
        acc ^= packet[index];
        index += 1;
    }
    acc
}

/// Host-to-module frame: `A5 5A 60` + 96 RGB bytes + XOR.
///
/// The trailing `0x00` used on the FTDI/XBee UART is not part of this frame.
/// The sketch wraps the 96 RGB bytes in the 802.15.4 header the glasses expect.
#[must_use]
pub fn frame(packet: &Packet) -> [u8; WIRE_LEN] {
    let mut wire = [0u8; WIRE_LEN];
    wire[0] = SYNC0;
    wire[1] = SYNC1;
    wire[2] = PAYLOAD_LEN;
    wire[3..3 + PACKET_LEN].copy_from_slice(packet);
    wire[WIRE_LEN - 1] = checksum(packet);
    wire
}

/// Recover one packet from a byte stream, including bytes before the sync word.
///
/// This is the host-side copy of the sketch's reader. A checksum miss drops the
/// frame and keeps scanning, so a lost byte cannot stick the link out of sync.
#[must_use]
pub fn deframe(bytes: &[u8]) -> Option<Packet> {
    let mut parser = Parser::default();
    let mut found = None;
    for byte in bytes {
        if let Some(packet) = parser.push(*byte) {
            found = Some(packet);
        }
    }
    found
}

struct Parser {
    state: u8,
    index: usize,
    checksum: u8,
    payload: Packet,
}

impl Default for Parser {
    fn default() -> Self {
        Self {
            state: 0,
            index: 0,
            checksum: 0,
            payload: [0; PACKET_LEN],
        }
    }
}

impl Parser {
    fn push(&mut self, byte: u8) -> Option<Packet> {
        match self.state {
            0 => {
                if byte == SYNC0 {
                    self.state = 1;
                }
                None
            }
            1 => {
                self.state = if byte == SYNC1 {
                    2
                } else {
                    u8::from(byte == SYNC0)
                };
                None
            }
            2 => {
                if byte == PAYLOAD_LEN {
                    self.state = 3;
                    self.index = 0;
                    self.checksum = 0;
                } else {
                    self.state = u8::from(byte == SYNC0);
                }
                None
            }
            3 => {
                self.payload[self.index] = byte;
                self.checksum ^= byte;
                self.index += 1;
                if self.index == PACKET_LEN {
                    self.state = 4;
                }
                None
            }
            _ => {
                self.state = 0;
                if byte == self.checksum {
                    Some(self.payload)
                } else {
                    None
                }
            }
        }
    }
}

fn is_dialin_node(path: &str) -> bool {
    path.contains("/tty.")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patterns::WHITE;

    #[test]
    fn identity_is_espressif_usb_serial_jtag() {
        assert!(is_esp32h2(0x303A, 0x1001));
        assert!(!is_esp32h2(0x0403, 0x6001));
        assert!(!is_esp32h2(0x303A, 0x0002));
    }

    #[test]
    fn dialin_nodes_are_not_opened() {
        assert!(is_dialin_node("/dev/tty.usbmodem1101"));
        assert!(!is_dialin_node("/dev/cu.usbmodem1101"));
        assert!(!is_dialin_node("/dev/ttyACM0"));
        assert!(!is_dialin_node("COM3"));
    }

    #[test]
    fn frame_matches_sketch_contract() {
        assert_eq!(PACKET_LEN, usize::from(PAYLOAD_LEN));
        let mut packet = WHITE;
        packet[0] = 1;
        packet[1] = 2;
        packet[2] = 3;
        packet[95] = 9;

        let wire = frame(&packet);
        assert_eq!(wire.len(), 100);
        assert_eq!(&wire[..3], &[SYNC0, SYNC1, PAYLOAD_LEN]);
        assert_eq!(&wire[3..6], &[1, 2, 3]);
        assert_eq!(wire[3 + 95], 9);
        assert_eq!(wire[99], checksum(&packet));
        assert_eq!(deframe(&wire), Some(packet));
    }

    #[test]
    fn deframe_skips_leading_garbage_and_rejects_a_bad_checksum() {
        let packet = WHITE;
        let wire = frame(&packet);
        let mut noisy = vec![0x00, 0xA5, 0x11, SYNC0];
        noisy.extend_from_slice(&wire);
        assert_eq!(deframe(&noisy), Some(packet));

        let mut bad = wire;
        bad[99] ^= 0xFF;
        assert_eq!(deframe(&bad), None);
    }
}
