# Beout_OS Security Appliance Demo

This repository contains the full enterprise demonstration architecture for **Beout_OS**, a premium, hardware-locked, API-driven network security appliance operating system (comparable to FortiGate or Palo Alto PAN-OS). 

Built from scratch using modern C++20 daemons and a React-based frontend on a highly locked-down Debian Linux core.

## Project Workspace Layout

* **`Beout_OS_Client/`**: Main client codebase.
  * **`api/`**: Native C++20 HTTPS REST API server using `cpp-httplib` and `OpenSSL` to expose configuration endpoints.
  * **`dashboard/`**: A fully responsive React/TypeScript SPA featuring glassmorphism and a dark mode aesthetic, served natively by the C++ backend.
  * **`provisioning/`**: A secure, interactive CLI network configuration application that takes over `tty1` to prevent unauthorized shell access.
  * **`database/`**: Persistent SQLite3 configuration engine orchestrating safe parameter storage across client modules.
  * **`crypto/` & `activation/`**: Cryptographic modules wrapping OpenSSL EVP interfaces, implementing Ed25519 signature-based hardware locks.
  * **`packaging/`**: Auto-updater background scripts (`check_updates.sh`), systemd timer definitions, and Debian package configurations.
  * **`installer/`**: Automated `live-build` configurations to compile a self-installing hybrid operating system ISO.
* **`Beout_OS_Server/`**: Isolated central management authority.
  * **`public/`**: Web root serving the glassmorphism admin dashboard and handling REST client heartbeats and activation pings.
  * **`src/`**: PHP business logic (SQLite PDO database interfaces, OpenSSL Ed25519 signing routines, licensing managers, and release publishers).

## Detailed Documentation Guides

Refer to the following guides inside the `Beout_OS_Client/docs/` folder:
* [Architecture Guide](file:///home/mesba7/Documents/GitHub/Test_Beout_OS/Beout_OS_Client/docs/ARCHITECTURE.md): System layout, network layers, and software interaction models.
* [Developer Guide & Git Strategy](file:///home/mesba7/Documents/GitHub/Test_Beout_OS/Beout_OS_Client/docs/DEVELOPER.md): Branch strategies, codebase standards, and extension paths.
* [Build & Installer Guide](file:///home/mesba7/Documents/GitHub/Test_Beout_OS/Beout_OS_Client/docs/BUILD_INSTALL.md): Compilation walkthroughs, live-build ISO requirements, and deployment instructions.
* [API & CLI Documentation](file:///home/mesba7/Documents/GitHub/Test_Beout_OS/Beout_OS_Client/docs/API_CLI.md): Exhaustive HTTP endpoints reference and setup console walkthrough.
* [Security & Threat Model Audit](file:///home/mesba7/Documents/GitHub/Test_Beout_OS/Beout_OS_Client/docs/SECURITY_AUDIT.md): STRIDE model threat mitigations, filesystem checks, and attack surfaces.
* [Roadmap & Release Notes](file:///home/mesba7/Documents/GitHub/Test_Beout_OS/Beout_OS_Client/docs/ROADMAP_RELEASES.md): Version history, feature milestones, and next release versions.

## Building the Client Ecosystem

The client ecosystem is orchestrated via the central `build.sh` pipeline which triggers CMake, NPM, and Debian packaging utilities.

**To execute the full build and unit-test pipeline locally (Requires CMake, OpenSSL, NPM):**
```bash
./build.sh clean
./build.sh configure
./build.sh build
./build.sh test
```

**To generate the final bootable ISO (Requires Debian `live-build` and `sudo`):**
```bash
./build.sh iso
```

