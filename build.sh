#!/usr/bin/env bash
set -euo pipefail

# Build script for Beout_OS Security Appliance

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
CMAKE_BUILD_TYPE="Release"

function usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  clean      Clean build directory"
    echo "  configure  Run CMake configure"
    echo "  build      Build all targets"
    echo "  test       Run unit tests"
    echo "  iso        Build the Beout_OS installer ISO"
    echo "  all        Clean, configure, build, and test"
}

function clean() {
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
}

function configure() {
    echo "Configuring CMake (${CMAKE_BUILD_TYPE})..."
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    cmake -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" ..
}

function build() {
    echo "Building project..."
    cmake --build "${BUILD_DIR}" -j"$(nproc)"
}

function run_tests() {
    echo "Running tests..."
    cd "${BUILD_DIR}"
    ctest --output-on-failure
}

function build_iso() {
    echo "============================================"
    echo "  Beout_OS ISO Builder"
    echo "============================================"
    echo ""

    # Step 1: Build the .deb package
    echo "[1/4] Building .deb package..."
    "${PROJECT_ROOT}/packaging/build_deb.sh"

    # Step 2: Configure live-build
    echo "[2/4] Configuring live-build..."
    cd "${PROJECT_ROOT}/installer"
    lb config

    # Step 3: Inject custom installer + packages into the live filesystem
    echo "[3/4] Injecting Beout_OS installer into live image..."

    # Create the includes.chroot directory structure
    # Files placed here appear in the live filesystem at the same path
    local CHROOT_DIR="${PROJECT_ROOT}/installer/config/includes.chroot"

    # Installer files
    mkdir -p "${CHROOT_DIR}/opt/beout_os/installer"
    cp "${PROJECT_ROOT}/installer/beout_installer.sh" "${CHROOT_DIR}/opt/beout_os/installer/"
    chmod +x "${CHROOT_DIR}/opt/beout_os/installer/beout_installer.sh"

    # Copy the .deb package (the installer will dpkg -i this into the target disk)
    cp "${PROJECT_ROOT}/beout_os-core.deb" "${CHROOT_DIR}/opt/beout_os/installer/"

    # Copy the hardening script (the installer will run this inside the target chroot)
    cp "${PROJECT_ROOT}/hardening/harden.sh" "${CHROOT_DIR}/opt/beout_os/installer/"
    chmod +x "${CHROOT_DIR}/opt/beout_os/installer/harden.sh"

    # Create systemd service that auto-runs our installer on boot
    mkdir -p "${CHROOT_DIR}/etc/systemd/system"
    cat <<'EOF' > "${CHROOT_DIR}/etc/systemd/system/beout-installer.service"
[Unit]
Description=Beout_OS Custom Installer
After=multi-user.target
Conflicts=getty@tty1.service

[Service]
Type=idle
ExecStart=/opt/beout_os/installer/beout_installer.sh
StandardInput=tty
StandardOutput=tty
TTYPath=/dev/tty1
Restart=no
User=root

[Install]
WantedBy=multi-user.target
EOF

    # Enable the installer service by creating the symlink
    mkdir -p "${CHROOT_DIR}/etc/systemd/system/multi-user.target.wants"
    ln -sf /etc/systemd/system/beout-installer.service \
        "${CHROOT_DIR}/etc/systemd/system/multi-user.target.wants/beout-installer.service"

    # Mask getty@tty1 in the live environment so our installer gets the console
    ln -sf /dev/null "${CHROOT_DIR}/etc/systemd/system/getty@tty1.service"

    # Step 4: Build the ISO
    echo "[4/4] Building ISO image (this takes several minutes)..."
    sudo lb build

    echo ""
    echo "============================================"
    echo "  ISO BUILD COMPLETE"
    echo "============================================"
    echo ""
    echo "  Output: ${PROJECT_ROOT}/installer/live-image-amd64.hybrid.iso"
    echo ""
}

if [[ $# -eq 0 ]]; then
    usage
    exit 1
fi

case "$1" in
    clean) clean ;;
    configure) configure ;;
    build) build ;;
    test) run_tests ;;
    iso) build_iso ;;
    all) clean; configure; build; run_tests ;;
    *) usage; exit 1 ;;
esac
