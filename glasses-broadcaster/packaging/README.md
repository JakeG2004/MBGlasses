# Packaging and releases

This is the maintainer guide. For installation without Rust or build tools, see the [user instructions](../README.md#install-prebuilt-packages). Distribute installers through [GitHub Releases](https://github.com/JakeG2004/MBGlasses/releases), not GitHub Packages. There is no automatic updater or APT/DNF repository feed.

## Targets and artifacts

[`targets.json`](targets.json) is the source of truth for supported versions, processors, native runners, container digests, and tool versions. The matrix contains 20 Linux targets (Debian/Ubuntu and Fedora/RHEL, each on x86-64 and ARM64), one macOS 11+ Apple Silicon target, and one Windows 10/11 x86-64 target. All 22 packages must pass their release gates. Its `expected_packages` inventory must be updated only for an intentional support-matrix change, never to hide missing or failed artifacts.

In filenames, `VERSION` is the numeric version without the tag's `v` prefix; `TARGETID` includes the distribution version and processor.

| Package | Filename |
| :--- | :--- |
| Debian / Ubuntu | `glasses-broadcaster-VERSION-TARGETID.deb` |
| Fedora / RHEL | `glasses-broadcaster-VERSION-TARGETID.rpm` |
| macOS, default unsigned | `glasses-broadcaster-VERSION-macos-11.0-arm64-unsigned.pkg` |
| macOS, optional signed/notarized | `glasses-broadcaster-VERSION-macos-11.0-arm64-signed.pkg` |
| Windows, unsigned | `glasses-broadcaster-VERSION-windows-10-11-x86_64.msi` |

Example Linux target IDs are `ubuntu-24.04-x86_64` and `rhel-9.8-arm64`. Every complete release includes `SHA256SUMS`, generated release notes, and an OS/processor download table. RHEL container compatibility checks are not Red Hat certification.

## Installed behavior

| Platform | Catalog location | Desktop launcher |
| :--- | :--- | :--- |
| Linux | `/usr/share/glasses-broadcaster/data` | Applications-menu entry opens a terminal dashboard |
| macOS | `/Applications/Glasses Broadcaster.app/Contents/Resources/data` | The application opens Terminal |
| Windows | Beside the executable; legacy layout `Program Files\glasses-broadcaster\bin\data` | Start-menu launcher pauses on startup errors |

`--data-dir PATH` explicitly overrides catalog discovery; `./data` is only a development fallback. `--check-data` validates the catalog without USB access or the terminal UI. `--list-devices` does not depend on a catalog being available.

Linux packages grant active-desktop-session access through `uaccess`, scoped to FTDI USB `0403:6001` and ESP32-H2 tty `303a:1001`. Replug the device after installation. Do not add world-writable rules or run the application as root. Normal installer administrator prompts still apply on supported platforms. Firmware flashing and Windows FTDI/WinUSB driver replacement are not automated; preserve the documented one-time driver prerequisite.

## Unsigned Windows MSI

Windows installers and executables are not signed. No Windows-specific Azure account, signing environment, certificate profile, secrets, or OIDC permission is required. Windows may display **Unknown Publisher** or SmartScreen warnings; users should download only from the official GitHub Releases page and verify the file against `SHA256SUMS`. Do not recommend disabling Windows security protections. The native Windows release job still validates MSI metadata, installed files and data, Start-menu shortcut, static runtime, upgrade behavior when a previous MSI exists, and clean removal.

## Optional macOS signing and notarization

The repository variable `MACOS_SIGN` defaults to `0`: no Apple credentials are needed. The installer is unsigned; Apple Silicon app binaries retain only required ad-hoc signatures. Users may need the Gatekeeper approval described in the installation guide.

To enable signing/notarization, set `MACOS_SIGN=1` and configure these secrets:

* Application certificate: `MACOS_APPLICATION_P12_BASE64`, `MACOS_APPLICATION_P12_PASSWORD`, and `MACOS_APPLICATION_IDENTITY` (`Developer ID Application: ...`).
* Installer certificate: `MACOS_INSTALLER_P12_BASE64`, `MACOS_INSTALLER_P12_PASSWORD`, and `MACOS_INSTALLER_IDENTITY` (`Developer ID Installer: ...`).
* Apple API credentials: `APPLE_API_KEY_BASE64` (P8 key), `APPLE_API_KEY_ID`, and `APPLE_API_ISSUER`.

Temporary keychains and private files are cleaned on exit. When signing is enabled, missing configuration, signing errors, or notarization errors must fail the build; there is no unsigned fallback.

## Maintainer validation and local builds

From `glasses-broadcaster/`, run:

```bash
cargo fmt --all -- --check
cargo clippy --locked --all-targets --all-features -- -D warnings
cargo test --locked --all-features
cargo build --locked --release --all-features
python3 -m unittest discover -s packaging/release -p 'test_*.py' -v
```

The release helper requires Python 3.11 or newer; CI pins its interpreter in `targets.json`. Local Linux packaging also requires a running Docker engine. Use versions and tool pins from `targets.json` for packaging. Replace `VERSION` and `OUTPUT` below with the numeric version and output directory.

### Linux

```bash
python3 packaging/release/release.py build-linux \
  --target ubuntu-24.04-x86_64 --version VERSION --output OUTPUT
```

Choose a full target ID from the matrix. Local cross-architecture Docker builds, including x86-64 emulation on ARM64, require explicit `--allow-emulation`. Release CI must use native runners and never enable that option.

### macOS

```bash
bash packaging/macos/build.sh VERSION OUTPUT
```

The default is unsigned. Native CI sets `MACOS_VERIFY_INSTALL=1` to perform an actual install, catalog check, and removal; it refuses to overwrite an existing installation.

### Windows

Run `packaging/windows/build.ps1` phases `Build`, `Package`, and `Verify`, using `-Version VERSION` and `-OutputDirectory OUTPUT`. `Package` requires `-WixVersion` matching `tools.wix` in `targets.json`. `Verify` accepts optional `-PreviousMsi` for an actual upgrade check against a prior release; either signed or unsigned previous MSI files are accepted if their metadata is valid.

## Publish and recover

1. Commit the application, lockfile, matrix, and workflows. Push a stable `vMAJOR.MINOR.PATCH` tag whose numeric version exactly matches `Cargo.toml`. Prereleases and leading zeros are rejected; the MSI version must not exceed `255.255.65535`.
2. The repository-root `.github/workflows/glasses-broadcaster-release.yml` reuses `.github/workflows/glasses-broadcaster.yml` CI and gates publication on all native packages. Concurrency is per tag. Only the publishing job receives `contents: write`; Windows signing credentials and OIDC are not needed. Optional macOS signing uses the configured `release-signing` environment secrets.
3. The workflow creates a draft, uploads the complete artifact set, and verifies remote checksums before publishing. A partial draft is not a completed release.
4. If a run fails, rerun the failed workflow to recover its incomplete draft. Build artifacts are retained for seven days; rerun all jobs if they have expired. Do not publish partial drafts manually. Unexpected draft assets require manual review rather than automatic deletion. Published releases are never silently replaced; corrections require a new version and tag.

Existing `dist-workspace` configuration is not part of this release path; do not depend on it or remove it as part of packaging maintenance.

## Verification limits and maintenance

Reported local evidence as of 2026-10-23:

* All 10 Debian/Ubuntu packages and 9 of 10 RPM packages passed local checks. Fedora 44 x86-64 was blocked by a local Docker emulation tar failure; deferral to mandatory native CI was explicitly approved.
* Native unsigned macOS package creation, staged data, metadata, launchers, and 41 Rust tests passed.
* Native Windows unsigned MSI build/install/removal checks and administrator-level macOS installation/removal were explicitly approved for deferral to mandatory native CI. These checks remain release gates; local evidence does not establish a successful first GitHub tag/release run.
* No physical broadcaster hardware was used or tested. Container checks do not constitute RHEL certification.

Lifecycle sources reviewed on 2026-10-23: [Debian releases](https://www.debian.org/releases/), [Ubuntu release cycle](https://ubuntu.com/about/release-cycle), Fedora [43](https://fedoraproject.org/wiki/Releases/43) / [44](https://fedoraproject.org/wiki/Releases/44), and [Red Hat release dates](https://access.redhat.com/articles/3078). Refresh the supported matrix, image digests, runners, and tool/action pins as vendors retire releases or introduce new versions; revalidate affected native packages.

`Cargo.toml` currently declares no project license. Maintainers must establish distribution rights and provide required project/dependency licenses and notices before publishing. The WiX template's Apache header does not establish the application's license.