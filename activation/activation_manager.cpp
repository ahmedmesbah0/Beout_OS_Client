#include "activation_manager.hpp"
#include "machine_id.hpp"
#include "../crypto/crypto_utils.hpp"
#include <iostream>

namespace beout_os {
namespace activation {

ActivationManager::ActivationManager(std::shared_ptr<database::DatabaseManager> db, const std::string& public_key_pem)
    : db_(std::move(db)), public_key_pem_(public_key_pem) {}

bool ActivationManager::apply_token(const std::string& signed_token_b64) {
    // For demo purposes, the payload is expected to be the Machine ID
    std::string expected_payload = MachineId::get();

    if (crypto::CryptoUtils::verify_signature(expected_payload, signed_token_b64, public_key_pem_)) {
        db_->set_config("activation_token", signed_token_b64);
        db_->set_config("activation_status", "ACTIVE");
        return true;
    }

    return false;
}

bool ActivationManager::is_activated() const {
    auto status = db_->get_config("activation_status");
    if (!status || status.value() != "ACTIVE") {
        return false;
    }

    auto token = db_->get_config("activation_token");
    if (!token) return false;

    // Verify token matches current machine ID to prevent token reuse across machines
    return crypto::CryptoUtils::verify_signature(MachineId::get(), token.value(), public_key_pem_);
}

} // namespace activation
} // namespace beout_os
