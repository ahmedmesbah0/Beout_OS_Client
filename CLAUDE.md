# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Common Development Tasks

- **Configure/build**: `cmake -B build && cmake --build build`
- **Run tests**: `ctest` or `make test`
- **Run a single test**: `ctest -R <test_name>`
- **Lint**: `clang-tidy src/**/*.cpp && cppcheck src/`
- **Format**: `clang-format -i src/**/*.cpp include/**/*.hpp`
- **Generate ISO**: `./build.sh iso` (placeholder – will invoke live-build later)
- **Launch demo VM**: `qemu-system-x86_64 -cdrom build/beout_os-demo.iso -m 2048 -enable-kvm`

## High‑Level Architecture

```
beout_os/
├─ installer/        # Debian live-build configuration & ISO generation
├─ boot/             # Bootloader (GRUB) and kernel images
├─ provisioning/     # Provisioning CLI source code
├─ appliance-cli/    # Entry point for configuration commands
├─ activation/       # License activation workflow
├─ licensing/        # Mock HTTPS licensing server (Python)
├─ config-engine/    # Generates system config from SQLite DB
├─ database/         # SQLite schema and access layer
├─ api/              # HTTPS REST API (C++/OpenSSL)
├─ dashboard/        # React + TypeScript web UI
├─ crypto/           # Cryptographic helpers (OpenSSL wrappers)
├─ security/         # Security‑related utilities
├─ logging/          # Centralized logging framework
├─ common/           # Shared C++ utilities & helpers
├─ include/          # Public headers for library components
├─ tests/            # GoogleTest unit & integration tests
├─ docs/             # Architecture, build, developer guides
├─ scripts/          # Helper scripts (build.sh, packaging, etc.)
├─ packaging/        # Packaging metadata (deb, ISO rules)
├─ systemd/          # Systemd unit files for services
├─ cmake/            # Component‑level CMake modules
└─ tools/            # External tools, third‑party scripts
```

Each top‑level component is deliberately isolated behind clean C++ interfaces (or a language‑appropriate API) so that future features—Suricata, VPN, HA, AI, etc.—can be added without redesign.

## Documentation
- `docs/README.md` – Overview of the project and quick‑start guide.
- `docs/architecture.md` – Detailed description of the subsystem interactions.
- `docs/build.md` – How to build the ISO, run tests, and generate the VM image.
- `docs/developer.md` – Coding standards, linting, and contribution workflow.
- `docs/security.md` – Threat model, hardening checklist, and audit notes.

## Cursor / Copilot Rules
*If a `.cursor/` or `.github/copilot-instructions.md` file exists, include the relevant rules here.*

## Verification
1. Run `./build.sh` – should configure CMake, compile all placeholder targets, and produce a `build/` directory.
2. Run `make test` – executes the example GoogleTest and should pass.
3. Verify `./build.sh iso` creates a mock ISO (placeholder).
4. Start the VM with the QEMU command and ensure the web UI is reachable at `https://localhost:8443` (once the full implementation is added).

---
*End of CLAUDE.md*