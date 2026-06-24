#pragma once

#include <string>

namespace beout_os {
namespace activation {

class MachineId {
public:
    // Retrieve the unique machine ID, generating one if it doesn't exist
    static std::string get();

private:
    // Generate a new machine ID and persist it to disk
    static std::string generate_and_persist();

    // Persistent storage path for generated machine ID
    static constexpr const char* PERSISTENT_ID_PATH = "/var/lib/beout_os/machine_id";
};

} // namespace activation
} // namespace beout_os
