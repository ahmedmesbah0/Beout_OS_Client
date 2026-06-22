#!/bin/bash
# =============================================================
#  BEOUT_OS — Enterprise Security Appliance Installer
#  Version 1.0.0
#
#  Custom text-based installer (Proxmox-style)
#  Completely replaces the Debian installer
# =============================================================
set -euo pipefail

# === PATHS ===
INSTALLER_DIR="/opt/beout_os/installer"
DEB_PKG="${INSTALLER_DIR}/beout_os-core.deb"
HARDEN_SCRIPT="${INSTALLER_DIR}/harden.sh"
TARGET_MNT="/mnt/target"
DEBIAN_RELEASE="bookworm"
HOSTNAME="beoutos"
LOG_FILE="/tmp/beout_install.log"

# === COLORS ===
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

# === GLOBAL STATE ===
TARGET_DISK=""
IS_EFI=false
TOTAL_STEPS=10
CURRENT_STEP=0

# === UTILITY FUNCTIONS ===

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG_FILE"
}

die() {
    echo -e "\n  ${RED}${BOLD}FATAL ERROR:${NC} $1"
    echo -e "  ${DIM}Installation log saved to: ${LOG_FILE}${NC}"
    echo ""
    echo -e "  ${YELLOW}Press any key to drop to a recovery shell...${NC}"
    read -n 1 -s
    exec /bin/bash
}

progress_bar() {
    local percent=$1
    local width=40
    local filled=$(( percent * width / 100 ))
    local empty=$(( width - filled ))
    local bar=""
    for ((i=0; i<filled; i++)); do bar+="█"; done
    for ((i=0; i<empty; i++)); do bar+="░"; done
    echo -ne "\r  [${CYAN}${bar}${NC}] ${WHITE}${percent}%${NC}  "
}

step() {
    CURRENT_STEP=$((CURRENT_STEP + 1))
    local msg="$1"
    local pct=$(( CURRENT_STEP * 100 / TOTAL_STEPS ))
    echo ""
    echo -e "  ${CYAN}${BOLD}Step ${CURRENT_STEP}/${TOTAL_STEPS}:${NC} ${WHITE}${msg}${NC}"
    progress_bar "$pct"
    echo ""
    log "STEP ${CURRENT_STEP}/${TOTAL_STEPS}: ${msg}"
}

print_header() {
    clear
    echo -e "${CYAN}"
    cat <<'BANNER'

    ╔═════════════════════════════════════════════════════════════════════╗
    ║                                                                     ║
    ║   ██████╗ ███████╗ ██████╗ ██╗   ██╗████████╗    ██████╗ ███████╗   ║
    ║   ██╔══██╗██╔════╝██╔═══██╗██║   ██║╚══██╔══╝   ██╔═══██╗██╔════╝   ║
    ║   ██████╔╝█████╗  ██║   ██║██║   ██║   ██║      ██║   ██║███████╗   ║
    ║   ██╔══██╗██╔══╝  ██║   ██║██║   ██║   ██║      ██║   ██║╚════██║   ║
    ║   ██████╔╝███████╗╚██████╔╝╚██████╔╝   ██║      ╚██████╔╝███████║   ║
    ║   ╚═════╝ ╚══════╝ ╚═════╝  ╚═════╝    ╚═╝       ╚═════╝ ╚══════╝   ║
    ║                                                                     ║
    ║                    Enterprise Security Appliance                    ║
    ║                          Installer v1.0.0                           ║
    ║                                                                     ║
    ╚═════════════════════════════════════════════════════════════════════╝

BANNER
    echo -e "${NC}"
}

print_separator() {
    echo -e "  ${DIM}───────────────────────────────────────────────────────${NC}"
}

# === PHASE 1: WELCOME ===

show_welcome() {
    print_header
    echo -e "  ${WHITE}${BOLD}Welcome to the Beout_OS Installation Wizard${NC}"
    echo ""
    echo -e "  This installer will deploy the Beout_OS Enterprise"
    echo -e "  Security Appliance to the selected target disk."
    echo ""
    echo -e "  ${YELLOW}⚠  WARNING: The selected disk will be completely erased.${NC}"
    echo ""
    print_separator
    echo ""
    echo -e "  ${WHITE}System Information:${NC}"
    echo -e "    CPU:     $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
    echo -e "    RAM:     $(free -h | awk '/Mem:/{print $2}')"
    echo -e "    Boot:    $([ -d /sys/firmware/efi ] && echo 'UEFI' || echo 'Legacy BIOS')"
    echo ""
    print_separator
    echo ""
    echo -ne "  ${CYAN}Press ENTER to continue or Ctrl+C to abort...${NC} "
    read -r
}

# === PHASE 2: DISK DETECTION ===

detect_disks() {
    print_header
    echo -e "  ${WHITE}${BOLD}Target Disk Selection${NC}"
    echo ""

    # Detect available block devices (exclude loop, CD-ROM, and partitions)
    mapfile -t DISK_DEVS < <(lsblk -dpno NAME | grep -E '/dev/(sd|vd|nvme|xvd)' || true)

    if [ ${#DISK_DEVS[@]} -eq 0 ]; then
        die "No suitable disks found. Please attach a disk and try again."
    fi

    echo -e "  ${WHITE}Available disks:${NC}"
    echo ""

    local idx=1
    for dev in "${DISK_DEVS[@]}"; do
        local size
        size=$(lsblk -dpno SIZE "$dev" | xargs)
        local model
        model=$(lsblk -dpno MODEL "$dev" 2>/dev/null | xargs || echo "Unknown")
        echo -e "    ${CYAN}[${idx}]${NC}  ${dev}  —  ${size}  (${model})"
        idx=$((idx + 1))
    done

    echo ""
    print_separator
    echo ""

    local choice
    while true; do
        echo -ne "  ${WHITE}Select target disk [1-${#DISK_DEVS[@]}]: ${NC}"
        read -r choice
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#DISK_DEVS[@]}" ]; then
            TARGET_DISK="${DISK_DEVS[$((choice - 1))]}"
            break
        fi
        echo -e "  ${RED}Invalid selection. Try again.${NC}"
    done

    echo ""
    local disk_size
    disk_size=$(lsblk -dpno SIZE "$TARGET_DISK" | xargs)
    echo -e "  ${YELLOW}${BOLD}⚠  ALL DATA on ${TARGET_DISK} (${disk_size}) will be DESTROYED.${NC}"
    echo ""
    echo -ne "  ${WHITE}Type ${BOLD}YES${NC}${WHITE} to confirm: ${NC}"
    local confirm
    read -r confirm
    if [ "$confirm" != "YES" ]; then
        echo -e "\n  ${RED}Installation aborted by user.${NC}"
        sleep 3
        exec /bin/bash
    fi
}

# === PHASE 3: PARTITIONING ===

partition_disk() {
    step "Partitioning target disk ${TARGET_DISK}"

    # Wipe existing partition table
    wipefs -af "$TARGET_DISK" >> "$LOG_FILE" 2>&1 || true
    dd if=/dev/zero of="$TARGET_DISK" bs=1M count=10 >> "$LOG_FILE" 2>&1 || true

    if [ -d /sys/firmware/efi ]; then
        IS_EFI=true
        log "EFI mode detected. Creating GPT partition table."
        parted -s "$TARGET_DISK" mklabel gpt
        parted -s "$TARGET_DISK" mkpart ESP fat32 1MiB 513MiB
        parted -s "$TARGET_DISK" set 1 esp on
        parted -s "$TARGET_DISK" mkpart primary ext4 513MiB 100%
    else
        IS_EFI=false
        log "Legacy BIOS mode detected. Creating MBR partition table."
        parted -s "$TARGET_DISK" mklabel msdos
        parted -s "$TARGET_DISK" mkpart primary ext4 1MiB 100%
        parted -s "$TARGET_DISK" set 1 boot on
    fi

    # Wait for kernel to re-read partition table
    partprobe "$TARGET_DISK" 2>/dev/null || true
    sleep 2

    log "Partitioning complete."
}

# === HELPER: Get partition device name ===

get_partition() {
    local disk="$1"
    local num="$2"
    # Handle both /dev/sda1 and /dev/nvme0n1p1 naming
    if echo "$disk" | grep -q "nvme"; then
        echo "${disk}p${num}"
    else
        echo "${disk}${num}"
    fi
}

# === PHASE 4: FORMATTING ===

format_partitions() {
    step "Formatting partitions"

    if $IS_EFI; then
        local efi_part
        efi_part=$(get_partition "$TARGET_DISK" 1)
        local root_part
        root_part=$(get_partition "$TARGET_DISK" 2)

        log "Formatting EFI partition: ${efi_part}"
        mkfs.fat -F32 "$efi_part" >> "$LOG_FILE" 2>&1

        log "Formatting root partition: ${root_part}"
        mkfs.ext4 -F -L "BEOUTOS_ROOT" "$root_part" >> "$LOG_FILE" 2>&1
    else
        local root_part
        root_part=$(get_partition "$TARGET_DISK" 1)

        log "Formatting root partition: ${root_part}"
        mkfs.ext4 -F -L "BEOUTOS_ROOT" "$root_part" >> "$LOG_FILE" 2>&1
    fi

    log "Formatting complete."
}

# === PHASE 5: MOUNT ===

mount_target() {
    step "Mounting target filesystem"

    mkdir -p "$TARGET_MNT"

    if $IS_EFI; then
        local root_part
        root_part=$(get_partition "$TARGET_DISK" 2)
        mount "$root_part" "$TARGET_MNT"

        mkdir -p "${TARGET_MNT}/boot/efi"
        local efi_part
        efi_part=$(get_partition "$TARGET_DISK" 1)
        mount "$efi_part" "${TARGET_MNT}/boot/efi"
    else
        local root_part
        root_part=$(get_partition "$TARGET_DISK" 1)
        mount "$root_part" "$TARGET_MNT"
    fi

    log "Filesystems mounted at ${TARGET_MNT}"
}

# === PHASE 6: DEBOOTSTRAP ===

install_base_system() {
    step "Installing Debian base system (this may take several minutes)"

    debootstrap --arch=amd64 "$DEBIAN_RELEASE" "$TARGET_MNT" http://deb.debian.org/debian >> "$LOG_FILE" 2>&1 || \
        die "debootstrap failed. Check ${LOG_FILE} for details."

    log "Base system installed successfully."
}

# === PHASE 7: CHROOT SETUP ===

configure_system() {
    step "Configuring the operating system"

    # Mount special filesystems for chroot
    mount --bind /dev  "${TARGET_MNT}/dev"
    mount --bind /dev/pts "${TARGET_MNT}/dev/pts"
    mount -t proc proc "${TARGET_MNT}/proc"
    mount -t sysfs sys "${TARGET_MNT}/sys"

    # Configure APT sources
    cat <<EOF > "${TARGET_MNT}/etc/apt/sources.list"
deb http://deb.debian.org/debian ${DEBIAN_RELEASE} main contrib non-free non-free-firmware
deb http://deb.debian.org/debian ${DEBIAN_RELEASE}-updates main contrib non-free non-free-firmware
deb http://security.debian.org/debian-security ${DEBIAN_RELEASE}-security main contrib non-free non-free-firmware
EOF

    # Set hostname
    echo "$HOSTNAME" > "${TARGET_MNT}/etc/hostname"
    cat <<EOF > "${TARGET_MNT}/etc/hosts"
127.0.0.1   localhost
127.0.1.1   ${HOSTNAME}
EOF

    # Set locale
    echo "en_US.UTF-8 UTF-8" > "${TARGET_MNT}/etc/locale.gen"

    # Configure fstab
    if $IS_EFI; then
        local root_uuid
        root_uuid=$(blkid -s UUID -o value "$(get_partition "$TARGET_DISK" 2)")
        local efi_uuid
        efi_uuid=$(blkid -s UUID -o value "$(get_partition "$TARGET_DISK" 1)")
        cat <<EOF > "${TARGET_MNT}/etc/fstab"
UUID=${root_uuid}  /          ext4  errors=remount-ro  0  1
UUID=${efi_uuid}   /boot/efi  vfat  umask=0077         0  1
EOF
    else
        local root_uuid
        root_uuid=$(blkid -s UUID -o value "$(get_partition "$TARGET_DISK" 1)")
        cat <<EOF > "${TARGET_MNT}/etc/fstab"
UUID=${root_uuid}  /  ext4  errors=remount-ro  0  1
EOF
    fi

    # Update package cache inside chroot and install essential packages
    chroot "${TARGET_MNT}" /bin/bash -c "
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -y -qq \
            linux-image-amd64 \
            systemd-sysv \
            locales \
            openssl \
            libssl3 \
            sqlite3 \
            libsqlite3-0 \
            iproute2 \
            ifupdown \
            net-tools \
            ca-certificates \
            grub-pc
    " >> "$LOG_FILE" 2>&1 || die "Failed to install system packages."

    # Generate locale
    chroot "${TARGET_MNT}" locale-gen >> "$LOG_FILE" 2>&1 || true

    log "System configuration complete."
}

# === PHASE 8: INSTALL BEOUT_OS ===

install_beout_os() {
    step "Installing Beout_OS Security Appliance packages"

    # Copy the .deb package into the chroot
    cp "$DEB_PKG" "${TARGET_MNT}/tmp/beout_os-core.deb"

    # Install it
    chroot "${TARGET_MNT}" /bin/bash -c "
        export DEBIAN_FRONTEND=noninteractive
        dpkg -i /tmp/beout_os-core.deb || true
        apt-get -f install -y -qq
        rm -f /tmp/beout_os-core.deb
    " >> "$LOG_FILE" 2>&1 || die "Failed to install Beout_OS package."

    log "Beout_OS package installed successfully."
}

# === PHASE 9: HARDENING ===

apply_hardening() {
    step "Applying enterprise security hardening"

    cp "$HARDEN_SCRIPT" "${TARGET_MNT}/tmp/harden.sh"
    chmod +x "${TARGET_MNT}/tmp/harden.sh"

    chroot "${TARGET_MNT}" /bin/bash /tmp/harden.sh >> "$LOG_FILE" 2>&1 || \
        log "WARNING: Some hardening steps may have failed in chroot."

    rm -f "${TARGET_MNT}/tmp/harden.sh"

    log "Hardening applied."
}

# === PHASE 10: BOOTLOADER ===

install_bootloader() {
    step "Installing GRUB bootloader"

    if $IS_EFI; then
        chroot "${TARGET_MNT}" /bin/bash -c "
            apt-get install -y -qq grub-efi-amd64
            grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=beoutos --recheck
            update-grub
        " >> "$LOG_FILE" 2>&1 || die "GRUB EFI installation failed."
    else
        chroot "${TARGET_MNT}" /bin/bash -c "
            grub-install --target=i386-pc ${TARGET_DISK}
            update-grub
        " >> "$LOG_FILE" 2>&1 || die "GRUB BIOS installation failed."
    fi

    # Customize GRUB for appliance mode
    sed -i 's/GRUB_TIMEOUT=5/GRUB_TIMEOUT=0/' "${TARGET_MNT}/etc/default/grub" 2>/dev/null || true
    chroot "${TARGET_MNT}" update-grub >> "$LOG_FILE" 2>&1 || true

    log "Bootloader installed."
}

# === CLEANUP ===

cleanup() {
    echo ""
    echo -e "  ${DIM}Unmounting filesystems...${NC}"

    # Unmount in reverse order
    umount "${TARGET_MNT}/dev/pts"  2>/dev/null || true
    umount "${TARGET_MNT}/dev"      2>/dev/null || true
    umount "${TARGET_MNT}/proc"     2>/dev/null || true
    umount "${TARGET_MNT}/sys"      2>/dev/null || true

    if $IS_EFI; then
        umount "${TARGET_MNT}/boot/efi" 2>/dev/null || true
    fi

    umount "${TARGET_MNT}" 2>/dev/null || true

    log "Cleanup complete."
}

# === COMPLETION SCREEN ===

show_complete() {
    print_header
    echo -e "  ${GREEN}${BOLD}╔═══════════════════════════════════════════════════╗${NC}"
    echo -e "  ${GREEN}${BOLD}║                                                   ║${NC}"
    echo -e "  ${GREEN}${BOLD}║   ✓  Installation Complete!                       ║${NC}"
    echo -e "  ${GREEN}${BOLD}║                                                   ║${NC}"
    echo -e "  ${GREEN}${BOLD}╚═══════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  ${WHITE}Beout_OS has been successfully installed to ${TARGET_DISK}.${NC}"
    echo ""
    print_separator
    echo ""
    echo -e "  ${WHITE}${BOLD}What happens next:${NC}"
    echo ""
    echo -e "    1. The system will reboot into the appliance."
    echo -e "    2. The ${CYAN}Provisioning Console${NC} will appear on screen."
    echo -e "    3. Configure the Management IP address."
    echo -e "    4. Access the Web Dashboard at ${CYAN}https://<MGMT_IP>:8443${NC}"
    echo ""
    print_separator
    echo ""
    echo -e "  ${WHITE}${BOLD}Default Web Credentials:${NC}"
    echo -e "    Username: ${CYAN}admin${NC}"
    echo -e "    Password: ${CYAN}admin${NC}"
    echo ""
    print_separator
    echo ""
    echo -e "  ${YELLOW}Please remove the installation media before rebooting.${NC}"
    echo ""
    echo -ne "  ${CYAN}Press ENTER to reboot now...${NC} "
    read -r
}

# === MAIN ===

main() {
    log "=== Beout_OS Installer Started ==="
    log "Date: $(date)"

    # Trap errors
    trap cleanup EXIT

    # Phase 1: Welcome
    show_welcome

    # Phase 2: Disk Selection
    detect_disks

    # Phase 3-10: Installation
    print_header
    echo -e "  ${WHITE}${BOLD}Installing Beout_OS Enterprise Security Appliance...${NC}"
    echo ""

    partition_disk
    format_partitions
    mount_target
    install_base_system
    configure_system
    install_beout_os
    apply_hardening
    install_bootloader

    # Cleanup
    cleanup
    trap - EXIT

    # Done
    show_complete

    # Reboot
    reboot
}

main "$@"
