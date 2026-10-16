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

## References

- [Espressif: installing Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Espressif: ESP32-H2 USB flashing and download mode](https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/get-started/establish-serial-connection.html)
- [ESP32-H2-DevKitM-1 user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32h2/esp32-h2-devkitm-1/user_guide.html)
