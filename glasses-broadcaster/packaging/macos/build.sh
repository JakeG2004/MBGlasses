#!/bin/bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'Usage: %s VERSION OUTPUT_DIRECTORY\nSet MACOS_SIGN=1 to require Developer ID signing and notarization.\n' "$0" >&2
    exit 2
fi
version=$1
output=$2
sign=${MACOS_SIGN:-0}
if [[ ! $version =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ || ! $sign =~ ^[01]$ ]]; then
    printf 'A stable X.Y.Z version and MACOS_SIGN=0 or 1 are required.\n' >&2
    exit 2
fi
if [[ ! ${MACOS_VERIFY_INSTALL:-0} =~ ^[01]$ || ( ${GITHUB_ACTIONS:-false} == true && ${MACOS_VERIFY_INSTALL:-0} != 1 ) ]]; then
    printf 'MACOS_VERIFY_INSTALL must be 0 or 1; release CI requires 1.\n' >&2
    exit 2
fi
if [[ $(uname -m) != arm64 ]]; then
    printf 'Build on an Apple Silicon macOS host.\n' >&2
    exit 2
fi
if [[ $sign == 1 ]]; then
    : "${MACOS_APPLICATION_P12_BASE64:?Required when MACOS_SIGN=1}"
    : "${MACOS_APPLICATION_P12_PASSWORD:?Required when MACOS_SIGN=1}"
    : "${MACOS_INSTALLER_P12_BASE64:?Required when MACOS_SIGN=1}"
    : "${MACOS_INSTALLER_P12_PASSWORD:?Required when MACOS_SIGN=1}"
    : "${MACOS_APPLICATION_IDENTITY:?Required when MACOS_SIGN=1}"
    : "${MACOS_INSTALLER_IDENTITY:?Required when MACOS_SIGN=1}"
    : "${APPLE_API_KEY_BASE64:?Required when MACOS_SIGN=1}"
    : "${APPLE_API_KEY_ID:?Required when MACOS_SIGN=1}"
    : "${APPLE_API_ISSUER:?Required when MACOS_SIGN=1}"
    case "$MACOS_APPLICATION_IDENTITY" in
        'Developer ID Application: '*) ;;
        *) printf 'A Developer ID Application identity is required.\n' >&2; exit 2 ;;
    esac
    case "$MACOS_INSTALLER_IDENTITY" in
        'Developer ID Installer: '*) ;;
        *) printf 'A Developer ID Installer identity is required.\n' >&2; exit 2 ;;
    esac
fi

script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
cd "$root"
crate_version=$(cargo metadata --locked --no-deps --format-version 1 | python3 -c 'import json,sys; print(next(p["version"] for p in json.load(sys.stdin)["packages"] if p["name"] == "glasses-broadcaster"))')
if [[ $crate_version != "$version" ]]; then
    printf 'Installer version must exactly match Cargo.toml (%s).\n' "$crate_version" >&2
    exit 2
fi
min_os=${MACOS_MIN_VERSION:-11.0}
if [[ ! $min_os =~ ^[0-9]+\.[0-9]+$ || ${min_os%%.*} -lt 11 ]]; then
    printf 'The Apple Silicon deployment minimum must be macOS 11.0 or newer.\n' >&2
    exit 2
fi
umask 022
mkdir -p "$output"
output=$(CDPATH= cd -- "$output" && pwd)
work=$(mktemp -d "$output/.macos-build.XXXXXX")
keychain=''
installed_by_us=0
identifier=io.github.jakeg2004.mbglasses.broadcaster
installed_app='/Applications/Glasses Broadcaster.app'
cleanup() {
    status=$?
    trap - EXIT
    if [[ $installed_by_us == 1 ]]; then
        sudo -n rm -rf -- "$installed_app" || status=1
        sudo -n pkgutil --forget "$identifier" >/dev/null || status=1
    fi
    if [[ -n $keychain && -f $keychain ]]; then
        security delete-keychain "$keychain" >/dev/null || status=1
    fi
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT

export MACOSX_DEPLOYMENT_TARGET=$min_os
export RUSTFLAGS='' CARGO_ENCODED_RUSTFLAGS=''
target_dir=${CARGO_TARGET_DIR:-"$root/target"}
mkdir -p "$target_dir"
target_dir=$(CDPATH= cd -- "$target_dir" && pwd)
export CARGO_TARGET_DIR=$target_dir
cargo test --locked --all-features --target aarch64-apple-darwin
cargo build --locked --release --all-features --target aarch64-apple-darwin

payload="$work/payload"
app="$payload/Applications/Glasses Broadcaster.app"
install -d -m755 "$app/Contents/MacOS" "$app/Contents/Resources"
install -m755 "$target_dir/aarch64-apple-darwin/release/glasses-broadcaster" "$app/Contents/MacOS/glasses-broadcaster"
install -m644 "$script_dir/Info.plist" "$app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $version" "$app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $version" "$app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $min_os" "$app/Contents/Info.plist"
ditto --noqtn --norsrc "$root/data" "$app/Contents/Resources/data"
find "$app/Contents/Resources/data" -type d -exec chmod 755 {} +
find "$app/Contents/Resources/data" -type f -exec chmod 644 {} +
install -m755 "$script_dir/launch.command" "$app/Contents/Resources/launch.command"
xcrun swiftc -swift-version 5 -parse-as-library -target "arm64-apple-macosx$min_os" "$script_dir/Launcher.swift" -o "$app/Contents/MacOS/Launcher"
plutil -lint "$app/Contents/Info.plist"
for binary in "$app/Contents/MacOS/glasses-broadcaster" "$app/Contents/MacOS/Launcher"; do
    [[ $(lipo -archs "$binary") == arm64 ]]
    [[ $(xcrun vtool -show-build "$binary" | awk '$1 == "minos" { print $2 }') == "$min_os" ]]
    dependencies=$(otool -L "$binary" | awk 'NR > 1 { print $1 }')
    while IFS= read -r dependency; do
        case "$dependency" in
            /usr/lib/*|/System/Library/*) ;;
            *) printf 'Unbundled dependency: %s\n' "$dependency" >&2; exit 1 ;;
        esac
    done <<< "$dependencies"
done

signing_label=unsigned
if [[ $sign == 1 ]]; then
    keychain="$work/signing.keychain-db"
    keychain_password=$(openssl rand -hex 24)
    security create-keychain -p "$keychain_password" "$keychain"
    security set-keychain-settings -lut 21600 "$keychain"
    security unlock-keychain -p "$keychain_password" "$keychain"
    printf '%s' "$MACOS_APPLICATION_P12_BASE64" | base64 --decode > "$work/application.p12"
    printf '%s' "$MACOS_INSTALLER_P12_BASE64" | base64 --decode > "$work/installer.p12"
    printf '%s' "$APPLE_API_KEY_BASE64" | base64 --decode > "$work/notary-key.p8"
    chmod 600 "$work/application.p12" "$work/installer.p12" "$work/notary-key.p8"
    security import "$work/application.p12" -k "$keychain" -P "$MACOS_APPLICATION_P12_PASSWORD" -T /usr/bin/codesign
    security import "$work/installer.p12" -k "$keychain" -P "$MACOS_INSTALLER_P12_PASSWORD" -T /usr/bin/productbuild -T /usr/bin/productsign
    security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain" >/dev/null
    for binary in "$app/Contents/MacOS/glasses-broadcaster" "$app/Contents/MacOS/Launcher"; do
        codesign --force --keychain "$keychain" --sign "$MACOS_APPLICATION_IDENTITY" --options runtime --timestamp "$binary"
    done
    codesign --force --keychain "$keychain" --sign "$MACOS_APPLICATION_IDENTITY" --options runtime --timestamp "$app"
    signing_label=signed
else
    # Apple Silicon executables require ad-hoc code signatures, not Apple credentials.
    codesign --force --sign - "$app/Contents/MacOS/glasses-broadcaster"
    codesign --force --sign - "$app/Contents/MacOS/Launcher"
    codesign --force --sign - "$app"
fi
codesign --verify --deep --strict --verbose=2 "$app"

pkgbuild --analyze --root "$payload" "$work/components.plist"
/usr/libexec/PlistBuddy -c 'Set :0:BundleIsRelocatable false' "$work/components.plist"
pkgbuild --root "$payload" --component-plist "$work/components.plist" --ownership recommended --identifier "$identifier" --version "$version" --install-location / "$work/glasses-broadcaster-component.pkg"
/usr/libexec/PlistBuddy -c 'Add :arch array' -c 'Add :arch:0 string arm64' -c 'Add :os array' -c "Add :os:0 string $min_os" "$work/product.plist"
productbuild --synthesize --product "$work/product.plist" --package "$work/glasses-broadcaster-component.pkg" "$work/Distribution"
python3 - "$work/Distribution" <<'PY'
import sys
import xml.etree.ElementTree as ET

tree = ET.parse(sys.argv[1])
root = tree.getroot()
title = root.find("title")
if title is None:
    title = ET.SubElement(root, "title")
title.text = "Glasses Broadcaster"
tree.write(sys.argv[1], encoding="utf-8", xml_declaration=True)
PY
package="$work/glasses-broadcaster.pkg"
if [[ $sign == 1 ]]; then
    productbuild --distribution "$work/Distribution" --package-path "$work" --keychain "$keychain" --sign "$MACOS_INSTALLER_IDENTITY" --timestamp "$package"
    xcrun notarytool submit "$package" --key "$work/notary-key.p8" --key-id "$APPLE_API_KEY_ID" --issuer "$APPLE_API_ISSUER" --wait --timeout 30m --output-format json > "$work/notarization.json"
    python3 -c 'import json,sys; result=json.load(open(sys.argv[1])); sys.exit(0 if result.get("status") == "Accepted" else f"Notarization rejected: {result}")' "$work/notarization.json"
    xcrun stapler staple "$package"
    xcrun stapler validate "$package"
    signature=$(pkgutil --check-signature "$package")
    case "$signature" in *'Developer ID Installer:'*) ;; *) printf 'Installer trust verification failed.\n' >&2; exit 1 ;; esac
    case "$signature" in *'Signed with a trusted timestamp'*) ;; *) printf 'Installer timestamp verification failed.\n' >&2; exit 1 ;; esac
    spctl --assess --type install --verbose=2 "$package"
else
    productbuild --distribution "$work/Distribution" --package-path "$work" "$package"
    signature_status=0
    signature=$(pkgutil --check-signature "$package" 2>&1) || signature_status=$?
    [[ $signature_status -le 1 ]]
    case "$signature" in *'Status: no signature'*) ;; *) printf '%s\n' "$signature" >&2; exit 1 ;; esac
fi

pkgutil --expand-full "$package" "$work/expanded"
expanded_app="$work/expanded/glasses-broadcaster-component.pkg/Payload/Applications/Glasses Broadcaster.app"
codesign --verify --deep --strict --verbose=2 "$expanded_app"
diff -r "$root/data" "$expanded_app/Contents/Resources/data"
(
    cd /
    "$expanded_app/Contents/MacOS/glasses-broadcaster" --help >/dev/null
    "$expanded_app/Contents/MacOS/glasses-broadcaster" --version
    "$expanded_app/Contents/MacOS/glasses-broadcaster" --check-data
    "$expanded_app/Contents/MacOS/Launcher" --check-bundle
    "$expanded_app/Contents/Resources/launch.command" --check-data
)
if [[ ${MACOS_VERIFY_INSTALL:-0} == 1 ]]; then
    if [[ -e $installed_app ]]; then
        printf 'Refusing to overwrite an existing application during installation tests.\n' >&2
        exit 1
    fi
    installed_by_us=1
    sudo -n installer -pkg "$package" -target /
    (cd /; "$installed_app/Contents/MacOS/glasses-broadcaster" --check-data)
    "$installed_app/Contents/MacOS/Launcher" --check-bundle
    codesign --verify --deep --strict --verbose=2 "$installed_app"
    sudo -n rm -rf -- "$installed_app"
    sudo -n pkgutil --forget "$identifier" >/dev/null
    [[ ! -e $installed_app ]]
    installed_by_us=0
fi
filename="glasses-broadcaster-$version-macos-$min_os-arm64-$signing_label.pkg"
if [[ -e $output/$filename ]]; then
    printf 'Refusing to replace an existing installer: %s\n' "$output/$filename" >&2
    exit 1
fi
mv "$package" "$output/$filename"
python3 - "$output/$filename" "$version" "$sign" "${MACOS_VERIFY_INSTALL:-0}" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

package = Path(sys.argv[1])
signed = sys.argv[3] == "1"
installed = sys.argv[4] == "1"
proof = dict(version=sys.argv[2], architecture="arm64", signed=signed, timestamped=signed,
             notarized=signed, installed=installed, uninstalled=installed,
             sha256=hashlib.sha256(package.read_bytes()).hexdigest())
(package.parent / "macos-verification.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
PY
printf 'Verified installer: %s\n' "$output/$filename"