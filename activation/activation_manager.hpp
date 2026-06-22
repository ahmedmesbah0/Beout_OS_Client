#pragma once

#include <string>
#include <memory>
#include "../database/database_manager.hpp"

namespace beout_os {
namespace activation {

class ActivationManager {
public:
    ActivationManager(std::shared_ptr<database::DatabaseManager> db, const std::string& public_key_pem);

    // Apply a token retrieved from the licensing server or manually provided
    bool apply_token(const std::string& signed_token_b64);

    // Check if the system is currently activated
    bool is_activated() const;

private:
    std::shared_ptr<database::DatabaseManager> db_;
    std::string public_key_pem_;
};

} // namespace activation
} // namespace beout_os
