#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
build_dir=$(mktemp -d ./receiver-build.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
status=0
for receiver in uiGC0 uiGCnew; do
    printf '\nTesting %s\n' "$receiver"
    "${CXX:-c++}" -std=c++14 -O1 -g -Wall -Wextra \
        -fsanitize="${SANITIZERS:-undefined}" -fno-sanitize-recover=all -fno-omit-frame-pointer \
        -DARDUINO=100 -I stubs \
        -DDRIVER_SOURCE="\"../$receiver/mrf24j.cpp\"" \
        -DSKETCH_SOURCE="\"../$receiver/$receiver.ino\"" \
        -DRECEIVER_NEW="$([[ "$receiver" == uiGCnew ]] && printf 1 || printf 0)" \
        receiver_test.cpp -o "$build_dir/$receiver"
    "$build_dir/$receiver" || status=1
done
exit "$status"