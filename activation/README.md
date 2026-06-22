# Beout_OS - Activation Client

This C++ component runs on the Beout_OS Appliance and manages the node's activation lifecycle.

## Overview
1. Obtains the hardware signature via `MachineId`.
2. Validates signed tokens using embedded Ed25519 public keys via the `crypto` module.
3. Persists the activation status inside the SQLite database via the `database` module.

## Tests
Covered under the main CTest suite:
```bash
./build.sh test
```
