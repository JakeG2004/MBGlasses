# Ben's Halftime Light Show Toolkit (`band-lighthshow`)

| Crate | Version | Edition | UI Dashboard |
| :--- | :--- | :--- | :--- |
| `band-lighthshow` | `0.1.0` | `2024` | Ratatui `0.30` / Crossterm `0.29` |

## Overview

**Ben's Halftime Light Show Toolkit** (`band-lighthshow`) is a modern Rust port and interactive terminal controller for synchronized marching band light shows. Originally created by Benjamin Jeffery (University of Idaho) in C/ncurses (`glasses.c`/`ht9.c`), this software controls wearable 32-channel RGB wireless glasses during live field performances.

Built with **Rust Edition 2024**, `band-lighthshow` provides an asynchronous, zero-latency control pipeline backed by **Tokio**, a live terminal dashboard powered by **Ratatui**, and direct userspace USB-serial communication via pure-Rust **FTDI** drivers.

### Operational Workflow & Startup Flow

1. **FTDI Enumeration**: On startup, the application scans the USB bus for attached FTDI devices matching VID `0x0403` and PID `0x6001` (FT232).
2. **Device Configuration**: It claims the first available FTDI interface and configures the serial link for **57600 baud, 8N1** (8 data bits, 1 stop bit, no parity) with hardware flow control disabled.
3. **Terminal Initialization**: Enters terminal raw mode and switches to the alternate screen buffer using `crossterm` and `ratatui`.
4. **State Machine & UI Event Loop**: Launches the interactive dashboard titled **"Ben's Halftime Light Show Toolkit"**, listening for keyboard triggers to execute light routines, compute real-time pattern transitions, and dispatch wire frames.
5. **Clean Teardown**: Upon exit (`.` or `Ctrl+C`), terminal raw mode and screen buffers are automatically restored, and USB device streams are cleanly flushed and closed.

---

## Hardware Architecture & Wiring

The light show system relies on a wireless broadcast topology from a central host computer to wearable receivers distributed across performers on the field.

```
+-------------------------------------------------------------------------+
|                              Host Computer                              |
|   band-lighthshow (Tokio Event Loop + Ratatui Terminal UI Dashboard)   |
+------------------------------------+------------------------------------+
                                     | USB
                                     v
+-------------------------------------------------------------------------+
|                  FTDI FT232 USB-to-UART Serial Bridge                   |
|                   VID: 0x0403  |  PID: 0x6001                           |
|                   57600 Baud   |  8N1 (No Flow Control)                 |
+------------------------------------+------------------------------------+
                                     | UART (TX / RX / GND)
                                     v
+-------------------------------------------------------------------------+
|                    XBee Wireless Transmitter Module                     |
|                   2.4 GHz RF Serial Broadcast Link                      |
+------------------------------------+------------------------------------+
                                     |
               Wireless Broadcast RF (Point-to-Multipoint)
                                     |
        +----------------------------+----------------------------+
        |                                                         |
        v                                                         v
+-------------------------------+         +-------------------------------+
| Receiver Unit 1 (Performer)   |  . . .  | Receiver Unit N (Performer)   |
| MRF / Arduino Receiver Board  |         | MRF / Arduino Receiver Board  |
| 32-Channel RGB Glasses LEDs   |         | 32-Channel RGB Glasses LEDs   |
+-------------------------------+         +-------------------------------+
```

### Hardware Components & Specifications

* **Host Controller**: PC/Mac/Linux machine running the `band-lighthshow` binary.
* **USB-to-UART Adapter**:
  * **Chipset**: FTDI FT232 series (or compatible FTDI UART bridge).
  * **Vendor ID (VID)**: `0x0403` (`FTDI_VID`).
  * **Product ID (PID)**: `0x6001` (`pid::FT232`).
  * **Baud Rate**: `57600` baud.
  * **Line Properties**: 8 Data Bits, 1 Stop Bit, No Parity (`8N1`).
  * **Flow Control**: Disabled (`FlowControl::Disabled`).
* **Wireless Transmitter**: XBee 2.4 GHz RF module wired to the FTDI UART output for broadcast transmission.
* **Wireless Wearable Receivers**: 32-channel RGB receiver units (embedded MRF/Arduino hardware with software PWM driving wearable RGB LED glasses).

---

## Prerequisites & Dependencies

### System & Environment Prerequisites

* **Rust Toolchain**: Rust compiler supporting **Edition 2024** (Rust 1.85 or newer recommended) with `cargo`.
* **Hardware Connection**: An FTDI FT232 USB UART device connected to an available host USB port.
* **OS & USB Permissions**:
  * **Linux**: Userspace USB access via `nusb` requires read/write permissions on the FTDI USB device node. Create a `udev` rule (e.g., in `/etc/udev/rules.d/99-ftdi.rules` matching `ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666", GROUP="plugdev"`) or ensure the user is part of the `dialout`/`plugdev` group.
  * **macOS**: Supported natively through userspace USB; ensure no exclusive kernel driver conflicts block direct access via `nusb`.
  * **Windows**: WinUSB driver configuration via Zadig for the FTDI device if direct userspace USB claiming is required.

### Crate Dependencies (`Cargo.toml`)

The application is built on modern asynchronous and terminal libraries:

| Dependency | Version | Purpose & Classification |
| :--- | :--- | :--- |
| **`tokio`** | `1.53.1` | Asynchronous runtime (`rt`, `rt-multi-thread`, `macros`, `time`) managing event loops, non-blocking serial I/O, and microsecond sleep timers. |
| **`ratatui`** | `0.30.2` | Terminal UI framework rendering the 4-panel show dashboard, routine cards, and channel status monitors. |
| **`crossterm`** | `0.29.0` | Cross-platform terminal handling for raw mode, alternate screen buffer, and keyboard event polling. |
| **`ftdi-nusb`** | `0.3.0` | Pure-Rust async FTDI device driver (with `tokio` support) for device discovery, baud configuration, and raw packet transmission. |
| **`nusb`** | `0.2.7` | High-performance, cross-platform userspace USB access library backing `ftdi-nusb`. |
| **`anyhow`** | `1.0.104` | Flexible error handling and contextual error propagation across application entry points. |
| **`serde`** | `1.0.229` | Serialization/deserialization framework (`derive` feature) for data models and pattern structures. |
| **`ron`** | `0.12.2` | Rusty Object Notation deserializer/serializer for pattern and configuration schemas. |
