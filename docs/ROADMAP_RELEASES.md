# Beout_OS Product Roadmap & Release Notes

This document highlights the milestones, release history, and feature development roadmap for Beout_OS.

---

## 1. Product Development Roadmap

* **Phase 1: Basic Architecture & Installer (Completed)**
  * Debian Minimal Live-build environment setup.
  * Automated installation hooks and hard drive partition overlays.
* **Phase 2: Local Interface Console (Completed)**
  * Secure `tty1` CLI provisioning console.
  * Validation parameters for static IP settings.
* **Phase 3: Database & C++ REST API Server (Completed)**
  * SQLite config DB implementation.
  * `cpp-httplib` HTTPS rest API service.
  * Glassmorphism client web interface.
* **Phase 4: Cryptographic Licensing Lock (Completed)**
  * OpenSSL EVP Ed25519 client-side verification engine.
  * Unique Machine ID extraction logic.
* **Phase 5: Central Licensing & Update Server (Completed)**
  * Decoupled PHP-based server engine (`Beout_OS_Server`).
  * Admin dashboard key generator, bulk importer, and updater manager.
  * Client heartbeat polling and auto-deactivation capabilities.
* **Phase 6: Advanced Network Controls (In Progress / Next)**
  * Multi-interface LAN/WAN bridge grouping.
  * Policy-based routing, traffic profiling, and DPI engine.
* **Phase 7: Cloud Threat Intelligence (Future)**
  * Live feed integration with Horus central threat feeds.

---

## 2. Release Notes: Version 1.0.0

### New Features:
* **Decoupled Architecture**: Split the workspace into `Beout_OS_Client/` and `Beout_OS_Server/` to isolate server and client responsibilities.
* **PHP Central Management Engine**: Replaced Python server mocking tools with a robust PHP implementation supporting prepared PDO SQLite schemas and OpenSSL Ed25519 signing.
* **Network Gateway Routing**: Extended CLI provisioning, REST endpoints, UI dashboards, and system network scripts to configure default gateways and netmasks.
* **Licensing Locks**: Integrated client activation endpoints and background heartbeat pings. VMs verify license statuses every 5 seconds and self-deactivate on server revocation.
* **Debian Installer Service**: Bundled client resources into a unified `.deb` package (`beout_os-core`) and configured custom systemd timers.
