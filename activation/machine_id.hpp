#pragma once

#include <string>

namespace beout_os {
namespace activation {

class MachineId {
public:
    // Generate or retrieve the unique machine ID
    static std::string get();
};

} // namespace activation
} // namespace beout_os
