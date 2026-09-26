#!/bin/bash

# Check if a file argument was provided
if [ -z "$1" ]; then
  echo "Warning: No hex file supplied."
  echo "Usage: $0 <path_to_hex_file>"
  exit 1
fi

HEX_FILE="$1"

# Execute avrdude command
avrdude -v -c stk500v1 -p atmega328p -P /dev/ttyUSB0 -b 38400 -U flash:w:"$HEX_FILE":i
