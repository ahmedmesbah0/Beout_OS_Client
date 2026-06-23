#!/bin/bash
set -e

# Script to package Beout_OS Core into a .deb file

BASE_DIR="$(pwd)/packaging/debian"

echo "Building Beout_OS C++ components inside a Debian Bookworm Docker container to ensure GLIBC compatibility..."
docker run --rm -v "$(pwd)":/workspace -w /workspace debian:bookworm-slim sh -c "
    apt-get update -qq &&
    apt-get install -y -qq build-essential cmake libssl-dev libsqlite3-dev &&
    ./build.sh clean &&
    ./build.sh configure &&
    ./build.sh build
"

echo "Building Dashboard..."
cd dashboard
npm install
npm run build
cd ..

echo "Copying binaries..."
cp build/api/beout_os_api $BASE_DIR/opt/beout_os/bin/
cp build/provisioning/beout_os_provisioning $BASE_DIR/opt/beout_os/bin/
cp packaging/sync_network.sh $BASE_DIR/opt/beout_os/bin/
chmod +x $BASE_DIR/opt/beout_os/bin/sync_network.sh
cp packaging/check_updates.sh $BASE_DIR/opt/beout_os/bin/
chmod +x $BASE_DIR/opt/beout_os/bin/check_updates.sh


echo "Copying Dashboard UI..."
rm -rf $BASE_DIR/opt/beout_os/dashboard/dist
mkdir -p $BASE_DIR/opt/beout_os/dashboard
cp -r dashboard/dist $BASE_DIR/opt/beout_os/dashboard/

echo "Setting permissions..."
chmod 755 $BASE_DIR/DEBIAN/preinst
chmod 755 $BASE_DIR/DEBIAN/postinst
chmod 755 $BASE_DIR/opt/beout_os/bin/beout_os_api
chmod 755 $BASE_DIR/opt/beout_os/bin/beout_os_provisioning

echo "Building .deb package..."
dpkg-deb --build $BASE_DIR beout_os-core.deb

echo "Packaging complete: beout_os-core.deb generated."
