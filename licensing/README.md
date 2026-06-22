# Beout_OS - Licensing Server

This contains the Mock HTTPS Licensing Server for the Beout_OS Demo.
It operates as a stand-alone Python 3 daemon.

## Purpose
Simulates a cloud-based enterprise licensing authority.
It accepts a Machine ID via a REST POST request to `/api/v1/activate`, signs it with an internal Ed25519 private key, and returns a Base64-encoded signature token.

## Usage
Run the server locally:
```bash
python3 server.py
```
This automatically generates self-signed certificates and signing key pairs.
