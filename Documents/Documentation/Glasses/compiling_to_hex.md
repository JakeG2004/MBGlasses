# Compiling to Hex

## From CLI (Recommended)

### Prerequisites
1) Install arduino-cli

2) Ensure arduino-cli is configured
    ```
    arduino-cli config init
    arduino-cli core update-index
    arduino-cli core install arduino:avr
    ```

### Steps
1) Compile your code
    ```
    arduino-cli compile --board arduino:avr:uno /path/to/sketchbook
    ```
    for example, `arduino-cli compile -b arduino:avr:uno ~/Arduino/Glasses/ --output-dir <target path>`
2) The .hex file should now exist in your target path

## From Arduino IDE

### Prerequisites
1) Ensure that verbose compilation output is enabled under File -> Preferences -> Toggle Show verbose output during compile tto true

### Steps
1) Write your source code
2) Hit sketch -> export compiled binary
3) In the Output at the bottom of the IDE, look at the last line before `Sketch uses ...`. The last argument in that line should be the where your binary is. In this example, `/home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.elf` is the location of the binary,
```cpp
Detecting libraries used...
Using cached library dependencies for file: /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/sketch/Glasses.ino.cpp.merged
Generating function prototypes...
Using cached sketch with function prototypes.
Compiling sketch...
Using previously compiled file: /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/sketch/Glasses.ino.cpp.o
Compiling libraries...
Compiling core...
Using precompiled core: /home/jake/.cache/arduino/cores/arduino_avr_crinket_dcc964143f9c952dcbd38777ca504b10/core.a
Linking everything together...
/home/jake/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-gcc -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega328p -o /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.elf /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/sketch/Glasses.ino.cpp.o /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/../../cores/arduino_avr_crinket_dcc964143f9c952dcbd38777ca504b10/core.a -L/home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5 -lm
/home/jake/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-objcopy -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.elf /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.eep
/home/jake/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-objcopy -O ihex -R .eeprom /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.elf /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.hex
/home/jake/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-size -A /home/jake/.cache/arduino/sketches/42B491BE96E2B57806FFBBEDFC027BA5/Glasses.ino.elf
Sketch uses 1626 bytes (5%) of program storage space. Maximum is 32256 bytes.
Global variables use 192 bytes (9%) of dynamic memory, leaving 1856 bytes for local variables. Maximum is 2048 bytes.
```
4) Copy the binary from that path to your target path.
5) Convert it to hex using `avr-objcopy -O ihex -R .eeprom input_file.elf output_file.hex`