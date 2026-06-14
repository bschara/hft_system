#include "utils/net/websocket/websocket.h"
#include "helper.hpp"
#include "utils/perf/benchmark/benchmark_utility.hpp"

namespace utility
{
    WebSocket::WebSocket(TLSClient &client_ref, const std::string &_host, const std::string &api_key_)
        : tls(client_ref), host(_host), api_key(api_key_)
    {
        std::cout << "inside websocket construct" << std::endl;
        websocket_key = helper::generate_random_base64_key();
        send_buf.reserve(MAX_FRAME_SIZE);
        recv_buf.resize(MAX_FRAME_SIZE);
    }

    WebSocket::~WebSocket() {}

    bool WebSocket::perform_handshake(std::string_view path, std::string_view protocol)
    {

        std::ostringstream req;
        req << "GET " << path << " HTTP/1.1\r\n";
        req << "Host: " << host << "\r\n";
        req << "Upgrade: websocket\r\n";
        req << "Connection: Upgrade\r\n";
        req << "Sec-WebSocket-Key: " << websocket_key << "\r\n";
        req << "Sec-WebSocket-Version: 13\r\n";
        if (!protocol.empty())
            req << "Sec-WebSocket-Protocol: " << protocol << "\r\n";
        if (!api_key.empty())
        {
            req << "X-MBX-APIKEY: " << api_key << "\r\n";
        }
        req << "\r\n";

        std::string request = req.str();
        tls.send(request.data(), request.size());

        char buf[2048];
        int n = tls.recv(buf, sizeof(buf) - 1);
        if (n <= 0)
            return false;

        buf[n] = 0;
        std::string response(buf);

        return response.find("HTTP/1.1 101") != std::string::npos &&
               response.find("Upgrade: websocket") != std::string::npos;
    }

    bool WebSocket::send_frame(std::span<const uint8_t> payload, uint8_t opcode)
    {
        send_buf.clear();

        send_buf.push_back(0x80 | (opcode & 0x0F));

        size_t len = payload.size();
        if (len <= 125)
        {
            send_buf.push_back(0x80 | static_cast<uint8_t>(len));
        }
        else if (len <= 65535)
        {
            send_buf.push_back(0x80 | 126);
            send_buf.push_back((len >> 8) & 0xFF);
            send_buf.push_back(len & 0xFF);
        }
        else
        {
            send_buf.push_back(0x80 | 127);
            for (int i = 7; i >= 0; --i)
                send_buf.push_back((len >> (8 * i)) & 0xFF);
        }

        uint8_t mask_key[4];
        RAND_bytes(mask_key, 4);
        send_buf.insert(send_buf.end(), mask_key, mask_key + 4);

        size_t start = send_buf.size();
        send_buf.resize(start + len);
        for (size_t i = 0; i < len; ++i)
            send_buf[start + i] = payload[i] ^ mask_key[i % 4];

        int sent = tls.send(send_buf.data(), send_buf.size());
        return sent == (int)send_buf.size();
    }

    bool WebSocket::send_control_frame(uint8_t opcode, std::span<const uint8_t> payload)
    {
        send_buf.clear();

        send_buf.push_back(0x80 | (opcode & 0x0F));

        size_t len = payload.size();
        if (len > 125)
            return false;

        send_buf.push_back(0x80 | static_cast<uint8_t>(len));

        uint8_t mask_key[4];
        RAND_bytes(mask_key, 4);
        send_buf.insert(send_buf.end(), mask_key, mask_key + 4);

        size_t start = send_buf.size();
        send_buf.resize(start + len);
        for (size_t i = 0; i < len; ++i)
            send_buf[start + i] = payload[i] ^ mask_key[i % 4];

        int sent = tls.send(send_buf.data(), send_buf.size());
        return sent == (int)send_buf.size();
    }

    bool WebSocket::recv_exact(uint8_t *dst, size_t len)
    {
        size_t total = 0;
        while (total < len)
        {
            int n = tls.recv(dst + total, len - total);
            if (n <= 0)
                return false;
            total += n;
        }
        return true;
    }

    std::optional<WebSocketFrame> WebSocket::read_frame()
    {
        size_t offset = 0;

        if (!recv_exact(recv_buf.data(), 2))
            return std::nullopt;

        uint8_t byte1 = recv_buf[0];
        uint8_t byte2 = recv_buf[1];

        bool fin = byte1 & 0x80;
        uint8_t opcode = byte1 & 0x0F;
        // std::cerr << "opcode " << (int)opcode << "\n";

        bool masked = byte2 & 0x80;
        // std::cerr << "masked " << masked << "\n";

        uint64_t payload_len = byte2 & 0x7F;
        offset = 2;

        // --- Step 2: Handle extended payload lengths ---
        if (payload_len == 126)
        {
            if (!recv_exact(recv_buf.data() + offset, 2))
                return std::nullopt;

            payload_len = (uint64_t(recv_buf[offset]) << 8) |
                          uint64_t(recv_buf[offset + 1]);

            offset += 2;
        }
        else if (payload_len == 127)
        {
            if (!recv_exact(recv_buf.data() + offset, 8))
                return std::nullopt;

            payload_len = 0;
            for (int i = 0; i < 8; ++i)
            {
                payload_len = (payload_len << 8) | recv_buf[offset + i];
            }

            offset += 8;
        }

        if (payload_len > MAX_FRAME_SIZE)
            return std::nullopt;

        uint8_t mask[4] = {0};
        if (masked)
        {
            if (!recv_exact(mask, 4))
                return std::nullopt;
        }

        if (payload_len > 0)
        {
            if (!recv_exact(recv_buf.data() + offset, payload_len))
                return std::nullopt;
        }

        last_io_done_tsc_ = rdtsc_end();

        uint8_t *payload_ptr = recv_buf.data() + offset;

        if (masked && payload_len > 0)
            mask_payload(payload_ptr, payload_len, mask);

        if (opcode == 0x9)
        {
            send_control_frame(0xA, std::span(payload_ptr, payload_len)); // pong
            return std::nullopt;
        }

        return WebSocketFrame{
            fin,
            opcode,
            std::span<uint8_t>(payload_ptr, payload_len)};
    }

    inline void WebSocket::mask_payload(uint8_t *data, size_t len, const uint8_t mask[4])
    {
        for (size_t i = 0; i < len; ++i)
            data[i] ^= mask[i % 4];
    }

}