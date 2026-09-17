use anyhow::{bail, Context, Result};
use ftdi_nusb::constants::pid::FT232;
use ftdi_nusb::constants::FTDI_VID;
use ftdi_nusb::types::{DataBits, FlowControl, Interface, Parity, StopBits};
use ftdi_nusb::FtdiDevice;
use nusb::DeviceInfo;

use crate::patterns::{Packet, PACKET_LEN};

pub const VID: u16 = FTDI_VID; // 0x0403
pub const PID: u16 = FT232; // 0x6001
pub const BAUD: u32 = 57_600;
pub const WIRE_LEN: usize = PACKET_LEN + 1; // 97 bytes

pub struct Xbee {
    dev: FtdiDevice,
}

pub enum XbeeHandle {
    Real(Xbee),
    Simulated(SimXbee),
}

impl XbeeHandle {
    pub async fn send(&mut self, packet: &Packet) -> anyhow::Result<()> {
        match self {
            XbeeHandle::Real(x) => x.send(packet).await.map_err(Into::into),
            XbeeHandle::Simulated(s) => s.send(packet).await,
        }
    }

    pub async fn close(self) {
        if let XbeeHandle::Real(x) = self {
            x.close().await;
        }
    }

    pub fn is_simulated(&self) -> bool {
        matches!(self, XbeeHandle::Simulated(_))
    }
}

pub struct SimXbee {
    pub packet_count: u64,
    log: Option<std::fs::File>,
}

impl SimXbee {
    pub fn new(log_path: Option<&std::path::Path>) -> anyhow::Result<Self> {
        let log = log_path.map(std::fs::File::create).transpose()?;
        Ok(Self { packet_count: 0, log })
    }

    pub async fn send(&mut self, packet: &Packet) -> anyhow::Result<()> {
        self.packet_count += 1;
        if let Some(f) = &mut self.log {
            use std::io::Write;
            let hex_str: String = packet.iter().map(|b| format!("{:02x}", b)).collect();
            writeln!(f, "{}", hex_str)?;
        }
        Ok(())
    }
}

impl Xbee {
    /// Discovers and lists all FTDI devices matching VID/PID.
    /// Replicates the device enumeration and prints from `glasses.c`.
    pub fn list() -> Result<Vec<DeviceInfo>> {
        let devices = ftdi_nusb::find_devices(VID, PID)
            .context("failed to enumerate FTDI USB devices")?;

        if devices.is_empty() {
            eprintln!("no ftdi devices found");
            bail!("no ftdi devices found");
        }

        eprintln!("{} ftdi devices found.", devices.len());
        for (i, dev) in devices.iter().enumerate() {
            println!("Checking device: {}", i);
            let manufacturer = dev.manufacturer_string().unwrap_or("");
            let description = dev.product_string().unwrap_or("");
            println!("Manufacturer: {}, Description: {}\n", manufacturer, description);
        }

        Ok(devices)
    }

    /// Opens the specified FTDI device and configures 57600 baud, 8N1.
    pub async fn open(info: DeviceInfo) -> Result<Self> {
        let mut dev = FtdiDevice::from_device_info(info, Interface::Any)
            .await
            .context("unable to open ftdi device")?;

        eprintln!("ftdi_open successful");

        if let Err(e) = dev.set_baudrate(BAUD).await {
            eprintln!("unable to set baud rate: {}.", e);
        } else {
            println!("baudrate set.");
        }

        if let Err(e) = dev
            .set_line_property(DataBits::Eight, StopBits::One, Parity::None)
            .await
        {
            eprintln!("unable to set line parameters: {}.", e);
        } else {
            println!("line parameters set.");
        }

        let _ = dev.set_flow_control(FlowControl::Disabled).await;

        Ok(Self { dev })
    }

    /// Constructs the 97-byte wire frame: 96 RGB bytes + trailing 0x00 terminator.
    pub fn frame(packet: &Packet) -> [u8; WIRE_LEN] {
        let mut wire = [0u8; WIRE_LEN];
        wire[..PACKET_LEN].copy_from_slice(packet);
        wire[PACKET_LEN] = 0x00;
        wire
    }

    /// Transmits a 97-byte frame over the FTDI link.
    pub async fn send(&mut self, packet: &Packet) -> ftdi_nusb::Result<()> {
        let wire = Self::frame(packet);
        self.dev.write_all(&wire).await
    }

    /// Closes and releases the FTDI device context.
    pub async fn close(mut self) {
        let _ = self.dev.flush_all().await;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patterns::WHITE;

    #[test]
    fn test_wire_framing() {
        let wire = Xbee::frame(&WHITE);
        assert_eq!(wire.len(), 97);
        assert_eq!(&wire[..96], &WHITE[..]);
        assert_eq!(wire[96], 0x00);
    }
}
