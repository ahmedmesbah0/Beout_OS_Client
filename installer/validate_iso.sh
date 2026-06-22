#!/usr/bin/env bash
# =============================================================
#  BEOUT_OS — ISO Build Pre-Flight Validator
# =============================================================
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}=============================================${NC}"
echo -e "${CYAN}  Beout_OS Installer Pre-Flight Validator    ${NC}"
echo -e "${CYAN}=============================================${NC}"
echo ""

errors=0
warnings=0

# Helper function to report check status
report_status() {
    local name="$1"
    local status="$2"
    local details="${3:-}"
    if [ "$status" = "OK" ]; then
        echo -e "  [${GREEN} PASS ${NC}] $name"
    elif [ "$status" = "WARN" ]; then
        echo -e "  [${YELLOW} WARN ${NC}] $name - $details"
        warnings=$((warnings + 1))
    else
        echo -e "  [${RED} FAIL ${NC}] $name - $details"
        errors=$((errors + 1))
    fi
}

# 1. Check Installer Script Syntax
if [ -f "installer/beout_installer.sh" ]; then
    if bash -n installer/beout_installer.sh; then
        report_status "beout_installer.sh syntax validation" "OK"
    else
        report_status "beout_installer.sh syntax validation" "FAIL" "Syntax errors found in script."
    fi
else
    report_status "beout_installer.sh existence" "FAIL" "installer/beout_installer.sh not found."
fi

# 2. Check Hardening Script Syntax
if [ -f "hardening/harden.sh" ]; then
    if bash -n hardening/harden.sh; then
        report_status "harden.sh syntax validation" "OK"
    else
        report_status "harden.sh syntax validation" "FAIL" "Syntax errors found in script."
    fi
else
    report_status "harden.sh existence" "FAIL" "hardening/harden.sh not found."
fi

# 3. Verify Live Build config files
if [ -f "installer/config/includes.binary/install/preseed.cfg" ]; then
    report_status "preseed.cfg existence" "OK"
else
    report_status "preseed.cfg existence" "FAIL" "preseed.cfg is missing."
fi

if [ -f "installer/config/hooks/01-force-autoinstall.hook.binary" ]; then
    if [ -x "installer/config/hooks/01-force-autoinstall.hook.binary" ]; then
        report_status "force-autoinstall hook" "OK"
    else
        report_status "force-autoinstall hook" "WARN" "Hook exists but is not executable."
    fi
else
    report_status "force-autoinstall hook" "FAIL" "Hook is missing."
fi

# 4. Check core package list validation
if [ -f "installer/config/package-lists/core.list" ]; then
    report_status "core.list package config existence" "OK"
    
    # Read packages
    mapfile -t packages < <(grep -v '^#' installer/config/package-lists/core.list | grep -v '^[[:space:]]*$' || true)
    
    # If apt-cache is available, verify all packages exist
    if command -v apt-cache >/dev/null 2>&1; then
        echo "  [INFO] Checking package availability against apt cache..."
        missing_pkgs=()
        for pkg in "${packages[@]}"; do
            # Clean package name in case of suffixes (like :amd64)
            clean_pkg=$(echo "$pkg" | cut -d: -f1)
            if ! apt-cache show "$clean_pkg" >/dev/null 2>&1; then
                missing_pkgs+=("$pkg")
            fi
        done
        
        if [ ${#missing_pkgs[@]} -eq 0 ]; then
            report_status "All core.list packages exist in repositories" "OK"
        else
            report_status "Packages existence in apt repositories" "FAIL" "Missing packages: ${missing_pkgs[*]}"
        fi
    else
        report_status "APT package availability lookup" "WARN" "apt-cache command not available on this host. Skipping validation."
    fi
else
    report_status "core.list existence" "FAIL" "installer/config/package-lists/core.list not found."
fi

# 5. Check if .deb package build files are configured
if [ -f "packaging/build_deb.sh" ]; then
    if bash -n packaging/build_deb.sh; then
        report_status "build_deb.sh syntax validation" "OK"
    else
        report_status "build_deb.sh syntax" "FAIL" "Syntax errors in packaging script."
    fi
else
    report_status "build_deb.sh existence" "FAIL" "packaging/build_deb.sh not found."
fi

# Summarize results
echo ""
echo -e "${CYAN}=============================================${NC}"
if [ $errors -eq 0 ]; then
    if [ $warnings -eq 0 ]; then
        echo -e "${GREEN}  PRE-FLIGHT VALIDATION PASSED SUCCESSFULLY!${NC}"
    else
        echo -e "${YELLOW}  PRE-FLIGHT VALIDATION PASSED WITH WARNINGS (${warnings})${NC}"
    fi
    echo -e "${CYAN}=============================================${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}  PRE-FLIGHT VALIDATION FAILED WITH ${errors} ERRORS!${NC}"
    echo -e "  Please correct the errors before building the ISO."
    echo -e "${CYAN}=============================================${NC}"
    echo ""
    exit 1
fi
