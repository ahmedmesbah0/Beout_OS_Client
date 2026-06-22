#include "api_server.hpp"
#include <iostream>

int main() {
    auto db = std::make_shared<beout_os::database::DatabaseManager>("/var/lib/beout_os/config.db");
    if (!db->initialize()) {
        std::cerr << "Failed to initialize database. Exiting." << std::endl;
        return 1;
    }

    // In a real environment, certs should be generated or provisioned
    // Using demo certs (assuming they are in the working directory)
    beout_os::api::ApiServer server("server.crt", "server.key", db);
    
    server.start("0.0.0.0", 8443);
    
    return 0;
}
