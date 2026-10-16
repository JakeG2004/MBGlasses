#!/bin/bash

# This script sets up persistent access for all USB devices on Arch Linux

echo "Creating udev rule for USB devices..."
cat <<EOF | sudo tee /etc/udev/rules.d/99-usb-storage.rules
SUBSYSTEM=="usb", ATTR{product}=="*", TAG+="uaccess"
SUBSYSTEM=="block", KERNEL=="sd*", TAG+="uaccess"
EOF

echo "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Installing udisks2 and udiskie for auto-mounting..."
sudo pacman -Syu --noconfirm udisks2 udiskie

echo "Setup complete. Run 'udiskie & ' in your user session to enable auto-mounting."
