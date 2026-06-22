# Beout_OS API & CLI Reference Guide

This document describes the CLI interactive config console and the REST APIs exposed by the client daemon.

---

## 1. CLI Interactive Config Console

On local boot, `beout_os_provisioning` binds to `/dev/tty1` and opens the interactive setup panel.

### Interface Setup Parameters:
The console prompts for network configuration parameters on the WAN and Management (MGMT) interfaces:
1. **IP Mode**: Static or DHCP.
2. **IP Address**: Designated static IPv4 (e.g. `192.168.10.10`).
3. **Netmask**: IPv4 netmask (e.g. `255.255.255.0`).
4. **Gateway**: IPv4 gateway IP (e.g. `192.168.10.1`).

*Note: In DHCP mode, gateway and netmask parameters are configured automatically.*

---

## 2. API Reference (Client Daemon)

By default, the C++ REST Daemon (`beout_os_api`) listens for local connections on port `8080` (HTTP) or `8443` (HTTPS).

### 1. GET `/api/license`
Queries the appliance activation status, machine hardware ID, and registered license key.
* **Headers**: `Authorization: Bearer <session_token>`
* **Response `200 OK`**:
  ```json
  {
    "status": "ACTIVE",
    "license_key": "BEOU-1234-ABCD-9999",
    "machine_id": "VM-UUID-8888-9999"
  }
  ```

### 2. POST `/api/license/activate`
Triggers client-side activation against the Main Server.
* **Headers**: `Authorization: Bearer <session_token>`
* **Request Body**:
  ```json
  {
    "license_key": "BEOU-1234-ABCD-9999"
  }
  ```
* **Response `200 OK`**:
  ```json
  {
    "status": "success"
  }
  ```
* **Response `400 Bad Request`**:
  ```json
  {
    "error": "Cryptographic signature verification failed"
  }
  ```

### 3. GET `/api/config`
Retrieves current system network configurations.
* **Headers**: `Authorization: Bearer <session_token>`
* **Response `200 OK`**:
  ```json
  {
    "wan_ip": "192.168.1.100",
    "wan_netmask": "255.255.255.0",
    "wan_gateway": "192.168.1.1",
    "mgmt_ip": "192.168.10.20",
    "mgmt_netmask": "255.255.255.0",
    "mgmt_gateway": "192.168.10.1"
  }
  ```

### 4. POST `/api/config`
Applies new network configuration parameters to the appliance config store.
* **Headers**: `Authorization: Bearer <session_token>`
* **Request Body**:
  ```json
  {
    "wan_ip": "192.168.1.100",
    "wan_netmask": "255.255.255.0",
    "wan_gateway": "192.168.1.1",
    "mgmt_ip": "192.168.10.20",
    "mgmt_netmask": "255.255.255.0",
    "mgmt_gateway": "192.168.10.1"
  }
  ```
* **Response `200 OK`**:
  ```json
  {
    "status": "success"
  }
  ```
