#include <vector>
#include <stdint.h>
#include <optional>
#include "utils/tls_client/tls_client.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <span>
#include <string_view>

namespace utility
{

    struct WebSocketFrame
    {
        bool fin;
        uint8_t opcode;
        std::span<uint8_t> payload;
    };

    class WebSocket
    {
    public:
        static constexpr size_t MAX_FRAME_SIZE = 8192;

        WebSocket(TLSClient &client_ref, const std::string &_host, const std::string &api_key_);
        ~WebSocket();

        bool perform_handshake(std::string_view path, std::string_view protocol);
        bool send_frame(std::span<const uint8_t> payload, uint8_t opcode = 0x1);
        bool send_control_frame(uint8_t opcode, std::span<const uint8_t> payload);
        std::optional<WebSocketFrame> read_frame();
        void mask_payload(uint8_t *data, size_t len, const uint8_t mask[4]);
        bool recv_exact(uint8_t *dst, size_t len);

    private:
        TLSClient &tls;
        std::string host;
        std::string api_key;
        std::string websocket_key;
        std::vector<uint8_t> send_buf;
        std::vector<uint8_t> recv_buf;
    };
}