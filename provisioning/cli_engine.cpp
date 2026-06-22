#include "cli_engine.hpp"
#include <iostream>
#include <regex>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace beout_os {
namespace provisioning {

CliEngine::CliEngine(std::shared_ptr<database::DatabaseManager> db)
: db_(std::move(db)) {}

void CliEngine::print_menu() {
    std::cout << "\n============================================\n"
              << "      BEOUT_OS PROVISIONING CONSOLE         \n"
              << "============================================\n"
              << "1. Configure WAN Interface\n"
              << "2. Configure LAN Interface\n"
              << "3. Configure Management Interface\n"
              << "4. View Current Configuration\n"
              << "5. Factory Reset\n"
              << "6. Reboot System\n"
              << "7. Shutdown System\n"
              << "8. Exit Provisioning\n"
              << "============================================\n"
              << "Select an option: ";
}

bool CliEngine::validate_ip(const std::string& ip) {
    const std::regex ip_regex(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );
    return std::regex_match(ip, ip_regex);
}

std::vector<std::string> get_available_interfaces() {
    std::vector<std::string> ifaces;
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net")) {
            std::string name = entry.path().filename().string();
            if (name != "lo") {
                ifaces.push_back(name);
            }
        }
    } catch (...) {
        // Fallback
    }
    return ifaces;
}

void CliEngine::configure_interface(const std::string& iface) {
    std::cout << "\nConfiguring " << iface << " interface\n";

    auto ifaces = get_available_interfaces();
    std::string selected_iface = "";
    if (ifaces.empty()) {
        std::cout << "No network interfaces detected! Enter interface name manually (e.g., eth0): ";
        std::cin >> selected_iface;
    } else {
        std::cout << "Available network interfaces:\n";
        for (size_t i = 0; i < ifaces.size(); ++i) {
            std::cout << "  " << i + 1 << ". " << ifaces[i] << "\n";
        }
        std::cout << "Select interface (1-" << ifaces.size() << "): ";
        int choice;
        if (std::cin >> choice && choice >= 1 && choice <= static_cast<int>(ifaces.size())) {
            selected_iface = ifaces[choice - 1];
        } else {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid choice! Enter interface name manually: ";
            std::cin >> selected_iface;
        }
    }

    std::string ip;
    std::cout << "Enter IP address (e.g., 192.168.1.1): ";
    std::cin >> ip;

    if (!validate_ip(ip)) {
        std::cout << "Invalid IP address format!\n";
        return;
    }

    std::string netmask;
    std::cout << "Enter Netmask (e.g., 255.255.255.0): ";
    std::cin >> netmask;

    if (!validate_ip(netmask)) {
        std::cout << "Invalid Netmask format!\n";
        return;
    }

    std::string gateway = "";
    if (iface == "WAN" || iface == "MGMT") {
        std::cout << "Enter Gateway (e.g., 192.168.1.254, press Enter to skip): ";
        std::cin.ignore(10000, '\n');
        std::getline(std::cin, gateway);
        if (gateway == "none" || gateway.empty()) {
            gateway = "";
        } else if (!validate_ip(gateway)) {
            std::cout << "Invalid Gateway format! Skipping gateway.\n";
            gateway = "";
        }
    }

    db_->set_config("network_" + iface + "_interface", selected_iface);
    db_->set_config("network_" + iface + "_ip", ip);
    db_->set_config("network_" + iface + "_netmask", netmask);
    if (iface == "WAN" || iface == "MGMT") {
        db_->set_config("network_" + iface + "_gateway", gateway);
    }
    
    std::cout << iface << " configured successfully on " << selected_iface << ".\n";
}

void CliEngine::factory_reset() {
    std::cout << "\nWARNING: This will erase all configuration. Continue? (y/N): ";
    std::string confirm;
    std::cin >> confirm;
    if (confirm == "y" || confirm == "Y") {
        std::cout << "Erasing configuration database...\n";
        db_->set_config("network_WAN_interface", "");
        db_->set_config("network_WAN_ip", "");
        db_->set_config("network_WAN_netmask", "");
        db_->set_config("network_WAN_gateway", "");
        db_->set_config("network_LAN_interface", "");
        db_->set_config("network_LAN_ip", "");
        db_->set_config("network_LAN_netmask", "");
        db_->set_config("network_MGMT_interface", "");
        db_->set_config("network_MGMT_ip", "");
        db_->set_config("network_MGMT_netmask", "");
        db_->set_config("network_MGMT_gateway", "");
        std::cout << "Factory reset complete. Please reboot.\n";
    }
}

void CliEngine::reboot() {
    std::cout << "Rebooting system...\n";
    std::system("reboot");
}

void CliEngine::shutdown() {
    std::cout << "Shutting down system...\n";
    std::system("shutdown -h now");
}

void CliEngine::run() {
    bool running = true;
    while (running) {
        print_menu();
        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }

        switch (choice) {
            case 1: configure_interface("WAN"); break;
            case 2: configure_interface("LAN"); break;
            case 3: configure_interface("MGMT"); break;
            case 4: {
                std::cout << "\n--- Current Configuration ---\n";
                std::cout << "WAN: " << db_->get_config("network_WAN_interface").value_or("Unassigned")
                          << " (IP: " << db_->get_config("network_WAN_ip").value_or("Unconfigured")
                          << ", Netmask: " << db_->get_config("network_WAN_netmask").value_or("Unconfigured")
                          << ", Gateway: " << db_->get_config("network_WAN_gateway").value_or("None") << ")\n";
                std::cout << "LAN: " << db_->get_config("network_LAN_interface").value_or("Unassigned")
                          << " (IP: " << db_->get_config("network_LAN_ip").value_or("Unconfigured")
                          << ", Netmask: " << db_->get_config("network_LAN_netmask").value_or("Unconfigured") << ")\n";
                std::cout << "MGMT: " << db_->get_config("network_MGMT_interface").value_or("Unassigned")
                          << " (IP: " << db_->get_config("network_MGMT_ip").value_or("Unconfigured")
                          << ", Netmask: " << db_->get_config("network_MGMT_netmask").value_or("Unconfigured")
                          << ", Gateway: " << db_->get_config("network_MGMT_gateway").value_or("None") << ")\n";
                break;
            }
            case 5: factory_reset(); break;
            case 6: reboot(); running = false; break;
            case 7: shutdown(); running = false; break;
            case 8: running = false; break;
            default: std::cout << "Invalid option. Please try again.\n"; break;
        }
    }
}

} // namespace provisioning
} // namespace beout_os
