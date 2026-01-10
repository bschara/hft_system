#include "market_data/data_ingester/MDataIngester.h"
#include <thread>
#include "utils/parsing_functions.hpp"

MDataIngester::MDataIngester(Common::LFQueue<MarketUpdate> *mDataQueue, std::string queryString, char *path, char *protocol, utility::TLSClient &tlsClient,
                             utility::BMWebSocket &socketClient) : queryString(queryString), path(path), protocol(protocol),
                                                                   tls_client(tlsClient), web_socket(socketClient)
{
    this->updatesQueue = mDataQueue;
}

MDataIngester::~MDataIngester()
{
    running = false;
}

void MDataIngester::startReceiving()
{
    if (!running)
    {
        running = true;
    }

    if (!tls_client.connect())
    {
        std::cerr << "TLS connection failed\n";
        return;
    }

    if (!web_socket.perform_handshake(path, protocol))
    {
        std::cerr << "WebSocket handshake failed\n";
        return;
    }

    if (!web_socket.send_frame(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t *>(queryString.data()),
            queryString.size())))
    {
        std::cerr << "Failed to send subscription\n";
        return;
    }

    std::cout << "Subscribed to btcusdt@trade\n";

    while (running)
    {
        auto frame = web_socket.read_frame();
        if (!frame)
            continue;

        if (frame->opcode == 0x2) // binary data
        {
            parseAndEnqueueUpdates(frame->payload);
        }
    }
}

void MDataIngester::parseAndEnqueueUpdates(std::span<const uint8_t> payload)
{

    // uint16_t blockLength = read_uint16_le(0);
    // uint16_t templateId = read_uint16_le(2);
    // uint16_t schemaId = read_uint16_le(4);
    // uint16_t version = read_uint16_le(6);
    size_t offset = 8;

    int64_t eventTime = read_int64_le(payload, offset);

    int64_t firstBookUpdateID = read_int64_le(payload, offset + 8);

    int64_t lastBookUpdateID = read_int64_le(payload, offset + 16);

    int8_t priceExp = read_int8(payload, offset + 24);

    int8_t qtyExp = read_int8(payload, offset + 25);

    offset += 26;

    uint16_t bidTradeBlockLength = read_uint16_le(payload, offset);
    uint16_t numBids = read_uint16_le(payload, offset + 2);
    offset += 4;

    for (uint8_t t = 0; t < numBids; ++t)
    {
        if (offset + bidTradeBlockLength > payload.size())
            break;

        int64_t price = read_int64_le(payload, offset);
        int64_t qty = read_int64_le(payload, offset + 8);

        *updatesQueue->getNextToWriteTo() = MarketUpdate(Side::BUY, price, qty, eventTime);
        updatesQueue->updateWriteIndex();

        offset += bidTradeBlockLength;
    }

    uint16_t askTradeBlockLength = read_uint16_le(payload, offset);
    uint16_t numAsks = read_uint16_le(payload, offset + 2);
    offset += 4;

    for (uint8_t t = 0; t < numBids; ++t)
    {
        if (offset + askTradeBlockLength > payload.size())
            break;

        int64_t price = read_int64_le(payload, offset);
        int64_t qty = read_int64_le(payload, offset + 8);

        *updatesQueue->getNextToWriteTo() = MarketUpdate(Side::SELL, price, qty, eventTime);
        updatesQueue->updateWriteIndex();

        offset += bidTradeBlockLength;
    }

    size_t tradesGroupEnd = offset; // offset points right after last trade

    if (tradesGroupEnd < payload.size())
    {
        uint8_t symbolLength = payload[tradesGroupEnd];
        if (tradesGroupEnd + 1 + symbolLength <= payload.size())
        {
            std::string_view symbol(
                reinterpret_cast<const char *>(&payload[tradesGroupEnd + 1]),
                symbolLength);
            std::cerr << "[Symbol] " << symbol << "\n";
        }
    }
}

void MDataIngester::stopReceiving()
{
    running = false;
}
