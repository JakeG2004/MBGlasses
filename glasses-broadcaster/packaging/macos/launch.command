#!/bin/bash

contents=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd) || exit 1
"$contents/MacOS/glasses-broadcaster" "$@"
status=$?
if [ "$status" -ne 0 ]; then
    printf '\nGlasses Broadcaster could not start (exit %s).\nPress Return to close this window.\n' "$status"
    read -r answer || true
fi
exit "$status"