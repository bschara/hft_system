#pragma once

#include <iostream>
#include <string_view>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace utility
{

    class TLSClient
    {
    public:
        TLSClient(std::string_view _host, int _port);
        ~TLSClient();

        bool connect();
        int send(const void *data, size_t length);
        int recv(void *buffer, size_t length);
        int fd();

    private:
        std::string_view host;
        int port;
        SSL *ssl;
        SSL_CTX *ssl_ctx;
        int socket_fd;
    };
}