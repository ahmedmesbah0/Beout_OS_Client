#!/bin/bash
# Beout_OS Auto-Updater & Heartbeat Client Script
set -e

DB_PATH="/var/lib/beout_os/config.db"

# Helper to run sqlite3 as the beout_api user
db_query() {
    sudo -u beout_api sqlite3 -cmd ".timeout 5000" "$DB_PATH" "$1"
}

# Helper to get config from sqlite
get_config() {
    db_query "SELECT value FROM config WHERE key='$1';" 2>/dev/null || echo ""
}

# 1. Get local system details
MACHINE_ID="BEOUT_OS-DEMO-MACHINE-ID-0000"
if [ -f /etc/machine-id ]; then
    MACHINE_ID=$(cat /etc/machine-id | tr -d ' \n\r')
fi

LICENSE_KEY=$(get_config "activation_license_key")

CURRENT_VERSION="1.0.0"
if [ -f /etc/beout_os_version ]; then
    CURRENT_VERSION=$(cat /etc/beout_os_version | tr -d ' \n\r')
fi

# 2. Determine main server URL
SERVER_URL=$(get_config "license_server_url")
if [ -z "$SERVER_URL" ]; then
    SERVER_URL="https://update.beout.ai"
fi

# Connection Security: Configure Curl to verify SSL/TLS certificates by default
VERIFY_SSL=$(get_config "license_server_verify_ssl")
CURL_OPTS=""
if [ "$VERIFY_SSL" != "0" ]; then
    if [ -f /opt/beout_os/etc/server_ca.pem ]; then
        CURL_OPTS="--cacert /opt/beout_os/etc/server_ca.pem"
    else
        CURL_OPTS=""
    fi
else
    CURL_OPTS="-k"
fi

# 3. Heartbeat Check-in and License Verification
if [ -n "$LICENSE_KEY" ]; then
    echo "Sending heartbeat check-in to main licensing server ($SERVER_URL)..."
    LOCAL_IP=$(hostname -I | awk '{print $1}')
    
    # Perform HTTP POST heartbeat request
    HB_RESPONSE=$(curl -s $CURL_OPTS -X POST -H "Content-Type: application/json" \
        -d "{\"machine_id\":\"$MACHINE_ID\",\"license_key\":\"$LICENSE_KEY\",\"machine_ip\":\"$LOCAL_IP\",\"os_version\":\"$CURRENT_VERSION\"}" \
        "$SERVER_URL/api/license/heartbeat" || echo "")
        
    if command -v jq >/dev/null 2>&1; then
        HB_STATUS=$(echo "$HB_RESPONSE" | jq -r '.status // empty' 2>/dev/null || echo "")
    else
        HB_STATUS=$(echo "$HB_RESPONSE" | grep -o '"status": "[^"]*' | grep -o '[^"]*$' || echo "")
    fi
    
    if [ "$HB_STATUS" = "REVOKED" ] || [ "$HB_STATUS" = "INACTIVE" ]; then
        echo "WARNING: License status has been marked as $HB_STATUS by the server. Deactivating appliance."
        db_query "INSERT OR REPLACE INTO config (key, value) VALUES ('activation_status', 'INACTIVE');" >/dev/null 2>&1 || true
        db_query "INSERT OR REPLACE INTO config (key, value) VALUES ('activation_token', '');" >/dev/null 2>&1 || true
        exit 0
    fi
fi

# 4. Check for software updates
UPDATE_URL="$SERVER_URL/api/updates/latest"
echo "Checking for updates at $UPDATE_URL..."

# Fetch latest.json
TEMP_JSON=$(mktemp)
if ! curl -s $CURL_OPTS -f -L -o "$TEMP_JSON" "$UPDATE_URL"; then
    echo "Error: Failed to fetch update metadata from $UPDATE_URL"
    rm -f "$TEMP_JSON"
    exit 1
fi

# Parse version, url and checksum from JSON
if command -v jq >/dev/null 2>&1; then
    LATEST_VERSION=$(jq -r '.version // empty' "$TEMP_JSON" 2>/dev/null || echo "")
    DEB_URL=$(jq -r '.url // empty' "$TEMP_JSON" 2>/dev/null || echo "")
    CHECKSUM=$(jq -r '.checksum // empty' "$TEMP_JSON" 2>/dev/null || echo "")
else
    # Fallback to grep if jq is not available
    LATEST_VERSION=$(grep -o '"version": "[^"]*' "$TEMP_JSON" | grep -o '[^"]*$' || echo "")
    DEB_URL=$(grep -o '"url": "[^"]*' "$TEMP_JSON" | grep -o '[^"]*$' || echo "")
    CHECKSUM=$(grep -o '"checksum": "[^"]*' "$TEMP_JSON" | grep -o '[^"]*$' || echo "")
fi

rm -f "$TEMP_JSON"

if [ -z "$LATEST_VERSION" ] || [ -z "$DEB_URL" ]; then
    echo "Error: Invalid update metadata JSON format."
    exit 1
fi

echo "Latest available version: $LATEST_VERSION"

# Helper to compare version numbers
version_gt() {
    [ "$1" = "$(echo -e "$1\n$2" | sort -V | tail -n1)" ] && [ "$1" != "$2" ]
}

if [ "$LATEST_VERSION" != "$CURRENT_VERSION" ]; then
    echo "Target version $LATEST_VERSION is different from current version $CURRENT_VERSION. Syncing..."
    
    TEMP_DEB=$(mktemp -t beout_os-XXXXXX.deb)
    if ! curl -s $CURL_OPTS -f -L -o "$TEMP_DEB" "$DEB_URL"; then
        echo "Error: Failed to download update from $DEB_URL"
        rm -f "$TEMP_DEB"
        exit 1
    fi
    
    # Checksum validation if provided
    if [ -n "$CHECKSUM" ]; then
        echo "Verifying checksum..."
        DOWNLOAD_SUM=$(sha256sum "$TEMP_DEB" | awk '{print $1}')
        if [ "$DOWNLOAD_SUM" != "$CHECKSUM" ]; then
            echo "Error: Checksum validation failed! Expected: $CHECKSUM, got: $DOWNLOAD_SUM"
            rm -f "$TEMP_DEB"
            exit 1
        fi
        echo "Checksum verified."
    fi
    
    echo "Installing update package..."
    export DEBIAN_FRONTEND=noninteractive
    if dpkg --force-downgrade -i "$TEMP_DEB"; then
        echo "Update installed successfully to version $LATEST_VERSION."
        echo "$LATEST_VERSION" > /etc/beout_os_version
    else
        echo "Error: Failed to install deb package."
        rm -f "$TEMP_DEB"
        exit 1
    fi
    
    rm -f "$TEMP_DEB"
else
    echo "System is up to date (Version $CURRENT_VERSION)."
fi
