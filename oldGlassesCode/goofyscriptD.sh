# Simple shell script to set the lower fuse and flash the goofy controller
#
# To use, type ./goofyscript.sh file
#
#  Don't include the .c in the file name
#
sudo avrdude -v -y -c avrispmkII -p m328 -P usb:30:63 -U lfuse:w:0xE2:m
sudo avrdude -v -y -c avrispmkII -p m328 -P usb:30:63 -U flash:w:uiGCdemo.cpp.standard.hex:i


