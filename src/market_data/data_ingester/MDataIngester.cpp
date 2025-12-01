#include "market_data/data_ingester/MDataIngester.h"
#include <thread>
#include "utils/benchmark/benchmark_utility.hpp"

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
        uint64_t start = rdtsc_start();
        auto frame = web_socket.read_frame();
        uint64_t end = rdtsc_end();
        std::cout << "CPU cycles: " << (end - start) << std::endl;
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
    auto read_int64_le = [&](size_t offset) -> int64_t
    {
        if (offset + sizeof(int64_t) > payload.size())
            return 0;
        int64_t value;
        std::memcpy(&value, &payload[offset], sizeof(int64_t));
        return value;
    };

    auto read_uint16_le = [&](size_t offset) -> uint16_t
    {
        if (offset + sizeof(uint16_t) > payload.size())
            return 0;
        uint16_t value;
        std::memcpy(&value, &payload[offset], sizeof(uint16_t));
        return value;
    };

    auto read_int8 = [&](size_t offset) -> int8_t
    {
        if (offset >= payload.size())
            return 0;
        int8_t value;
        std::memcpy(&value, &payload[offset], sizeof(int8_t));
        return value;
    };

    auto read_uint8 = [&](size_t offset) -> uint8_t
    {
        if (offset >= payload.size())
            return 0;
        uint8_t value;
        std::memcpy(&value, &payload[offset], sizeof(uint8_t));
        return value;
    };
    auto read_bool = [&](size_t offset) -> bool
    {
        if (offset >= payload.size())
            return false;
        return payload[offset] != 0;
    };

    uint16_t blockLength = read_uint16_le(0);
    uint16_t templateId = read_uint16_le(2);
    uint16_t schemaId = read_uint16_le(4);
    uint16_t version = read_uint16_le(6);

    size_t offset = 8;

    int64_t eventTime = read_int64_le(offset);

    int64_t firstBookUpdateID = read_int64_le(offset + 8);

    int64_t lastBookUpdateID = read_int64_le(offset + 16);

    int8_t priceExp = read_int8(offset + 24);

    int8_t qtyExp = read_int8(offset + 25);

    double priceScale = std::pow(10.0, priceExp);
    double qtyScale = std::pow(10.0, qtyExp);

    offset += 26;

    uint16_t bidTradeBlockLength = read_uint16_le(offset);
    uint16_t numBids = read_uint16_le(offset + 2);
    offset += 4;

    for (uint8_t t = 0; t < numBids; ++t)
    {
        if (offset + bidTradeBlockLength > payload.size())
            break;

        int64_t priceRaw = read_int64_le(offset);
        // std::cerr << "[BID] price " << priceRaw * priceScale << std::endl;
        int64_t qtyRaw = read_int64_le(offset + 8);
        // std::cerr << "[BID] quantity " << qtyRaw * qtyScale << std::endl;

        double price = priceRaw * priceScale;
        double qty = qtyRaw * qtyScale;

        *updatesQueue->getNextToWriteTo() = MarketUpdate(Side::BUY, price, qty, eventTime);
        updatesQueue->updateWriteIndex();

        offset += bidTradeBlockLength;
    }

    uint16_t askTradeBlockLength = read_uint16_le(offset);
    uint16_t numAsks = read_uint16_le(offset + 2);
    offset += 4;

    for (uint8_t t = 0; t < numBids; ++t)
    {
        if (offset + askTradeBlockLength > payload.size())
            break;

        int64_t priceRaw = read_int64_le(offset);
        // std::cerr << "[ASK] price " << priceRaw * priceScale << std::endl;
        int64_t qtyRaw = read_int64_le(offset + 8);
        // std::cerr << "[ASK] quantity " << qtyRaw * qtyScale << std::endl;

        int64_t price = priceRaw * priceScale;
        int64_t qty = qtyRaw * qtyScale;

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
