#include "machine_id.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace beout_os {
namespace activation {

std::string MachineId::generate_and_persist() {
    // Generate a UUID v4 style random machine ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dis(0, 255);

    unsigned char bytes[16];
    for (int i = 0; i < 16; ++i) {
        bytes[i] = dis(gen);
    }
    // Set UUID v4 variant bits
    bytes[6] = (bytes[6] & 0x0f) | 0x40;  // Version 4
    bytes[8] = (bytes[8] & 0x3f) | 0x80;  // Variant 1

    // Format as UUID string
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11],
        bytes[12], bytes[13], bytes[14], bytes[15]);

    std::string machine_id(uuid_str);

    // Persist to /var/lib/beout_os/machine_id
    std::string dir = "/var/lib/beout_os";
    mkdir(dir.c_str(), 0755);

    std::ofstream out(PERSISTENT_ID_PATH);
    if (out.is_open()) {
        out << machine_id;
    }

    return machine_id;
}

std::string MachineId::get() {
    // 1. Try /etc/machine-id (systemd's standard)
    std::ifstream file("/etc/machine-id");
    std::string id;
    if (file.is_open()) {
        std::getline(file, id);
        // Trim whitespace
        id.erase(0, id.find_first_not_of(" \t\r\n"));
        id.erase(id.find_last_not_of(" \t\r\n") + 1);
    }

    if (!id.empty()) {
        return id;
    }

    // 2. Try persistent Beout_OS generated ID
    std::ifstream persistent(PERSISTENT_ID_PATH);
    if (persistent.is_open()) {
        std::getline(persistent, id);
        id.erase(0, id.find_first_not_of(" \t\r\n"));
        id.erase(id.find_last_not_of(" \t\r\n") + 1);
    }

    if (!id.empty()) {
        return id;
    }

    // 3. Generate a new unique machine ID and persist it
    return generate_and_persist();
}

} // namespace activation
} // namespace beout_os
