# Beout_OS - Provisioning CLI

This module implements the initial hardware provisioning interface for the Beout_OS Security Appliance.
It is designed to run on `tty1` in place of a standard Linux shell to ensure the appliance remains locked down (no bash access).

## Features
- **Network Configuration**: Supports setting up WAN, LAN, and Management interfaces with basic IPv4 validation.
- **SQLite Database Integration**: All configurations are securely stored using the `database` module (`/var/lib/beout_os/config.db`).
- **System Lifecycle**: Exposes Reboot, Shutdown, and Factory Reset triggers without exposing system internals to the user.
- **Strict Input Validation**: Regular expressions ensure inputs like IP addresses and subnet masks are syntactically valid before persisting.

## Testing
Run unit tests with:
```bash
./build.sh test
```
