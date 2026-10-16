# Configure Arduino IDE

These instructions flash the glasses receiver, `esp32h2-glasses.ino`, onto an
ESP32-H2-DevKitM-1. The receiver uses the onboard RGB LED on GPIO8 and the raw
802.15.4 radio; it does not use Zigbee or Thread.

## 1. Install the IDE and ESP32 board support

1. Install Arduino IDE 2.x from https://www.arduino.cc/en/software.
2. Open **Arduino IDE > Settings** on macOS, or **File > Preferences** on
   Windows/Linux. Add the following to **Additional boards manager URLs**, keeping
   any URLs already there:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. Open **Tools > Board > Boards Manager**, search for `esp32`, and install
   **esp32 by Espressif Systems**. Choose the latest stable **3.x** release;
   these sketches use the ESP32 Arduino core's low-level radio APIs, not core 2.x.
4. No extra Library Manager libraries are required. The board package supplies
   `esp_ieee802154.h` and `neopixelWrite()`.

## 2. Open the receiver as a separate sketch

Arduino requires a sketch folder with the same name as its main `.ino` file and
compiles all `.ino` files in that folder together.

1. Create an `esp32h2-glasses` folder in your Arduino sketchbook and copy only
   `esp32h2-glasses.ino` from this directory into it:

   ```text
   esp32h2-glasses/
     esp32h2-glasses.ino
   ```

2. In Arduino IDE, use **File > Open** to open the copied `.ino` file.

Do not copy `esp32h2-broadcaster.ino` into the receiver's sketch folder. It is a
different firmware with its own `setup()` and `loop()`; compiling both together
causes duplicate-definition errors.

## 3. Connect and select the board

1. Connect the board with a **data-capable** USB cable. Use the **ESP32-H2 USB**
   connector (native USB-Serial/JTAG), not the USB-to-UART connector if your board
   has both. The sketches use the native USB port for `Serial`.
2. Set these options in **Tools**:

   | Setting | Value |
   | --- | --- |
   | Board | **esp32 > ESP32H2 Dev Module** |
   | USB CDC On Boot | **Enabled** |
   | Zigbee Mode | **Disabled** |
   | Port | The ESP32-H2's USB serial port |

   The port is typically `/dev/cu.usbmodem...` on macOS, `/dev/ttyACM...` on Linux,
   or `COM...` on Windows. Leave the other board settings at their defaults.

## 4. Compile and upload

1. Click **Verify** (the checkmark) to compile the sketch.
2. For the first native-USB flash, enter download mode: hold **BOOT**, press and
   release **RESET** (sometimes labelled **EN**), then release **BOOT**. Select
   the newly appearing port under **Tools > Port** if necessary.
3. Close any other application using that serial port, then click **Upload**
   (the right arrow). Wait for the IDE to report that uploading is complete.
4. Press **RESET** without holding **BOOT** to start the firmware. If the USB
   port changes after reset, select it again. Later uploads normally enter
   download mode automatically; repeat the BOOT/RESET sequence if they do not.

## 5. Confirm the receiver is running

Open **Tools > Serial Monitor**, select **115200 baud**, and press **RESET** to
see the startup messages. With the unmodified sketch, expect:

```text
uiGC0 H2 radio ch 12 pan 2015 addr 0x4202 id 0 offset 0
listening
```

The onboard RGB LED flashes white briefly at startup, then stays off until a
valid matching show frame arrives. Periodic `listening` status lines continue
even without a transmitter; an `rx` count of zero is normal in that case.

## Troubleshooting

- **No port or upload cannot connect:** try a known data-capable cable, check
  that you used the native USB connector, and repeat the BOOT/RESET sequence
  before reselecting the port and uploading.
- **Serial Monitor is blank:** confirm **USB CDC On Boot > Enabled**, upload
  again if you changed it, select the native USB port, and reset the board.
  With CDC disabled, `Serial` goes to UART0 instead of this USB port.
- **Missing radio headers or API declarations:** check that the selected board
  is **ESP32H2 Dev Module** and update **esp32 by Espressif Systems** to the latest
  stable 3.x release. These are board-package APIs, not separate libraries.
- **`radio start failed` or a flashing red LED:** flashing completed, but radio
  initialization failed. Confirm **Zigbee Mode > Disabled**, then upload again.

## Flashing the USB broadcaster instead

Use the same board settings and upload procedure, but copy
`esp32h2-broadcaster.ino` into its own `esp32h2-broadcaster` sketch folder. This
replaces the receiver firmware on that board. The broadcaster does not print the
receiver startup messages above. Close Serial Monitor before connecting the
host broadcaster application, since both would otherwise compete for the port.

## Changing the radio channel

**The currently configured radio channel is `12` (2410 MHz), matching the
existing hardware.** Both `esp32h2-broadcaster.ino` and `esp32h2-glasses.ino`
use `static const uint8_t kChannel = 12;`. Keep channel `12` when communicating
with existing glasses that still listen on that channel.

The ESP32-H2 supports IEEE 802.15.4 radio channels `11–26` in the 2.4 GHz band;
these are not Wi-Fi channel numbers. A broadcaster and every receiver belonging
to its show must use the same radio channel. Changing only the broadcaster will
stop glasses still listening on the old channel from receiving it.

For two independent shows at the same location, for example:

| Show | Broadcaster radio channel | Glasses radio channel |
| --- | --- | --- |
| Existing hardware | `12` (unchanged) | `12` (unchanged) |
| New ESP32-H2 hardware | `25` | `25` |

To move the new ESP32-H2 show to channel `25`:

1. In both `esp32h2-broadcaster.ino` and `esp32h2-glasses.ino`, replace
   `static const uint8_t kChannel = 12;` with:

   ```cpp
   static const uint8_t kChannel = 25;
   ```

   Edit the copies in the separate Arduino sketch folders you will upload, or
   update the source files here and recopy each into its respective sketch
   folder. Leave `kPanId` and the other settings unchanged for a channel-only
   change. The receiver's color-channel/DIP-switch ID does not select the radio
   channel.
2. Compile and upload the broadcaster sketch to the new show's broadcaster,
   using the board settings and upload procedure above. Compile and upload the
   receiver sketch to **every ESP32-H2 receiver in that show**. Leave the existing
   show's hardware on channel `12`; uploading only the broadcaster is not enough.
3. Reset each updated receiver and check Serial Monitor at **115200 baud**. Its
   startup banner should now contain `radio ch 25` instead of `radio ch 12`.
   Close Serial Monitor before connecting the host broadcaster application.
4. Send a test color from each show's broadcaster and confirm that only its own
   glasses respond. Then run both shows together to check operation at the venue.

Channel changes require recompiling and reflashing; the host application's RGB
frames do not retune the radios. Separate channels reduce interference but do
not guarantee interference-free operation, so consider local Wi-Fi usage when
choosing channels.

## References

- [Espressif: installing Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Espressif: ESP32-H2 USB flashing and download mode](https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/get-started/establish-serial-connection.html)
- [ESP32-H2-DevKitM-1 user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32h2/esp32-h2-devkitm-1/user_guide.html)
