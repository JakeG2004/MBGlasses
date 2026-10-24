# Ben's Halftime Light Show Toolkit (`band-lighthshow`)

| Crate | Version | Edition | UI Dashboard |
| :--- | :--- | :--- | :--- |
| `band-lighthshow` | `0.1.0` | `2024` | Ratatui `0.30` / Crossterm `0.29` |

## Install prebuilt packages

You do **not** need Rust or build tools to install Glasses Broadcaster. Download an installer from [GitHub Releases](https://github.com/JakeG2004/MBGlasses/releases), not GitHub Packages or the source-code archives. Each complete release includes an operating-system/processor download table, release notes, and `SHA256SUMS` for checking downloads.

Choose the file for your exact operating-system version and processor:

| Operating system | Processor | Installer |
| :--- | :--- | :--- |
| Windows 10 / 11 | Intel / AMD 64-bit (`x86_64`) | `.msi` |
| macOS 11 or newer | Apple Silicon (`arm64`) | `.pkg`, unsigned by default |
| Debian 12 LTS / 13; Ubuntu 22.04 / 24.04 / 26.04 LTS | Intel / AMD 64-bit or ARM64 | `.deb` |
| Fedora 43 / 44; RHEL 8.10 / 9.8 / 10.2 | Intel / AMD 64-bit or ARM64 | `.rpm` |

Open the downloaded installer; normal administrator prompts are expected.

* **Windows**: Double-click the unsigned `.msi`, then open **Glasses Broadcaster** from the Start menu. The launcher keeps startup errors visible. Windows may show an **Unknown Publisher** or SmartScreen warning because the installer is unsigned. Only install a file downloaded directly from the Releases page above, and compare it with the matching entry in `SHA256SUMS`; do not disable Windows security protections. If unsure, stop and ask your system administrator.
* **macOS**: Open the `.pkg`, then open **Glasses Broadcaster.app** in **Applications**; it opens the dashboard in Terminal. The default installer is unsigned and may need extra approval: if blocked, use **System Settings → Privacy & Security → Open Anyway**, then retry. Older macOS versions call this **System Preferences → Security & Privacy**. Approve only downloads you trust; do not disable Gatekeeper.
* **Linux**: Open the `.deb` or `.rpm` with your desktop's native package installer, if available. Launch **Glasses Broadcaster** from the applications menu to open its terminal dashboard. After installation, unplug and reconnect the broadcaster while logged into an active desktop session so device access applies; run the dashboard as your normal user, not as root.

If your Linux desktop has no package installer, open a terminal in the download folder and use the matching command, replacing `FILE` with the downloaded filename:

```bash
# Debian / Ubuntu
sudo apt install ./FILE.deb
# Fedora / RHEL
sudo dnf install ./FILE.rpm
```

Installers do not flash broadcaster firmware or replace Windows USB drivers. The hardware preparation and one-time FTDI/WinUSB driver prerequisite below still apply. To update, download and install the newer matching release; there is no automatic updater or APT/DNF repository feed.

### Bundled data and startup checks

Installed packages find their bundled pattern catalog automatically; you do not need to run from the source directory or copy a `data` folder. `--data-dir PATH` is an optional explicit override; `./data` is only a development fallback. `--check-data` validates the catalog without opening USB devices or the dashboard, while `--list-devices` works independently of the catalog.

See the [packaging and release guide](packaging/README.md) for installed paths, maintainer instructions, and verification limits. No physical broadcaster hardware was used in package validation.

## Overview

**Ben's Halftime Light Show Toolkit** (`band-lighthshow`) is a modern Rust port and interactive terminal controller for synchronized marching band light shows. Originally created by Benjamin Jeffery (University of Idaho) in C/ncurses (`glasses.c`/`ht9.c`), this software controls wearable 32-channel RGB wireless glasses during live field performances.

Built with **Rust Edition 2024**, `band-lighthshow` provides an asynchronous, zero-latency control pipeline backed by **Tokio**, a live terminal dashboard powered by **Ratatui**, and direct userspace USB-serial communication via pure-Rust **FTDI** drivers.

### Operational Workflow & Startup Flow

1. **Broadcast-device enumeration**: On startup, the application looks for an FTDI FT232 (VID `0x0403`, PID `0x6001`) and for an ESP32-H2-MINI-1 on its native USB Serial/JTAG port (VID `0x303A`, PID `0x1001`). `--link auto` (the default) keeps the FTDI path when that dongle is present, and uses the ESP32-H2 when it is the only broadcast device.
2. **Device configuration**: An FTDI link is claimed and set to **57600 baud, 8N1**. An ESP32-H2 link is the OS CDC serial port for that USB identity; the module ignores the baud rate and reads framed color packets.
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
* **Wireless Transmitter**: Either an XBee 2.4 GHz RF module wired to the FTDI UART, or an ESP32-H2-MINI-1 connected by its own USB port and running `firmware/esp32h2-broadcaster`. The module transmits the same 96-byte RGB payload on 802.15.4 channel 12, PAN 2015, to the broadcast address.
* **Wireless Wearable Receivers**: 32-channel RGB receiver units (embedded MRF/Arduino hardware with software PWM driving wearable RGB LED glasses).

---

## Prerequisites & Dependencies

Rust and build dependencies are needed only for source development, not for the prebuilt installers above.

### System & Environment Prerequisites

* **Rust Toolchain**: Install Rust through `rustup`; `rust-toolchain.toml` selects the rolling **stable** channel with `clippy` and `rustfmt` for local development and CI.
* **Linux Build Dependencies**: On Ubuntu/Debian, install `pkg-config` and `libudev-dev` (`sudo apt-get install pkg-config libudev-dev`) for `serialport` device enumeration.
* **Hardware Connection**: An FTDI FT232 USB UART device connected to an available host USB port.
* **OS & USB Permissions**:
  * **Linux**: Packages install device-scoped `udev`/`uaccess` rules for FTDI USB `0403:6001` and ESP32-H2 tty `303a:1001`. An active desktop session and unplug/replug after installation are required. Source installations need equivalent device-scoped access; do not use world-writable rules or run the dashboard as root.
  * **macOS**: Supported natively through userspace USB; ensure no exclusive kernel driver conflicts block direct access via `nusb`.
  * **Windows**: One-time WinUSB driver configuration via Zadig for the FTDI device if direct userspace USB claiming is required. This remains a manual prerequisite; installers do not replace drivers.

### Crate Dependencies (`Cargo.toml`)

The application is built on modern asynchronous and terminal libraries:

| Dependency | Version | Purpose & Classification |
| :--- | :--- | :--- |
| **`tokio`** | `1.53.1` | Asynchronous runtime (`rt`, `rt-multi-thread`, `macros`, `time`) managing event loops, non-blocking serial I/O, and microsecond sleep timers. |
| **`ratatui`** | `0.30.2` | Terminal UI framework rendering the 4-panel show dashboard, routine cards, and channel status monitors. |
| **`crossterm`** | `0.29.0` | Cross-platform terminal handling for raw mode, alternate screen buffer, and keyboard event polling. |
| **`ftdi-nusb`** | `0.3.0` | Pure-Rust async FTDI device driver (with `tokio` support) for device discovery, baud configuration, and raw packet transmission. |
| **`nusb`** | `0.2.7` | High-performance, cross-platform userspace USB access library backing `ftdi-nusb`. |
| **`serialport`** | `4.10.1` | CDC-ACM access to the ESP32-H2-MINI-1 USB Serial/JTAG port. |
| **`anyhow`** | `1.0.104` | Flexible error handling and contextual error propagation across application entry points. |
| **`serde`** | `1.0.229` | Serialization/deserialization framework (`derive` feature) for data models and pattern structures. |
| **`ron`** | `0.12.2` | Rusty Object Notation deserializer/serializer for pattern and configuration schemas. |

---

## Continuous integration

The repository-root workflow [`.github/workflows/glasses-broadcaster.yml`](../.github/workflows/glasses-broadcaster.yml) runs on branch pushes and pull requests that change this project or its workflows, and can also be started manually. On Ubuntu 24.04 it checks formatting, rejects all Clippy warnings, runs the Rust and release-contract tests, and builds the release application. No broadcast hardware is needed for these checks.

Ordinary checks use the latest stable Rust toolchain; `rust-toolchain.toml` also selects `stable` for local development. The [tag-release workflow](../.github/workflows/glasses-broadcaster-release.yml) calls these checks with the compiler pinned in `packaging/targets.json`, without a duplicate branch-CI run for tags. Run `rustup update stable` to update your local compiler. Keep `Cargo.lock` committed; CI uses `--locked` to prevent dependency resolution from changing during a build.

Dependabot checks this project's Cargo dependencies and the repository's GitHub Actions weekly, configured in [`.github/dependabot.yml`](../.github/dependabot.yml). It opens pull requests to update dependencies, including `Cargo.toml` and `Cargo.lock` as needed. These pull requests run through the same CI checks; this configuration does not enable automatic merging.

Run the same checks from `glasses-broadcaster/`:

```bash
cargo fmt --all -- --check
cargo clippy --locked --all-targets --all-features -- -D warnings
cargo test --locked --all-features
cargo build --locked --release --all-features
python3 -m unittest discover -s packaging/release -p 'test_*.py' -v
```

---

## ESP32-H2-MINI-1 broadcaster

The FTDI + XBee path is unchanged. To broadcast from an ESP32-H2-MINI-1 instead, flash `../ESP32 H2 Firmware/esp32h2-broadcaster.ino`, then start the host with that link.

Arduino settings:

* Board: `ESP32H2 Dev Module`
* USB CDC On Boot: Enabled
* Zigbee Mode: Disabled
* Port: the module's own USB connector (USB Serial/JTAG, VID `0x303A` PID `0x1001`)

```bash
cargo run -- --check-data
cargo run -- --list-devices
cargo run -- --link esp32h2
cargo run -- --link ftdi
```

`--link auto` uses the FTDI dongle when it is connected, and the ESP32-H2 when that module is the only broadcast device. `--device N` indexes the list for the link that will actually be opened.

The host sends `A5 5A 60`, then 96 RGB bytes, then the XOR of those 96 bytes. The sketch transmits those 96 bytes as the 802.15.4 payload on channel 12, PAN 2015, destination `0xFFFF`, with no ACK request. Change `kChannel` and `kPanId` in the sketch if the existing XBee was configured differently. Close the Arduino serial monitor before starting the host, or the port will already be open.
