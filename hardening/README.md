# Beout_OS - Hardening

This module applies the final security lockdown scripts during the `live-build` ISO generation phase.

## Operations Performed
- **Root Lockdown**: Disables root login and shell access natively via `usermod`.
- **Service Masking**: Masks standard TTYs (`tty2` through `tty6`) so users cannot swap virtual consoles to bypass the `tty1` CLI engine.
- **Protocol Blacklist**: Disables obscure network protocols (SCTP, RDS, TIPC) to reduce kernel attack surface.
- **Boot Hardening**: Modifies GRUB to completely hide the boot menu and enable AppArmor and kernel auditing silently.

## Execution
The `harden.sh` script is injected into the installer's chroot hooks (`installer/config/hooks/live/99-harden.chroot`). It executes exactly once during the automated Debian ISO mastering process.
