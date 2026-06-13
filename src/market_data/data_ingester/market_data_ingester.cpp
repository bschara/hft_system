#include "market_data/data_ingester/market_data_ingester.h"
#include "utils/benchmark/benchmark_utility.hpp"

MarketDataIngester::MarketDataIngester(Common::LFQueue<MarketUpdate> &mDataQueue,
                                       utility::TLSClient &tlsClient,
                                       utility::WebSocket &socketClient,
                                       const VenueRegistry &registry,
                                       std::string venue_name)
    : updatesQueue(mDataQueue), tls_client(tlsClient), web_socket(socketClient),
      registry_(registry), venue_name_(std::move(venue_name))
{
}

MarketDataIngester::~MarketDataIngester()
{
    running = false;
}

void MarketDataIngester::startReceiving(std::string_view path, std::string_view protocol)
{
    if (!running)
        running = true;

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

    // Binance combined-stream URLs encode all subscriptions in the path;
    // no subscription JSON frame is needed after the handshake.

    while (running)
    {
        auto frame = web_socket.read_frame();
        if (!frame)
            continue;

        if (frame->opcode == 0x2) // binary data
            parseAndEnqueueUpdates(frame->payload);
    }
}

void MarketDataIngester::parseAndEnqueueUpdates(std::span<const uint8_t> payload)
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

    // --- SBE header (8 bytes) ---
    // uint16_t blockLength = read_uint16_le(0);
    // uint16_t templateId  = read_uint16_le(2);
    // uint16_t schemaId    = read_uint16_le(4);
    // uint16_t version     = read_uint16_le(6);

    size_t offset = 8;

    int64_t eventTime = read_int64_le(offset);       // +0
    // int64_t firstBookUpdateID = read_int64_le(offset + 8);
    // int64_t lastBookUpdateID  = read_int64_le(offset + 16);

    int8_t priceExp = read_int8(offset + 24);
    int8_t qtyExp   = read_int8(offset + 25);

    double priceScale = std::pow(10.0, priceExp);
    double qtyScale   = std::pow(10.0, qtyExp);

    offset += 26;

    // --- Pre-scan: compute where the symbol field lives before entering loops ---
    uint16_t bidBlockLen = read_uint16_le(offset);
    uint16_t numBids     = read_uint16_le(offset + 2);
    size_t   bids_end    = offset + 4 + static_cast<size_t>(bidBlockLen) * numBids;

    uint16_t askBlockLen = read_uint16_le(bids_end);
    uint16_t numAsks     = read_uint16_le(bids_end + 2);
    size_t   asks_end    = bids_end + 4 + static_cast<size_t>(askBlockLen) * numAsks;

    // --- Read symbol from the trailing field ---
    uint32_t instrument_id = 0;
    if (asks_end < payload.size())
    {
        uint8_t symLen = payload[asks_end];
        if (asks_end + 1 + symLen <= payload.size())
        {
            std::string_view symbol(reinterpret_cast<const char *>(&payload[asks_end + 1]), symLen);
            try
            {
                instrument_id = registry_.lookup(venue_name_, std::string(symbol));
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Ingester] " << e.what() << "\n";
                return;
            }
        }
    }

    // Stamp ingestion time once per payload; all levels in this SBE message share it.
    uint64_t recv_tsc = rdtsc_start();

    // --- Bid loop ---
    offset += 4; // skip bidBlockLen + numBids header
    for (uint16_t t = 0; t < numBids; ++t)
    {
        if (offset + bidBlockLen > payload.size())
            break;

        double price = read_int64_le(offset)     * priceScale;
        double qty   = read_int64_le(offset + 8) * qtyScale;

        auto* slot = updatesQueue.getNextToWriteTo();
        *slot = MarketUpdate(instrument_id, Side::BUY, price, qty, eventTime);
        slot->_recv_tsc = recv_tsc;
        updatesQueue.updateWriteIndex();

        offset += bidBlockLen;
    }

    // --- Ask loop ---
    offset += 4; // skip askBlockLen + numAsks header
    for (uint16_t t = 0; t < numAsks; ++t)
    {
        if (offset + askBlockLen > payload.size())
            break;

        double price = read_int64_le(offset)     * priceScale;
        double qty   = read_int64_le(offset + 8) * qtyScale;

        auto* slot = updatesQueue.getNextToWriteTo();
        *slot = MarketUpdate(instrument_id, Side::SELL, price, qty, eventTime);
        slot->_recv_tsc = recv_tsc;
        updatesQueue.updateWriteIndex();

        offset += askBlockLen;
    }
}

void MarketDataIngester::stopReceiving()
{
    running = false;
}
