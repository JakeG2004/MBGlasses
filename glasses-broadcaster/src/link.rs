use anyhow::{Result, bail};

use crate::esp32h2::Esp32H2;
use crate::patterns::Packet;
use crate::xbee::Xbee;

/// Which USB broadcaster the operator asked for.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LinkChoice {
    /// FTDI if one is connected, otherwise an ESP32-H2.
    Auto,
    /// Original FTDI FT232 to the `XBee`, 57600 8N1.
    Ftdi,
    /// ESP32-H2-MINI-1 native USB Serial/JTAG.
    Esp32H2,
}

/// Broadcaster selected after enumeration.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceKind {
    Ftdi,
    Esp32H2,
}

/// Open USB link that can send one color packet.
pub enum BroadcastLink {
    Ftdi(Box<Xbee>),
    Esp32H2(Esp32H2),
}

impl BroadcastLink {
    /// Dashboard label for the open link.
    #[must_use]
    pub const fn label(&self) -> &'static str {
        match self {
            Self::Ftdi(_) => "FTDI 57600 8N1",
            Self::Esp32H2(_) => "ESP32-H2 USB",
        }
    }

    /// Send one 32-channel RGB packet.
    ///
    /// # Errors
    ///
    /// Returns an error if the underlying USB write fails.
    pub async fn send(&mut self, packet: &Packet) -> Result<()> {
        match self {
            Self::Ftdi(xbee) => xbee
                .send(packet)
                .await
                .map_err(|err| anyhow::anyhow!("{err}")),
            Self::Esp32H2(link) => link.send(packet),
        }
    }

    /// Flush and release the USB device.
    pub async fn close(self) {
        match self {
            Self::Ftdi(xbee) => xbee.close().await,
            Self::Esp32H2(link) => link.close(),
        }
    }
}

/// Pick a broadcaster from what is actually connected.
///
/// Auto keeps the existing FTDI path when that dongle is present, including
/// when an Espressif debug probe is also plugged in. An ESP32-H2 is used only
/// when it is the only broadcast device, or when the operator asks for it.
///
/// # Errors
///
/// Returns an error if the requested kind is not connected.
pub fn resolve(choice: LinkChoice, ftdi_count: usize, esp_count: usize) -> Result<DeviceKind> {
    match choice {
        LinkChoice::Ftdi => {
            if ftdi_count == 0 {
                if esp_count > 0 {
                    bail!(
                        "no FTDI FT232 found, but an ESP32-H2 USB device is connected. Pass --link esp32h2"
                    );
                }
                bail!("no FTDI FT232 broadcast device found");
            }
            Ok(DeviceKind::Ftdi)
        }
        LinkChoice::Esp32H2 => {
            if esp_count == 0 {
                bail!(
                    "no ESP32-H2 USB broadcast device found (Espressif VID 0x303A PID 0x1001). Connect the module USB port and flash firmware/esp32h2-broadcaster"
                );
            }
            Ok(DeviceKind::Esp32H2)
        }
        LinkChoice::Auto => match (ftdi_count > 0, esp_count > 0) {
            (true, _) => Ok(DeviceKind::Ftdi),
            (false, true) => Ok(DeviceKind::Esp32H2),
            (false, false) => bail!(
                "no broadcast device found. Connect an FTDI FT232 or an ESP32-H2-MINI-1 (USB VID 0x303A PID 0x1001)"
            ),
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn auto_prefers_ftdi_when_both_are_present() {
        assert_eq!(resolve(LinkChoice::Auto, 1, 1).unwrap(), DeviceKind::Ftdi);
        assert_eq!(resolve(LinkChoice::Auto, 2, 0).unwrap(), DeviceKind::Ftdi);
    }

    #[test]
    fn auto_uses_esp32h2_when_it_is_the_only_device() {
        assert_eq!(
            resolve(LinkChoice::Auto, 0, 1).unwrap(),
            DeviceKind::Esp32H2
        );
    }

    #[test]
    fn explicit_choice_requires_that_device() {
        assert_eq!(
            resolve(LinkChoice::Esp32H2, 1, 1).unwrap(),
            DeviceKind::Esp32H2
        );
        assert_eq!(resolve(LinkChoice::Ftdi, 1, 0).unwrap(), DeviceKind::Ftdi);
        assert!(resolve(LinkChoice::Esp32H2, 1, 0).is_err());
        assert!(resolve(LinkChoice::Ftdi, 0, 1).is_err());
        assert!(resolve(LinkChoice::Auto, 0, 0).is_err());
    }
}
