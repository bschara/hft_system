#include "utils/tls_client/tls_client.h"

namespace utility
{
    TLSClient::TLSClient(std::string_view _host, int _port)
        : host(_host), port(_port), ssl(nullptr), ssl_ctx(nullptr), socket_fd(-1)
    {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    }

    TLSClient::~TLSClient()
    {
        if (ssl)
        {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }

        if (ssl_ctx)
        {
            SSL_CTX_free(ssl_ctx);
        }

        if (socket_fd != -1)
        {
            close(socket_fd);
        }

        ERR_free_strings();
        EVP_cleanup();
    }

    bool TLSClient::connect()
    {
        struct addrinfo hints{}, *res;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string port_str = std::to_string(port);
        if (getaddrinfo(std::string(host).c_str(), port_str.c_str(), &hints, &res) != 0)
        {
            std::cerr << "❌ getaddrinfo failed\n";
            return false;
        }

        socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (socket_fd < 0)
        {
            std::cerr << "❌ socket_fd() failed\n";
            freeaddrinfo(res);
            return false;
        }

        if (::connect(socket_fd, res->ai_addr, res->ai_addrlen) < 0)
        {
            std::cerr << "❌ connect() failed\n";
            close(socket_fd);
            socket_fd = -1;
            freeaddrinfo(res);
            return false;
        }

        freeaddrinfo(res);

        ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx)
        {
            std::cerr << "❌ Failed to create SSL_CTX\n";
            return false;
        }

        // Create SSL object
        ssl = SSL_new(ssl_ctx);
        if (!ssl)
        {
            std::cerr << "❌ Failed to create SSL object\n";
            return false;
        }

        SSL_set_fd(ssl, socket_fd);

        SSL_set_tlsext_host_name(ssl, std::string(host).c_str());

        if (SSL_connect(ssl) <= 0)
        {
            std::cerr << "❌ TLS handshake failed\n";
            ERR_print_errors_fp(stderr);
            return false;
        }

        std::cout << "✅ TLS handshake completed with " << host << "\n";
        return true;
    }

    int TLSClient::send(const void *data, size_t length)
    {
        return SSL_write(ssl, data, length);
    }

    int TLSClient::recv(void *buffer, size_t length)
    {
        return SSL_read(ssl, buffer, length);
    }

    int TLSClient::fd()
    {
        return socket_fd;
    }
}