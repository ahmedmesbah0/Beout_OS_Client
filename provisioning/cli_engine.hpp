#pragma once

#include "database_manager.hpp"
#include <memory>
#include <string>

namespace beout_os {
namespace provisioning {

class CliEngine {
public:
    CliEngine(std::shared_ptr<database::DatabaseManager> db);
    
    void run();
    static bool validate_ip(const std::string& ip);

private:
    std::shared_ptr<database::DatabaseManager> db_;

    void print_menu();
    void configure_interface(const std::string& iface);
    void factory_reset();
    void reboot();
    void shutdown();
};

} // namespace provisioning
} // namespace beout_os
