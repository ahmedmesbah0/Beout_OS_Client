# Beout_OS Security Architecture Reference Guide

This document outlines the end-to-end security model, system hardening policies, and cryptographic validation measures implemented within Beout_OS.

---

## 1. Operating System Hardening

Beout_OS applies strict hardening policies on the underlying Debian kernel and user space to transform the operating system into a secure closed-box security appliance:

* **Console Lockout**: 
  * All standard interactive TTY login prompts (`getty`) are masked and disabled. 
  * The local terminal (`tty1`) is exclusively taken over by the interactive `beout_os_provisioning` utility. This prevents root or user shell logins on the physical/virtual console.
* **Root De-activation**: 
  * The `root` account login is disabled (`usermod -p '!' root`).
  * The root shell is pointed to `/usr/sbin/nologin` to block interactive login shells.
* **Service Disabling**:
  * SSH daemon (`sshd`) is disabled and removed from target images to block external remote shell attempts.
* **Kernel & GRUB Protections**:
  * GRUB bootloader timeout is set to `0` seconds to block boot parameter manipulation (such as booting into single-user recovery mode).
  * Enforces kernel-level audit logging (`audit=1`) and standard network interface mapping settings.
* **Protocol Blacklisting**:
  * Disables uncommon and insecure transport protocols in kernel space via module blacklisting:
    * DCCP (Datagram Congestion Control Protocol)
    * SCTP (Stream Control Transmission Protocol)
    * RDS (Reliable Datagram Sockets)
    * TIPC (Transparent Inter-Process Communication)

---

## 2. Hardware Cryptographic Licensing (Ed25519)

Appliance activation is cryptographically bound to the unique hardware fingerprint of the virtualization hypervisor/machine:

* **Hardware Fingerprinting**: 
  * The client daemon reads `/etc/machine-id` (the immutable UUID of the operating system installation).
* **Asymmetric Key Exchange**:
  * The central licensing server holds the **Ed25519 Private Key**.
  * The client VM has the **Ed25519 Public Key** burned into `/opt/beout_os/etc/license_public_key.pem`.
* **Activation Signature Handshake**:
  * The activation token issued by the server is an Ed25519 signature of the `machine_id` payload.
  * The C++ client validation engine (`ActivationManager`) parses the license config, reads `/etc/machine-id`, and verifies the signature using the embedded public key.
  * Bypassing this licensing model is cryptographically infeasible without compromising the server's private key.

---

## 3. Connection & Transport Security (VM to Server)

All client-to-server traffic is encrypted using HTTPS. To prevent Man-in-the-Middle (MitM) spoofing and interception:

* **Enforced Certificate Verification**:
  * By default, client daemon (`beout_os_api`) and auto-updater (`check_updates.sh`) enforce SSL/TLS certificate validation against the system CA trust store.
* **Certificate Pinning / Private CA Support**:
  * If a deployment uses a private/self-signed CA, the administrator can place the PEM certificate in `/opt/beout_os/etc/server_ca.pem`.
  * The HTTP client checks for this file and securely overrides the certificate verification path to trust only the specified CA.
* **Fallback Mode (Verification Toggle)**:
  * For local testing or sandboxed installations, certificate verification can be disabled by updating the SQLite configuration key `license_server_verify_ssl` to `0`. If not set or set to `1`, validation is strictly enforced.

---

## 4. Server-Side Administrative Security

The central management portal (`Beout_OS_Server`) is protected against unauthorized access and remote administration attempts:

* **Session Authentication**:
  * All `/api/admin/...` REST endpoints and dashboard routes are shielded behind PHP Session Validation (`enforceAdminAuth`). Unauthorized requests are immediately blocked with a `401 Unauthorized` response.
* **Secure Login**:
  * The administrator authenticates via a dedicated portal (`/public/login.php`) which uses Bcrypt-hashed password verification (`password_verify`) against SQLite records.
  * Default password seeding initializes securely on database generation.
* **Logout & Session Destruction**:
  * The logout endpoint terminates the session and destroys server-side data tokens.
* **SQL Injection Protections**:
  * Prepared statements are used across all licensing, updates, and settings query paths.


---

## 5. Secure Auto-Updates Verification

To prevent supply chain compromise or package tampering during auto-updates:
* **Payload Verification**:
  * The central updates repository publishes a version metadata document containing the secure **SHA256 checksum** of the release package.
* **Local Checksum Verification**:
  * The update client (`check_updates.sh`) downloads the metadata and binary release package (`.deb`).
  * Before calling the installer package manager, the script computes the SHA256 sum of the local file and compares it to the signed server checksum.
  * If a mismatch is detected, the package is immediately deleted, and the installation is aborted.
