# Beout_OS Deployment & Operations Guide

This document provides complete instructions on how to build, deploy, activate, and manage the **Beout_OS Security Appliance**.

---

## 1. Build Environment Requirements

To generate the final bootable ISO, you must execute the build script on a **Debian 12** or **Ubuntu 24.04** host.

### Required Dependencies
Ensure your host machine has the following installed:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev sqlite3 libsqlite3-dev live-build debootstrap npm nodejs openssl
```

> **Note:** The ISO generation utilizes `live-build` and `chroot` commands, which absolutely require **root (`sudo`) privileges**.

---

## 2. Compiling the Appliance ISO

The repository contains an automated build script that handles the entire pipeline (C++ compilation, React frontend build, `.deb` packaging, and Debian ISO generation).

To build the complete project and output the ISO, run:

```bash
cd /path/to/Test_Beout_OS
./build.sh iso
```

### What this does:
1. **C++ Daemon Build**: Compiles the native REST API and Provisioning binaries.
2. **Dashboard Build**: Compiles the React SPA using Vite into `dashboard/dist`.
3. **Debian Packaging**: Bundles everything into `beout_os-core.deb` via `dpkg-deb`.
4. **OS Hardening**: Hooks `hardening/harden.sh` into the ISO build.
5. **Live Build**: Downloads Debian Minimal packages, injects our `.deb`, and wraps it into a bootable ISO.

**Output Location:** 
When finished, the bootable installer will be located at:
`installer/live-image-amd64.hybrid.iso`

---

## 3. Deployment

### Bare Metal Deployment
1. Flash the resulting `.iso` to a USB drive using `dd`, `Rufus`, or `BalenaEtcher`.
   ```bash
   sudo dd if=installer/live-image-amd64.hybrid.iso of=/dev/sdX bs=4M status=progress
   ```
2. Insert the USB into the target appliance hardware and boot from it.
3. The installer runs **completely automatically** (unattended) via the preseed configuration. It will wipe the primary hard drive, install the OS, and reboot automatically.

### Virtual Machine Deployment (VMware / Proxmox / VirtualBox)
1. Create a new Linux VM (Debian 12 / 64-bit).
2. Assign at least **2 vCPUs**, **2GB RAM**, and a **16GB Virtual Disk**.
3. Attach the generated `.iso` to the virtual CD/DVD drive.
4. Boot the VM. The installation process requires zero user interaction and will reboot automatically into the locked-down environment.

---

## 4. Initial Provisioning (CLI)

Once the appliance boots from its hard drive, you will **not** see a standard Linux login prompt. The system is hardened. 

Instead, on the physical console (`tty1`), you will be greeted immediately by the **Beout_OS Provisioning Console**.

### First Steps:
1. Press `3` to select **Configure Management Interface**.
2. Enter your desired Static IP address (e.g., `192.168.1.50`).
3. Enter your Subnet Mask (e.g., `255.255.255.0`).
4. (Optional) Configure WAN or LAN interfaces.
5. Press `8` to exit, or allow the daemon to run. The configuration is immediately saved to the secure SQLite database.

> **Security Note:** There is no SSH access. There is no Bash access. All local administration must be done through this specific Provisioning Menu.

---

## 5. Mock Licensing Server

By default, the Web UI will display the appliance as "Inactive" until it is successfully licensed against the mock server.

### Running the Licensing Server
On a separate machine (or your developer host), start the Python licensing authority:

```bash
cd licensing/
python3 server.py
```
*This will automatically generate Ed25519 cryptographic keys and an SSL certificate, and listen on port `8443`.*

### Activating the Appliance (Demo Flow)
*In a production environment, this is automated via the API, but for the demo, you can test the cryptography manually.*

1. Obtain the appliance's unique Machine ID (located at `/etc/machine-id` internally).
2. Send a POST request to your mock licensing server:
   ```bash
   curl -k -X POST https://<licensing_server_ip>:8443/api/v1/activate \
        -H "Content-Type: application/json" \
        -d '{"machine_id": "YOUR_MACHINE_ID"}'
   ```
3. The server will return a cryptographically signed Base64 token.
4. This token must be applied to the appliance via the REST API or database. *(Note: Our demo uses the `ActivationManager` C++ class which parses this token and validates the signature).*

---

## 6. Accessing the Web Dashboard

Once the management IP is configured via the Provisioning CLI, you can access the modern UI.

1. Open a web browser on your network.
2. Navigate to: `https://<MANAGEMENT_IP>:8443`
3. Accept the self-signed TLS certificate warning.
4. **Login Credentials**:
   - **Username**: `admin`
   - **Password**: `admin`

### Dashboard Capabilities:
- **System Status**: View the locked-in health metrics polling directly from the C++ daemons.
- **Interfaces**: View the current IPs set for WAN, LAN, and MGMT ports.
- **Licensing**: View the cryptographic lock status to confirm whether the appliance is activated.
