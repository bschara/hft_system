#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace hash_helpers
{

    static EVP_PKEY *loadPrivateKeyFromString(const std::string &pem_key)
    {
        BIO *bio = BIO_new_mem_buf(pem_key.data(), static_cast<int>(pem_key.size()));
        if (!bio)
            throw std::runtime_error("Failed to create BIO from string");

        EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!pkey)
            throw std::runtime_error("Failed to parse private key from string");
        return pkey;
    }

    static std::string base64Encode(const unsigned char *input, size_t length)
    {
        BIO *bio, *b64;
        BUF_MEM *bufferPtr;

        b64 = BIO_new(BIO_f_base64());
        // No newlines
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);

        BIO_write(bio, input, length);
        BIO_flush(bio);
        BIO_get_mem_ptr(bio, &bufferPtr);

        std::string result(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);
        return result;
    }

    static std::string logonRawData(EVP_PKEY *pkey,
                             const std::string &sender,
                             const std::string &target,
                             const std::string &seqNum,
                             const std::string &time)
    {

        // Build payload with ASCII 0x01 separator
        std::string payload = "A" + std::string(1, '\x01') + sender + '\x01' +
                              target + '\x01' + seqNum + '\x01' + time;

        EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
        if (!md_ctx)
            throw std::runtime_error("Failed to create digest context");

        std::vector<unsigned char> signature(64); // Ed25519 sig is 64 bytes
        size_t siglen = signature.size();

        if (EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, pkey) != 1)
            throw std::runtime_error("DigestSignInit failed");

        if (EVP_DigestSign(md_ctx,
                           signature.data(),
                           &siglen,
                           reinterpret_cast<const unsigned char *>(payload.data()),
                           payload.size()) != 1)
            throw std::runtime_error("DigestSign failed");

        EVP_MD_CTX_free(md_ctx);
        return base64Encode(signature.data(), siglen);
    }

};
