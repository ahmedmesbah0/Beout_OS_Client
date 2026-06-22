#include "machine_id.hpp"
#include <fstream>
#include <sstream>

namespace beout_os {
namespace activation {

std::string MachineId::get() {
    std::ifstream file("/etc/machine-id");
    std::string id;
    if (file.is_open()) {
        std::getline(file, id);
    }
    
    if (id.empty()) {
        // Fallback for testing environments
        return "BEOUT_OS-DEMO-MACHINE-ID-0000";
    }
    
    return id;
}

} // namespace activation
} // namespace beout_os
