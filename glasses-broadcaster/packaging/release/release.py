#!/usr/bin/env python3
"""Native package jobs and the fail-closed GitHub Release contract (Python 3.11+)."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
from urllib import error, parse, request
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "packaging/targets.json"
NAME = "glasses-broadcaster"


class ReleaseError(ValueError):
    pass


def run(arguments, **kwargs):
    return subprocess.run([str(a) for a in arguments], check=True, **kwargs)


def output(arguments, **kwargs):
    return run(arguments, text=True, stdout=subprocess.PIPE, **kwargs).stdout.strip()


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def validate_tag(tag, crate_version):
    match = re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", tag)
    if not match or len(tag) > 40:
        raise ReleaseError("Use a stable vMAJOR.MINOR.PATCH tag without leading zeros.")
    version = tag[1:]
    if version != crate_version:
        raise ReleaseError(f"Tag {tag} must exactly match Cargo.toml version {crate_version}.")
    if any(int(part) > limit for part, limit in zip(match.groups(), (255, 255, 65535))):
        raise ReleaseError("MSI version limits are 255.255.65535.")
    return version


def load_config(path=CONFIG):
    config = json.loads(Path(path).read_text(encoding="utf-8"))
    if config.get("schema_version") != 1:
        raise ReleaseError("Unsupported target matrix schema.")
    targets(config)
    return config


def targets(config):
    result = []
    architectures = set(config["linux_architectures"])
    if architectures != {"x86_64", "arm64"}:
        raise ReleaseError("Linux requires x86-64 and ARM64 architecture mappings.")
    for row in config["linux"]:
        if row["format"] not in ("deb", "rpm"):
            raise ReleaseError("Unsupported Linux package format.")
        if set(row["images"]) != architectures:
            raise ReleaseError("Every Linux distribution requires both processor images.")
        for architecture, digest in row["images"].items():
            if architecture not in config["linux_architectures"] or not re.fullmatch(r"sha256:[a-f0-9]{64}", digest):
                raise ReleaseError("Every Linux architecture needs a pinned image digest.")
            target = {k: v for k, v in row.items() if k not in ("images", "repository")}
            target.update(config["linux_architectures"][architecture])
            target.update(id=f"{row['id']}-{architecture}", os="linux", architecture=architecture,
                          image=f"{row['repository']}@{digest}")
            target["package_arch"] = target[f"{row['format']}_arch"]
            result.append(target)
    result.extend(dict(config[name]) for name in ("macos", "windows"))
    ids = [target["id"] for target in result]
    if len(ids) != len(set(ids)) or any(not re.fullmatch(r"[a-z0-9][a-z0-9._-]+", key) for key in ids):
        raise ReleaseError("Duplicate or unsafe target identifiers.")
    if len(result) != config.get("expected_packages"):
        raise ReleaseError("The matrix does not match its declared package inventory.")
    if config["windows"]["architecture"] != "x86_64" or config["macos"]["architecture"] != "arm64":
        raise ReleaseError("Only x86-64 Windows and Apple Silicon macOS are supported.")
    return result


def target_for(config, identifier):
    for target in targets(config):
        if target["id"] == identifier:
            return target
    raise ReleaseError(f"Unknown target: {identifier}")


def artifact_name(target, version, macos_sign=False):
    suffix = f"-{'signed' if macos_sign else 'unsigned'}" if target["os"] == "macos" else ""
    return f"{NAME}-{version}-{target['id']}{suffix}.{target['format']}"


def required_checks(target, macos_sign=False):
    checks = {"build", "tests", "metadata", "install", "uninstall", "catalog", "data", "native"}
    if target["os"] == "linux":
        checks.update(("desktop", "udev"))
    elif target["os"] == "windows":
        checks.update(("shortcut", "static_runtime"))
    else:
        checks.add("bundle")
        if macos_sign:
            checks.update(("signature", "timestamp", "notarization"))
    return checks


def source_commit():
    commit = output(("git", "rev-parse", "HEAD"), cwd=ROOT)
    if not re.fullmatch(r"[a-f0-9]{40}", commit) or os.environ.get("GITHUB_SHA", commit) != commit:
        raise ReleaseError("Checkout does not match the triggering commit.")
    return commit


def native_architecture(machine=None):
    machine = (machine or platform.machine()).lower()
    return {"amd64": "x86_64", "x86_64": "x86_64", "arm64": "arm64", "aarch64": "arm64"}.get(machine)


def write_receipt(target, version, package, destination, metadata, checks, *, signed=False, native=True):
    receipt = dict(schema_version=1, target=target["id"], version=version, filename=package.name,
                   source_commit=source_commit(), sha256=sha256(package), size=package.stat().st_size,
                   metadata=metadata, checks=sorted(checks), signed=signed, native=native)
    Path(destination).write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return receipt


def collect_artifacts(directory, config, version, commit, macos_sign=False):
    if not re.fullmatch(r"[a-f0-9]{40}", commit):
        raise ReleaseError("Verification receipts require a full hexadecimal source commit.")
    expected = {artifact_name(t, version, macos_sign): t for t in targets(config)}
    found = {}
    for path in Path(directory).rglob("*"):
        if path.is_symlink():
            raise ReleaseError(f"Symlink in artifact bundle: {path}")
        if path.is_file():
            if path.name in found:
                raise ReleaseError(f"Duplicate artifact: {path.name}")
            found[path.name] = path
    names = set(expected) | {name + ".json" for name in expected}
    if set(found) != names:
        raise ReleaseError(f"Artifact set mismatch; missing={sorted(names - found.keys())}, unexpected={sorted(found.keys() - names)}")
    packages = []
    for name, target in expected.items():
        path = found[name]
        receipt = json.loads(found[name + ".json"].read_text(encoding="utf-8"))
        required = required_checks(target, macos_sign)
        metadata = receipt.get("metadata", {})
        package_arch = target.get("package_arch", target["architecture"])
        signed = target["os"] == "macos" and macos_sign
        if (receipt.get("schema_version") != 1 or receipt.get("target") != target["id"]
                or receipt.get("version") != version or receipt.get("source_commit") != commit
                or receipt.get("filename") != name or receipt.get("native") is not True
                or receipt.get("signed") is not signed or not required.issubset(receipt.get("checks", []))
                or metadata.get("version") != version or metadata.get("architecture") != package_arch):
            raise ReleaseError(f"Invalid or incomplete native verification receipt: {name}")
        if receipt.get("sha256") != sha256(path) or receipt.get("size") != path.stat().st_size:
            raise ReleaseError(f"Checksum/size mismatch after native verification: {name}")
        packages.append((path, receipt))
    return packages


def download_table(config, version, repository, macos_sign=False):
    lines = ["### Downloads", "", "Choose your operating system version and processor; no Rust or build tools are needed.", "",
             "| System | Version | Processor | Download |", "|---|---|---|---|"]
    for target in targets(config):
        name = artifact_name(target, version, macos_sign)
        label = f"{target['distribution']}"
        if target["os"] == "macos":
            label += " (signed/notarized)" if macos_sign else " (unsigned; extra macOS approval may be needed)"
        url = f"https://github.com/{repository}/releases/download/v{version}/{name}"
        lines.append(f"| {label} | {target['version']} | {target['processor']} | [{name}]({url}) |")
    lines.extend(("", "### Installation", "",
                  "- Linux: open the matching package with your system's software installer. If needed, use `sudo apt install ./FILE.deb` or `sudo dnf install ./FILE.rpm`. Open **Glasses Broadcaster** from the application menu, then reconnect the USB broadcaster.",
                  "- Mac: open the Apple Silicon `.pkg`, then open **Glasses Broadcaster** in Applications. Unsigned packages may require approval in Privacy & Security → Open Anyway; do not disable system security globally.",
                  "- Windows 10/11 (Intel/AMD 64-bit): open the unsigned `.msi`, then use the Start-menu shortcut. Windows may show Unknown Publisher or SmartScreen warnings; download only from this GitHub Releases page and check `SHA256SUMS` before opening it. The FTDI/WinUSB driver setup remains a one-time hardware prerequisite; this installer never replaces drivers or flashes firmware.",
                  "- Administrator approval is normal. These launch the existing terminal dashboard, not a new graphical interface.",
                  "", "`SHA256SUMS` contains checksums of the final release files; the macOS installer may be signed/notarized when enabled. Linux compatibility checks do not certify physical USB/radio operation or Red Hat certification.", ""))
    return "\n".join(lines)


class SafeRedirect(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if redirected and parse.urlsplit(req.full_url).netloc != parse.urlsplit(newurl).netloc:
            redirected.remove_header("Authorization")
        return redirected


class GitHub:
    def __init__(self, repository, token):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository) or not token:
            raise ReleaseError("A repository and publishing token are required.")
        self.base = f"https://api.github.com/repos/{repository}"
        self.token = token
        self.opener = request.build_opener(SafeRedirect())

    def call(self, method, path, data=None, *, binary=False):
        url = path if path.startswith("https://") else self.base + path
        headers = {"Authorization": f"Bearer {self.token}", "X-GitHub-Api-Version": "2022-11-28",
                   "Accept": "application/octet-stream" if binary else "application/vnd.github+json"}
        if isinstance(data, Path):
            payload = data.read_bytes()
            headers["Content-Type"] = "application/octet-stream"
        else:
            payload = None if data is None else json.dumps(data).encode()
            headers["Content-Type"] = "application/json"
        try:
            with self.opener.open(request.Request(url, payload, headers, method=method), timeout=120) as response:
                body = response.read()
                return body if binary else (json.loads(body) if body else None)
        except error.HTTPError as exc:
            if method == "GET" and exc.code == 404:
                return None
            raise ReleaseError(f"GitHub {method} failed with HTTP {exc.code} at {parse.urlsplit(url).path}") from exc


def remote_tag_commit(api, tag):
    ref = api.call("GET", "/git/ref/tags/" + parse.quote(tag, safe=""))
    if not ref:
        raise ReleaseError("The release tag no longer exists.")
    obj = ref["object"]
    for _ in range(8):
        if obj["type"] == "commit":
            return obj["sha"]
        if obj["type"] != "tag":
            break
        obj = api.call("GET", "/git/tags/" + obj["sha"])["object"]
    raise ReleaseError("Could not resolve the release tag to a commit.")


def publish(api, tag, commit, files, body):
    if remote_tag_commit(api, tag) != commit:
        raise ReleaseError("The remote tag changed after preflight.")
    release = api.call("GET", "/releases/tags/" + tag)
    if release and not release["draft"]:
        raise ReleaseError("This release is already published; use a new version, never overwrite it.")
    if release and release["target_commitish"] != commit:
        raise ReleaseError("Existing draft belongs to a different commit.")
    notes = api.call("POST", "/releases/generate-notes", {"tag_name": tag, "target_commitish": commit})
    text = body + "\n### Release notes\n\n" + notes["body"]
    if not release:
        release = api.call("POST", "/releases", {"tag_name": tag, "target_commitish": commit,
                                                "name": f"Glasses Broadcaster {tag}", "draft": True, "prerelease": False, "body": text})
    assets = api.call("GET", f"/releases/{release['id']}/assets?per_page=100")
    by_name = {asset["name"]: asset for asset in assets}
    expected = {path.name for path in files}
    if len(by_name) != len(assets) or set(by_name) - expected:
        raise ReleaseError("Draft contains duplicate or unexpected assets; review it manually before retrying.")
    for path in files:
        digest = "sha256:" + sha256(path)
        asset = by_name.get(path.name)
        if asset and (asset.get("digest") != digest or asset["size"] != path.stat().st_size):
            api.call("DELETE", f"/releases/assets/{asset['id']}")
            asset = None
        if not asset:
            upload = release["upload_url"].split("{")[0] + "?name=" + parse.quote(path.name, safe="")
            asset = api.call("POST", upload, path)
        if asset["size"] != path.stat().st_size:
            raise ReleaseError(f"Uploaded size mismatch: {path.name}")
        remote = api.call("GET", f"/releases/assets/{asset['id']}", binary=True)
        if remote is None or hashlib.sha256(remote).hexdigest() != digest.removeprefix("sha256:"):
            raise ReleaseError(f"Uploaded checksum mismatch: {path.name}")
    final = api.call("GET", f"/releases/{release['id']}/assets?per_page=100")
    if {asset["name"] for asset in final} != expected or len(final) != len(expected):
        raise ReleaseError("Draft is incomplete; refusing publication.")
    if remote_tag_commit(api, tag) != commit:
        raise ReleaseError("The tag changed during draft uploads.")
    api.call("PATCH", f"/releases/{release['id']}", {"body": text, "draft": False, "make_latest": "legacy"})


def previous_msi(api, config, version, destination):
    candidates = []
    page = 1
    while True:
        releases = api.call("GET", f"/releases?per_page=100&page={page}")
        for release in releases:
            tag = release["tag_name"]
            if not release["draft"] and not release["prerelease"] and re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", tag):
                key = tuple(map(int, tag[1:].split(".")))
                if key < tuple(map(int, version.split("."))):
                    candidates.append((key, release))
        if len(releases) < 100:
            break
        page += 1
    for _, release in sorted(candidates, key=lambda item: item[0], reverse=True):
        expected = artifact_name(config["windows"], release["tag_name"][1:])
        matching = [asset for asset in release["assets"] if asset["name"] == expected]
        if len(matching) > 1:
            raise ReleaseError("Duplicate prior Windows installers.")
        if matching:
            asset = matching[0]
            payload = api.call("GET", f"/releases/assets/{asset['id']}", binary=True)
            if payload is None or (asset.get("digest") and asset["digest"] != "sha256:" + hashlib.sha256(payload).hexdigest()):
                raise ReleaseError("Prior MSI download checksum mismatch.")
            destination.mkdir(parents=True, exist_ok=True)
            path = destination / expected
            path.write_bytes(payload)
            return path
    return None


BOOTSTRAP = r'''
if [ "$FORMAT" = deb ]; then
    apt-get -qq update
    apt-get -qq install -y --no-install-recommends build-essential pkg-config libudev-dev binutils dpkg-dev curl ca-certificates xz-utils >/dev/null
else
    dnf -y --setopt=install_weak_deps=False install gcc systemd-devel pkgconf-pkg-config rpm-build redhat-rpm-config tar gzip ca-certificates >/dev/null
    if ! command -v curl >/dev/null; then dnf -y install curl >/dev/null; fi
fi
export CARGO_HOME=/cache/cargo RUSTUP_HOME=/cache/rustup RUSTUP_TOOLCHAIN="$RUST_VERSION"
export PATH=/cache/cargo/bin:$PATH RUSTFLAGS='' CARGO_ENCODED_RUSTFLAGS=''
export CARGO_TARGET_DIR=/work/cargo-target CARGO_BUILD_JOBS=2
if [ ! -x /cache/cargo/bin/rustup ]; then
    curl --fail --silent --show-error --proto '=https' --tlsv1.2 "https://static.rust-lang.org/rustup/archive/$RUSTUP_VERSION/$RUST_TARGET/rustup-init" -o /cache/rustup-init
    printf '%s  /cache/rustup-init\n' "$RUSTUP_SHA256" | sha256sum --check -
    chmod 755 /cache/rustup-init
    /cache/rustup-init -y --no-modify-path --default-toolchain none --profile minimal
fi
rustup toolchain install "$RUST_VERSION" --profile minimal --no-self-update
if [ "$FORMAT" = deb ]; then
    cargo test --locked --all-features
    cargo build --locked --release --all-features
    cargo install cargo-deb --locked --version "$CARGO_DEB_VERSION"
    cargo deb --no-build --output /work/package.deb
else
    mkdir -p /work/rpm/BUILD /work/rpm/BUILDROOT /work/rpm/SOURCES /work/rpm/SPECS /work/rpm/RPMS /work/rpm/SRPMS
    tar -czf "/work/rpm/SOURCES/glasses-broadcaster-$VERSION.tar.gz" --transform="s,^,glasses-broadcaster-$VERSION/," Cargo.toml Cargo.lock rust-toolchain.toml README.md src data fixtures packaging
    rpmbuild -bb --define '_topdir /work/rpm' --define "app_version $VERSION" --define "rust_toolchain $RUST_VERSION" --define "dist $DIST" packaging/rpm/glasses-broadcaster.spec
    cp "/work/rpm/RPMS/$PACKAGE_ARCH/glasses-broadcaster-$VERSION-1$DIST.$PACKAGE_ARCH.rpm" /work/package.rpm
fi
'''

LINUX_VERIFY = r'''
if [ "$FORMAT" = deb ]; then
    apt-get -qq update
    apt-get -qq install -y /work/package.deb >/dev/null
    test "$(dpkg-deb -f /work/package.deb Package)" = glasses-broadcaster
    test "$(dpkg-deb -f /work/package.deb Version)" = "$VERSION-1"
    test "$(dpkg-deb -f /work/package.deb Architecture)" = "$PACKAGE_ARCH"
    dependencies=$(dpkg-deb -f /work/package.deb Depends)
    case "$dependencies" in *libudev1*) ;; *) exit 1;; esac
else
    dnf -y --setopt=install_weak_deps=False install /work/package.rpm >/dev/null
    test "$(rpm -qp --qf '%{NAME}' /work/package.rpm)" = glasses-broadcaster
    test "$(rpm -qp --qf '%{VERSION}' /work/package.rpm)" = "$VERSION"
    test "$(rpm -qp --qf '%{RELEASE}' /work/package.rpm)" = "1$DIST"
    test "$(rpm -qp --qf '%{ARCH}' /work/package.rpm)" = "$PACKAGE_ARCH"
    dependencies=$(rpm -qp --requires /work/package.rpm)
    case "$dependencies" in *libudev.so.1*) ;; *) exit 1;; esac
fi
cd /
/usr/bin/glasses-broadcaster --help >/dev/null
test "$(/usr/bin/glasses-broadcaster --version)" = "glasses-broadcaster $VERSION"
/usr/bin/glasses-broadcaster --check-data
/usr/bin/glasses-broadcaster --list-devices --data-dir /missing-data
if [ "$FORMAT" = deb ]; then
    apt-get -qq install -y --no-install-recommends desktop-file-utils udev diffutils >/dev/null
    dpkg -i /work/package.deb >/dev/null
else
    dnf -y --setopt=install_weak_deps=False install desktop-file-utils systemd-udev diffutils >/dev/null
    rpm -U --replacepkgs /work/package.rpm
    rpm -V glasses-broadcaster
fi
diff -r /source/data /usr/share/glasses-broadcaster/data
desktop-file-validate /usr/share/applications/glasses-broadcaster.desktop
cmp /source/packaging/linux/70-glasses-broadcaster.rules /usr/lib/udev/rules.d/70-glasses-broadcaster.rules
test "$(stat -c %a /usr/lib/glasses-broadcaster/launch.sh)" = 755
udev_version=$(udevadm --version); udev_version=${udev_version%% *}
if [ "$udev_version" -ge 254 ]; then
    udevadm verify /usr/lib/udev/rules.d/70-glasses-broadcaster.rules
else
    udevadm test /sys/class/tty/tty >/work/udev-check.log 2>&1
fi
if [ "$FORMAT" = deb ]; then
    dpkg --purge glasses-broadcaster >/dev/null
else
    rpm -e glasses-broadcaster
fi
test ! -e /usr/bin/glasses-broadcaster
test ! -e /usr/share/glasses-broadcaster/data
test ! -e /usr/share/applications/glasses-broadcaster.desktop
test ! -e /usr/lib/glasses-broadcaster/launch.sh
test ! -e /usr/lib/udev/rules.d/70-glasses-broadcaster.rules
'''


def build_linux(config, target, version, destination, allow_emulation=False):
    daemon_architecture = native_architecture(output(("docker", "info", "--format", "{{.Architecture}}")))
    native = daemon_architecture == target["architecture"]
    if os.environ.get("GITHUB_ACTIONS") == "true" and native_architecture() != target["architecture"]:
        raise ReleaseError("Release runner architecture does not match the selected target.")
    if not native and (not allow_emulation or os.environ.get("GITHUB_ACTIONS") == "true"):
        raise ReleaseError("Release jobs require a native processor; --allow-emulation is local-only.")
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    work = destination / "work"
    cache = ROOT / "target/packaging-cache" / target["id"]
    work.mkdir(exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    tools = config["tools"]
    environment = dict(FORMAT=target["format"], VERSION=version, PACKAGE_ARCH=target["package_arch"],
                       DIST=target.get("dist", ""), RUST_VERSION=tools["rust"], RUSTUP_VERSION=tools["rustup"],
                       RUSTUP_SHA256=tools["rustup_sha256"][target["architecture"]], RUST_TARGET=target["rust_target"],
                       CARGO_DEB_VERSION=tools["cargo_deb"], DEBIAN_FRONTEND="noninteractive")
    base = ["docker", "run", "--rm", "--platform", target["platform"], "--mount", f"type=bind,src={ROOT},dst=/source,readonly",
            "--mount", f"type=bind,src={work},dst=/work", "--workdir", "/source"]
    for key, value in environment.items():
        base.extend(("--env", f"{key}={value}"))
    run(base + ["--mount", f"type=bind,src={cache},dst=/cache", target["image"], "sh", "-ec", BOOTSTRAP])
    run(base + [target["image"], "sh", "-ec", LINUX_VERIFY])
    package = destination / artifact_name(target, version)
    shutil.copy2(work / f"package.{target['format']}", package)
    write_receipt(target, version, package, package.with_name(package.name + ".json"),
                  dict(version=version, architecture=target["package_arch"], distribution=target["id"]),
                  required_checks(target), native=native)
    print(package)


def macos_metadata(target, version, package, destination):
    with tempfile.TemporaryDirectory(dir=destination) as temporary:
        expanded = Path(temporary) / "expanded"
        run(("pkgutil", "--expand", package, expanded))
        distribution = ET.parse(expanded / "Distribution").getroot()
        if (distribution.find("options").get("hostArchitectures") != target["architecture"]
                or distribution.find("volume-check/allowed-os-versions/os-version").get("min") != target["minimum_os"]):
            raise ReleaseError("macOS installer architecture/minimum mismatch.")
        component = ET.parse(expanded / "glasses-broadcaster-component.pkg/PackageInfo").getroot()
        if component.get("version") != version or component.get("install-location") != "/" or component.get("relocatable") != "false":
            raise ReleaseError("macOS package metadata mismatch.")
    return dict(version=version, architecture=target["architecture"], minimum_os=target["minimum_os"])


def attest(config, target, version, package, destination, proof_path, macos_sign=False):
    if native_architecture() != target["architecture"] or platform.system().lower() != {"macos": "darwin", "windows": "windows"}[target["os"]]:
        raise ReleaseError("Attestation must run on the native platform.")
    proof = json.loads(Path(proof_path).read_text(encoding="utf-8-sig"))
    signed = target["os"] == "macos" and macos_sign
    if (proof.get("version") != version or proof.get("architecture") != target["architecture"]
            or proof.get("installed") is not True or proof.get("uninstalled") is not True or proof.get("signed") is not signed
            or proof.get("sha256") != sha256(package) or (signed and proof.get("timestamped") is not True)):
        raise ReleaseError("Native installation proof is incomplete or mismatched.")
    metadata = dict(version=version, architecture=target["architecture"])
    if target["os"] == "macos":
        metadata = macos_metadata(target, version, package, destination)
        if signed and proof.get("notarized") is not True:
            raise ReleaseError("macOS notarization proof is missing.")
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    final = destination / artifact_name(target, version, macos_sign)
    if package.resolve() != final.resolve():
        shutil.copy2(package, final)
    write_receipt(target, version, final, final.with_name(final.name + ".json"), metadata,
                  required_checks(target, macos_sign), signed=signed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    pre = commands.add_parser("preflight")
    pre.add_argument("--tag", required=True)
    pre.add_argument("--macos-sign", choices=("0", "1"), default="0")
    pre.add_argument("--github-output", type=Path)
    build = commands.add_parser("build-linux")
    build.add_argument("--target", required=True)
    build.add_argument("--version", required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--allow-emulation", action="store_true")
    proof = commands.add_parser("attest")
    proof.add_argument("--target", required=True)
    proof.add_argument("--version", required=True)
    proof.add_argument("--package", type=Path, required=True)
    proof.add_argument("--output", type=Path, required=True)
    proof.add_argument("--proof", type=Path, required=True)
    proof.add_argument("--macos-sign", choices=("0", "1"), default="0")
    previous = commands.add_parser("previous-msi")
    previous.add_argument("--version", required=True)
    previous.add_argument("--output", type=Path, required=True)
    previous.add_argument("--github-output", type=Path)
    pub = commands.add_parser("publish")
    pub.add_argument("--tag", required=True)
    pub.add_argument("--artifacts", type=Path, required=True)
    pub.add_argument("--output", type=Path, required=True)
    pub.add_argument("--macos-sign", choices=("0", "1"), default="0")
    args = parser.parse_args()
    config = load_config()
    manifest = tomllib.loads((ROOT / "Cargo.toml").read_text(encoding="utf-8"))
    version = validate_tag(getattr(args, "tag", "v" + getattr(args, "version", "")), manifest["package"]["version"])
    macos_sign = getattr(args, "macos_sign", "0") == "1"
    if args.command == "preflight":
        commit = source_commit()
        run(("git", "diff", "--quiet", "HEAD", "--", "."), cwd=ROOT)
        if output(("git", "rev-parse", f"refs/tags/{args.tag}^{{commit}}"), cwd=ROOT) != commit:
            raise ReleaseError("The triggering tag must point at the checked-out commit.")
        values = dict(version=version, commit=commit, macos_sign=str(int(macos_sign)),
                      linux=json.dumps({"include": [t for t in targets(config) if t["os"] == "linux"]}, separators=(",", ":")),
                      macos=json.dumps(config["macos"]), windows=json.dumps(config["windows"]), **config["tools"])
        values.pop("rustup_sha256")
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8") as stream:
                for key, value in values.items():
                    stream.write(f"{key}={value}\n")
        print(json.dumps(values, indent=2))
    elif args.command == "build-linux":
        target = target_for(config, args.target)
        if target["os"] != "linux":
            raise ReleaseError("build-linux requires a Linux matrix target.")
        build_linux(config, target, version, args.output, args.allow_emulation)
    elif args.command == "attest":
        args.output.mkdir(parents=True, exist_ok=True)
        attest(config, target_for(config, args.target), version, args.package.resolve(), args.output.resolve(), args.proof, macos_sign)
    elif args.command == "previous-msi":
        api = GitHub(os.environ.get("GITHUB_REPOSITORY", ""), os.environ.get("GH_TOKEN", ""))
        path = previous_msi(api, config, version, args.output.resolve())
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8") as stream:
                stream.write(f"path={path or ''}\n")
        print(path or "No prior Windows MSI is available; upgrade check will start with the next release.")
    else:
        commit = source_commit()
        run(("git", "diff", "--quiet", "HEAD", "--", "."), cwd=ROOT)
        packages = collect_artifacts(args.artifacts, config, version, commit, macos_sign)
        args.output.mkdir(parents=True, exist_ok=True)
        sums = args.output / "SHA256SUMS"
        sums.write_text("".join(f"{receipt['sha256']}  {path.name}\n" for path, receipt in sorted(packages)), encoding="utf-8")
        index = args.output / "release-manifest.json"
        index.write_text(json.dumps([receipt for _, receipt in packages], indent=2) + "\n", encoding="utf-8")
        repository = os.environ.get("GITHUB_REPOSITORY", "")
        api = GitHub(repository, os.environ.get("GH_TOKEN", ""))
        publish(api, args.tag, commit, [path for path, _ in packages] + [sums, index], download_table(config, version, repository, macos_sign))


if __name__ == "__main__":
    try:
        main()
    except (ReleaseError, OSError, subprocess.CalledProcessError, KeyError) as exc:
        print(f"Release blocked: {exc}", file=sys.stderr)
        sys.exit(1)