# Receiver regression tests

Run from the repository root:

```sh
bash oldGlassesCode/tests/run.sh
```

Requires a C++14 compiler with UndefinedBehaviorSanitizer (set `CXX` to override
`c++`). To also use AddressSanitizer, run with `SANITIZERS=address,undefined`.
The local Apple clang 17 AddressSanitizer runtime stalled during initialization,
before tests ran; UndefinedBehaviorSanitizer is the default for this harness.
Temporary executables are created under this directory and removed on exit.
Each receiver's actual driver and sketch are
compiled separately; Arduino, SPI, and PWM hardware are mocked. SPI commands
are decoded into register accesses, not replaced with a fake receive routine.

The load test compares the same 96-byte payload with optional raw buffering
on/off: 206 versus 99 FIFO byte reads. This demonstrates reduced receive work,
not elimination of LED flicker. Host tests do not model radio timing, electrical
behavior, or the actual SoftPWM timer, and do not flash any hardware.

## Coverage

- First reception, alternating payload sizes, FIFO lengths 0/10/11/127/128/255,
  exact payload/metadata, bounded register accesses, and optional raw buffering.
- Notification ordering, preservation of prior data/pending notifications on
  rejection, recovery, and simultaneous RX/TX completion.
- Enabled/disabled interrupt entry states and pending RX notification saturation.
- RGB lengths immediately below/at each receiver's selected triplet, IDs 0–15,
  repeated pink and zero-blue colors, and retention of the last valid color.
- A simulated pending radio ISR at snapshot exit or between PWM setters; no SPI
  accesses or PWM setters inside the sketch snapshot section.
- The unchanged `uiGCnew` ID-15 demo colors and delay sequence.

## Validation status

The initial unsanitized `uiGC0` harness run reproduced the raw-buffering setup
failure and confirmed the 206-to-99 FIFO-read comparison. At the user's request,
no further host tests were executed. The completed regression suite and firmware
changes have been inspected but have **not passed a post-fix test run**.

An AVR firmware build with the bundled `SoftPWM` remains blocked: no
`arduino-cli`, `avr-g++`, or `avr-gcc` was found on PATH, and no reproducible
receiver board/clock configuration was found. The flashing scripts' `m328`
argument is only a target clue, not a confirmed Arduino board configuration.
The IDE build request returned limited-diagnostics information, not evidence of
a successful AVR compile. No prebuilt firmware or hardware was changed.