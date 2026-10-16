# Simple shell script to set the lower fuse and flash the goofy controller
#
# To use, type ./goofyscript.sh file
#
#  Don't include the .c in the file name
#
sudo avrdude -v -y -c usbtiny   -p m328 -U lfuse:w:0xE2:m
sudo avrdude -v -y -c usbtiny   -p m328 -U flash:w:uiGCdemo.cpp.standard.hex:i


