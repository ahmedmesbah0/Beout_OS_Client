# Beout_OS - Packaging

This module builds the `beout_os-core.deb` package, enabling standard deployment on Debian 12 Minimal.

## Package Contents
1. `/opt/beout_os/bin/beout_os_api`: The native C++ HTTPS web server and core API backend.
2. `/opt/beout_os/bin/beout_os_provisioning`: The provisioning CLI for `tty1`.
3. `/opt/beout_os/dashboard/dist/`: The React-based SPA management dashboard.
4. `/lib/systemd/system/beout_os-api.service`: Background API service.
5. `/lib/systemd/system/beout_os-provisioning.service`: Interactive setup bound strictly to `/dev/tty1`.

## Build
Execute the packaging script from the repository root:
```bash
./packaging/build_deb.sh
```
The output will be generated as `beout_os-core.deb`.
