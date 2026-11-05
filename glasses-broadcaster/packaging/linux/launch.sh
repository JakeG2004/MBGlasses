#!/bin/sh

/usr/bin/glasses-broadcaster "$@"
status=$?
if [ "$status" -ne 0 ]; then
    printf '\nGlasses Broadcaster could not start (exit %s).\nPress Enter to close this window.\n' "$status"
    read -r answer || true
fi
exit "$status"