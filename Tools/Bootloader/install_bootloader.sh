avrdude -c avrisp -p atmega328p -P /dev/ttyUSB0 -b 19200 -U lfuse:w:0xE2:m -U hfuse:w:0xD8:m -U efuse:w:0xFF:m -U flash:w:bootloader.hex:i -U lock:w:0x0F:m
