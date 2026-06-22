#include <gtest/gtest.h>
#include "../crypto/crypto_utils.hpp"

using namespace beout_os::crypto;

TEST(CryptoTest, GenerateAndVerifyEd25519) {
    std::string pub_key, priv_key;
    ASSERT_TRUE(CryptoUtils::generate_keypair(pub_key, priv_key));
    ASSERT_FALSE(pub_key.empty());
    ASSERT_FALSE(priv_key.empty());

    std::string payload = "BEOUT_OS-DEMO-MACHINE-ID-1234";
    std::string signature = CryptoUtils::sign_payload(payload, priv_key);
    ASSERT_FALSE(signature.empty());

    EXPECT_TRUE(CryptoUtils::verify_signature(payload, signature, pub_key));
    EXPECT_FALSE(CryptoUtils::verify_signature("wrong-payload", signature, pub_key));
}

TEST(CryptoTest, Base64Encoding) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    std::string encoded = CryptoUtils::base64_encode(data);
    EXPECT_EQ(encoded, "AQIDBA==");

    std::vector<uint8_t> decoded = CryptoUtils::base64_decode(encoded);
    EXPECT_EQ(decoded, data);
}
