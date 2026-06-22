# Beout_OS - REST API Server

This module provides the core management interface over HTTPS using the `cpp-httplib` and `nlohmann_json` libraries.

## Architecture
The API server acts as the primary configuration gateway, designed to bind to the Management IP over port `8443`.
It handles:
1. **Authentication**: Provides session tokens (stateless simple tokens for demo) upon a successful login sequence.
2. **Configuration (`/api/config`)**: Fetches network variables securely from the SQLite database.
3. **License Verification (`/api/license`)**: Queries the internal licensing lock state natively without shelling out.
4. **Health Checking (`/api/health`)**: Exposes basic node vitality without requiring authentication.

## Documentation
The full specification is documented using OpenAPI 3.0 standards. You can find the YAML schema at:
`docs/openapi.yaml`

## Running
Once compiled, it runs natively in the background:
```bash
./build/api/beout_os_api
```
*(Requires `server.crt` and `server.key` in the execution directory for SSL/TLS termination.)*
