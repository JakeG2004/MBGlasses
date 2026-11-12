%global debug_package %{nil}
%{!?rust_toolchain:%global rust_toolchain stable}

Name:           glasses-broadcaster
Version:        %{app_version}
Release:        1%{?dist}
Summary:        Marching band light-show operator console
# The project has no declared license. This is not a redistribution grant.
License:        Unspecified
URL:            https://github.com/JakeG2004/MBGlasses
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc
BuildRequires:  pkgconfig(libudev)
Requires(post): /bin/sh
Requires(postun): /bin/sh

%description
Terminal dashboard for broadcasting marching band light shows over FTDI or
ESP32-H2 USB devices. Includes the bundled show and frame catalog.

%prep
%setup -q

%build
export CARGO_TARGET_DIR="$PWD/target"
cargo +%{rust_toolchain} build --locked --release --all-features

%install
install -Dm755 target/release/glasses-broadcaster %{buildroot}%{_bindir}/glasses-broadcaster
install -d %{buildroot}%{_datadir}/glasses-broadcaster
cp -R data %{buildroot}%{_datadir}/glasses-broadcaster/data
install -Dm755 packaging/linux/launch.sh %{buildroot}%{_prefix}/lib/glasses-broadcaster/launch.sh
install -Dm644 packaging/linux/glasses-broadcaster.desktop %{buildroot}%{_datadir}/applications/glasses-broadcaster.desktop
install -Dm644 packaging/linux/70-glasses-broadcaster.rules %{buildroot}%{_prefix}/lib/udev/rules.d/70-glasses-broadcaster.rules

%check
export CARGO_TARGET_DIR="$PWD/target"
cargo +%{rust_toolchain} test --locked --all-features

%post
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules >/dev/null 2>&1 || true
fi
exit 0

%postun
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules >/dev/null 2>&1 || true
fi
exit 0

%files
%defattr(644,root,root,755)
%doc README.md
%attr(755,root,root) %{_bindir}/glasses-broadcaster
%{_datadir}/glasses-broadcaster/
%{_datadir}/applications/glasses-broadcaster.desktop
%dir %{_prefix}/lib/glasses-broadcaster
%attr(755,root,root) %{_prefix}/lib/glasses-broadcaster/launch.sh
%{_prefix}/lib/udev/rules.d/70-glasses-broadcaster.rules