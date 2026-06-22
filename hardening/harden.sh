#!/bin/sh
set -e

echo "Running Beout_OS Appliance Hardening..."

# 1. Disable root login & set impossible password
usermod -p '!' root
usermod -s /usr/sbin/nologin root

# 2. Disable SSH (if installed)
if [ -f /lib/systemd/system/ssh.service ]; then
    systemctl disable ssh 2>/dev/null || true
fi

# 3. Secure GRUB Bootloader
if [ -f /etc/default/grub ]; then
    sed -i 's/GRUB_TIMEOUT=5/GRUB_TIMEOUT=0/' /etc/default/grub
    sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="quiet"/GRUB_CMDLINE_LINUX_DEFAULT="quiet splash net.ifnames=0 biosdevname=0 audit=1"/' /etc/default/grub
    update-grub 2>/dev/null || true
fi

# 4. Restrict permissions on sensitive files
chmod 600 /etc/shadow
chmod 644 /etc/passwd
chmod 600 /etc/crontab 2>/dev/null || true
chmod 600 /etc/ssh/sshd_config 2>/dev/null || true

# 5. Disable unused network protocols (SCTP, RDS, TIPC)
cat <<EOF > /etc/modprobe.d/beout_os-blacklist.conf
install dccp /bin/true
install sctp /bin/true
install rds /bin/true
install tipc /bin/true
EOF

# 6. Mask all getty services — only our provisioning console will run on tty1
ln -sf /dev/null /etc/systemd/system/getty@tty1.service 2>/dev/null || true
ln -sf /dev/null /etc/systemd/system/getty@tty2.service 2>/dev/null || true
ln -sf /dev/null /etc/systemd/system/getty@tty3.service 2>/dev/null || true
ln -sf /dev/null /etc/systemd/system/getty@tty4.service 2>/dev/null || true
ln -sf /dev/null /etc/systemd/system/getty@tty5.service 2>/dev/null || true
ln -sf /dev/null /etc/systemd/system/getty@tty6.service 2>/dev/null || true
ln -sf /dev/null /etc/systemd/system/getty-static.service 2>/dev/null || true

# 7. Force enable the provisioning and api services manually
# (Using symlinks instead of systemctl enable — safe inside chroot)
mkdir -p /etc/systemd/system/multi-user.target.wants/
ln -sf /lib/systemd/system/beout_os-provisioning.service /etc/systemd/system/multi-user.target.wants/beout_os-provisioning.service
ln -sf /lib/systemd/system/beout_os-api.service /etc/systemd/system/multi-user.target.wants/beout_os-api.service

echo "Hardening complete."
