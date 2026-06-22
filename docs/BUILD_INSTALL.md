# Beout_OS Build & Installer Guide

This document contains step-by-step instructions for compiling client components, bundling the Debian package, and building the installation ISO.

---

## 1. Build Guide (Prerequisites & Process)

### System Requirements:
* OS: Debian 12 / Ubuntu 22.04 LTS (amd64)
* Packages: `cmake`, `g++`, `libssl-dev`, `sqlite3`, `libsqlite3-dev`, `nodejs`, `npm`, `live-build`

### Step-by-Step Compilation:

#### 1. Build the C++ Backend & Run Tests:
Run the orchestrator script to compile the targets:
```bash
./build.sh clean
./build.sh configure
./build.sh build
./build.sh test
```
The binaries are created inside the `build/` folder.

#### 2. Bundle the React Dashboard:
Compile the TypeScript files and bundle the React SPA into static assets using Vite:
```bash
cd dashboard
npm install
npm run build
```
The bundled files will reside in `dashboard/dist/`.

#### 3. Assemble the Debian Package:
Create the final `.deb` package containing the binaries, React files, network configuration script, auto-updater daemon timer, and post-installation scripts:
```bash
./packaging/build_deb.sh
```
This outputs `beout_os-core.deb` in the client directory.

---

## 2. Installer Guide (ISO Generation)

The installer utilizes Debian `live-build` to compile a bootable operating system ISO.

### ISO Generation Steps:
Run the build script with `iso` parameter under `sudo`:
```bash
sudo ./build.sh iso
```
The script:
1. Cleans old cache data.
2. Injects the custom Debian preseed (`preseed.cfg`) into the installer environment.
3. Packages the custom kernel boot configurations and AppArmor profile.
4. Drops `beout_os-core.deb` into the system overlay for post-installation setup.
5. Emits the final bootable hybrid installation ISO file.

### Installation Process:
1. Burn the ISO to a USB flash drive or attach it to a virtual machine (CD-ROM).
2. Start the system. The bootloader immediately triggers the automated install hook (`01-force-autoinstall.hook.binary`).
3. The installer automatically:
   * Erases all data and partitions the target storage disk.
   * Installs the minimal base Debian OS.
   * Deploys `beout_os-core.deb`, installing the REST C++ daemon, React dashboard, local CLI interface, and systemd units.
   * Lockdowns shell environments, disables standard login consoles, and reboots into the custom Beout_OS environment.
