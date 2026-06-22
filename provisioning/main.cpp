#include "cli_engine.hpp"
#include <iostream>

int main() {
    auto db = std::make_shared<beout_os::database::DatabaseManager>("/var/lib/beout_os/config.db");
    if (!db->initialize()) {
        std::cerr << "Failed to initialize database. Falling back to memory DB." << std::endl;
        db = std::make_shared<beout_os::database::DatabaseManager>(":memory:");
        if (!db->initialize()) {
            std::cerr << "Failed to initialize memory DB. Exiting." << std::endl;
            return 1;
        }
    }

    beout_os::provisioning::CliEngine engine(db);
    engine.run();

    return 0;
}
