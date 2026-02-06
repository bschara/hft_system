#pragma once

#include <string>
#include <openssl/ssl.h>
#include <openssl/rand.h>

namespace helper
{
    std::string generate_random_base64_key()
    {
        unsigned char rand_bytes[16];
        RAND_bytes(rand_bytes, sizeof(rand_bytes));

        BIO *b64 = BIO_new(BIO_f_base64());
        BIO *mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(b64, rand_bytes, sizeof(rand_bytes));
        BIO_flush(b64);

        BUF_MEM *bptr;
        BIO_get_mem_ptr(b64, &bptr);

        std::string result(bptr->data, bptr->length);
        BIO_free_all(b64);
        return result;
    }

    // --- Helper: Compute Sec-WebSocket-Accept ---
    std::string compute_accept_key(const std::string &key)
    {
        const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = key + magic;

        unsigned char sha1_result[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char *>(combined.c_str()), combined.size(), sha1_result);

        BIO *b64 = BIO_new(BIO_f_base64());
        BIO *mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(b64, sha1_result, SHA_DIGEST_LENGTH);
        BIO_flush(b64);

        BUF_MEM *bptr;
        BIO_get_mem_ptr(b64, &bptr);
        std::string accept(bptr->data, bptr->length);
        BIO_free_all(b64);

        return accept;
    }

}