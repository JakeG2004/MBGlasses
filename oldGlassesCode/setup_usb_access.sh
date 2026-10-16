#!/bin/bash

# This script sets up persistent access for all USB devices on Ubuntu Linux

echo "Creating udev rule for USB devices..."
cat <<EOF | sudo tee /etc/udev/rules.d/99-usb-storage.rules
SUBSYSTEM=="usb", ATTR{product}=="*", MODE="0666"
SUBSYSTEM=="block", KERNEL=="sd*", GROUP="plugdev", MODE="0660"
EOF

echo "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Adding current user to plugdev group..."
sudo usermod -aG plugdev $USER

echo "Installing usbmount for auto-mounting on headless systems..."
sudo apt update
sudo apt install -y usbmount

echo "Configuring usbmount options..."
sudo sed -i 's|^MOUNTOPTIONS=.*|MOUNTOPTIONS="sync,noexec,nodev,noatime,nodiratime,umask=000"|' /etc/usbmount/usbmount.conf

echo "Setup complete. Please reboot for changes to take full effect."
