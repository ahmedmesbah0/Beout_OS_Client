#include "crypto_utils.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <memory>
#include <stdexcept>
#include <iostream>

namespace beout_os {
namespace crypto {

struct BIO_deleter { void operator()(BIO* b) const { BIO_free_all(b); } };
struct EVP_PKEY_deleter { void operator()(EVP_PKEY* p) const { EVP_PKEY_free(p); } };
struct EVP_MD_CTX_deleter { void operator()(EVP_MD_CTX* c) const { EVP_MD_CTX_free(c); } };

std::string CryptoUtils::base64_encode(const std::vector<uint8_t>& data) {
    std::unique_ptr<BIO, BIO_deleter> b64(BIO_new(BIO_f_base64()));
    std::unique_ptr<BIO, BIO_deleter> bmem(BIO_new(BIO_s_mem()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    b64.reset(BIO_push(b64.release(), bmem.release())); // bmem is now owned by b64 chain

    BIO_write(b64.get(), data.data(), data.size());
    BIO_flush(b64.get());

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64.get(), &bptr);

    return std::string(bptr->data, bptr->length);
}

std::vector<uint8_t> CryptoUtils::base64_decode(const std::string& input) {
    std::unique_ptr<BIO, BIO_deleter> b64(BIO_new(BIO_f_base64()));
    std::unique_ptr<BIO, BIO_deleter> bmem(BIO_new_mem_buf(input.c_str(), input.length()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    b64.reset(BIO_push(b64.release(), bmem.release()));

    std::vector<uint8_t> output(input.length()); // Output will be smaller than input
    int decoded_len = BIO_read(b64.get(), output.data(), output.size());
    if (decoded_len >= 0) {
        output.resize(decoded_len);
    } else {
        output.clear();
    }
    return output;
}

bool CryptoUtils::generate_keypair(std::string& public_key_pem, std::string& private_key_pem) {
    std::unique_ptr<EVP_PKEY_CTX, void(*)(EVP_PKEY_CTX*)> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr), EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0) return false;

    EVP_PKEY* pkey_raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &pkey_raw) <= 0) return false;
    std::unique_ptr<EVP_PKEY, EVP_PKEY_deleter> pkey(pkey_raw);

    // Write private key
    std::unique_ptr<BIO, BIO_deleter> priv_bio(BIO_new(BIO_s_mem()));
    PEM_write_bio_PrivateKey(priv_bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr);
    BUF_MEM* priv_ptr;
    BIO_get_mem_ptr(priv_bio.get(), &priv_ptr);
    private_key_pem = std::string(priv_ptr->data, priv_ptr->length);

    // Write public key
    std::unique_ptr<BIO, BIO_deleter> pub_bio(BIO_new(BIO_s_mem()));
    PEM_write_bio_PUBKEY(pub_bio.get(), pkey.get());
    BUF_MEM* pub_ptr;
    BIO_get_mem_ptr(pub_bio.get(), &pub_ptr);
    public_key_pem = std::string(pub_ptr->data, pub_ptr->length);

    return true;
}

std::string CryptoUtils::sign_payload(const std::string& payload, const std::string& private_key_pem) {
    std::unique_ptr<BIO, BIO_deleter> priv_bio(BIO_new_mem_buf(private_key_pem.c_str(), -1));
    std::unique_ptr<EVP_PKEY, EVP_PKEY_deleter> priv_key(PEM_read_bio_PrivateKey(priv_bio.get(), nullptr, nullptr, nullptr));
    if (!priv_key) return "";

    std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_deleter> md_ctx(EVP_MD_CTX_new());
    if (EVP_DigestSignInit(md_ctx.get(), nullptr, nullptr, nullptr, priv_key.get()) <= 0) return "";

    size_t sig_len = 0;
    if (EVP_DigestSign(md_ctx.get(), nullptr, &sig_len, reinterpret_cast<const unsigned char*>(payload.c_str()), payload.length()) <= 0) return "";

    std::vector<uint8_t> signature(sig_len);
    if (EVP_DigestSign(md_ctx.get(), signature.data(), &sig_len, reinterpret_cast<const unsigned char*>(payload.c_str()), payload.length()) <= 0) return "";
    signature.resize(sig_len);

    return base64_encode(signature);
}

bool CryptoUtils::verify_signature(const std::string& payload, const std::string& signature_b64, const std::string& public_key_pem) {
    std::vector<uint8_t> signature = base64_decode(signature_b64);
    if (signature.empty() && !signature_b64.empty()) return false;

    std::unique_ptr<BIO, BIO_deleter> pub_bio(BIO_new_mem_buf(public_key_pem.c_str(), -1));
    std::unique_ptr<EVP_PKEY, EVP_PKEY_deleter> pub_key(PEM_read_bio_PUBKEY(pub_bio.get(), nullptr, nullptr, nullptr));
    if (!pub_key) return false;

    std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_deleter> md_ctx(EVP_MD_CTX_new());
    if (EVP_DigestVerifyInit(md_ctx.get(), nullptr, nullptr, nullptr, pub_key.get()) <= 0) return false;

    int ret = EVP_DigestVerify(md_ctx.get(), signature.data(), signature.size(), reinterpret_cast<const unsigned char*>(payload.c_str()), payload.length());
    return ret == 1;
}

} // namespace crypto
} // namespace beout_os
