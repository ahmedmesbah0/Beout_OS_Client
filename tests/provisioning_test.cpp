#include <gtest/gtest.h>
#include "../provisioning/cli_engine.hpp"
#include "../database/database_manager.hpp"

TEST(ProvisioningTest, IPValidation) {
    EXPECT_TRUE(beout_os::provisioning::CliEngine::validate_ip("192.168.1.1"));
    EXPECT_TRUE(beout_os::provisioning::CliEngine::validate_ip("10.0.0.1"));
    EXPECT_TRUE(beout_os::provisioning::CliEngine::validate_ip("255.255.255.255"));
    EXPECT_TRUE(beout_os::provisioning::CliEngine::validate_ip("0.0.0.0"));

    EXPECT_FALSE(beout_os::provisioning::CliEngine::validate_ip("256.1.1.1"));
    EXPECT_FALSE(beout_os::provisioning::CliEngine::validate_ip("192.168.1"));
    EXPECT_FALSE(beout_os::provisioning::CliEngine::validate_ip("192.168.1.1.1"));
    EXPECT_FALSE(beout_os::provisioning::CliEngine::validate_ip("abc.def.ghi.jkl"));
    EXPECT_FALSE(beout_os::provisioning::CliEngine::validate_ip(""));
}

TEST(DatabaseTest, SetAndGetConfig) {
    auto db = std::make_shared<beout_os::database::DatabaseManager>(":memory:");
    ASSERT_TRUE(db->initialize());

    EXPECT_TRUE(db->set_config("test_key", "test_value"));
    auto val = db->get_config("test_key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "test_value");

    EXPECT_TRUE(db->set_config("test_key", "new_value"));
    val = db->get_config("test_key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "new_value");
    
    val = db->get_config("nonexistent");
    EXPECT_FALSE(val.has_value());
}
