#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace beout_os {
namespace crypto {

class CryptoUtils {
public:
    // Generate a random Ed25519 or RSA key pair (for testing/mocking)
    static bool generate_keypair(std::string& public_key_pem, std::string& private_key_pem);

    // Sign a payload (e.g., license token) using a PEM private key
    static std::string sign_payload(const std::string& payload, const std::string& private_key_pem);

    // Verify a payload signature using a PEM public key
    static bool verify_signature(const std::string& payload, const std::string& signature_b64, const std::string& public_key_pem);

    // Encode/Decode Base64
    static std::string base64_encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> base64_decode(const std::string& input);
};

} // namespace crypto
} // namespace beout_os
