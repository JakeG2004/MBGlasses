import subprocess
import sys
import time

PORT = "/dev/ttyUSB3"
HEXFILE = "uiGC0.ino.hex"

def port_is_available() -> bool:
    result = subprocess.run(['ls', PORT], capture_output=True, text=True)
    if(result.returncode == 0):
        return True

    return False


def main():
    cmd = [
        "avrdude",
        "-D",
        "-v",
        "-c", "arduino",
        "-p", "m328p",
        "-P", "/dev/ttyUSB3",
        "-b", "115200",
        "-U", f"flash:w:{HEXFILE}:i",
    ]

    has_written = False;
    while(not has_written):
        print(f"Waiting for connection on {PORT}")
        if(port_is_available()):
            print("Connected!")
            has_written = True
            subprocess.run(cmd)
            print("Subprocess complete")

if __name__ == "__main__":
    sys.exit(main())
