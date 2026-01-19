#!/bin/bash
# Install QTermWidget dependencies for Raymap Editor

echo "=== Installing QTermWidget for Raymap Editor ==="
echo ""

# Detect Qt version
if command -v qmake6 &> /dev/null; then
    QT_VERSION=6
    PACKAGE="qtermwidget-qt6"
    DEV_PACKAGE="qtermwidget-qt6-dev"
elif command -v qmake &> /dev/null; then
    QT_VERSION=5
    PACKAGE="qtermwidget5"
    DEV_PACKAGE="qtermwidget5-dev"
else
    echo "ERROR: Qt not found. Please install Qt first."
    exit 1
fi

echo "Detected Qt${QT_VERSION}"
echo "Installing ${PACKAGE}..."
echo ""

# Install based on distribution
if [ -f /etc/debian_version ]; then
    # Debian/Ubuntu/Linux Mint
    sudo apt-get update
    sudo apt-get install -y ${DEV_PACKAGE} lib${PACKAGE}-0
elif [ -f /etc/arch-release ]; then
    # Arch Linux
    sudo pacman -S --noconfirm qtermwidget
elif [ -f /etc/fedora-release ]; then
    # Fedora
    sudo dnf install -y qtermwidget-devel
else
    echo "WARNING: Unknown distribution. Please install qtermwidget manually."
    echo "Package names:"
    echo "  - Debian/Ubuntu: ${DEV_PACKAGE}"
    echo "  - Arch: qtermwidget"
    echo "  - Fedora: qtermwidget-devel"
    exit 1
fi

echo ""
echo "✓ QTermWidget installed successfully!"
echo ""
echo "Next steps:"
echo "1. cd build"
echo "2. cmake .."
echo "3. make"
echo ""
