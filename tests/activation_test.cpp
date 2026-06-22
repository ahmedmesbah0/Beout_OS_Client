#include <gtest/gtest.h>
#include "../activation/activation_manager.hpp"
#include "../activation/machine_id.hpp"
#include "../crypto/crypto_utils.hpp"
#include "../database/database_manager.hpp"

using namespace beout_os::activation;
using namespace beout_os::crypto;

TEST(ActivationTest, ApplyAndVerifyToken) {
    auto db = std::make_shared<beout_os::database::DatabaseManager>(":memory:");
    db->initialize();

    std::string pub_key, priv_key;
    CryptoUtils::generate_keypair(pub_key, priv_key);

    ActivationManager manager(db, pub_key);

    EXPECT_FALSE(manager.is_activated());

    std::string machine_id = MachineId::get();
    std::string valid_token = CryptoUtils::sign_payload(machine_id, priv_key);

    EXPECT_TRUE(manager.apply_token(valid_token));
    EXPECT_TRUE(manager.is_activated());

    std::string invalid_token = CryptoUtils::sign_payload("wrong-machine-id", priv_key);
    EXPECT_FALSE(manager.apply_token(invalid_token));
}
