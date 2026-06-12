#pragma once

#include "utils/lock_free_queue.hpp"
#include "market_data/order_book/market_update.h"
#include "utils/websocket/websocket.h"
#include "market_data/venue_registry.hpp"
#include <cmath>
#include <string>
#include <string_view>

class MarketDataIngester
{

public:
    explicit MarketDataIngester(Common::LFQueue<MarketUpdate> &mDataQueue, utility::TLSClient &tlsClient,
                                utility::WebSocket &socketClient,
                                const VenueRegistry &registry, std::string venue_name);

    ~MarketDataIngester();

    void startReceiving(std::string_view path, std::string_view protocol);

    void stopReceiving();

    void parseAndEnqueueUpdates(std::span<const uint8_t> payload);

    std::string_view buildQueryString(std::string_view file_path);

    MarketDataIngester() = delete;

    MarketDataIngester(const MarketDataIngester &) = delete;

    MarketDataIngester(const MarketDataIngester &&) = delete;

    MarketDataIngester &operator=(const MarketDataIngester &) = delete;

    MarketDataIngester &operator=(const MarketDataIngester &&) = delete;

private:
    std::atomic<bool> running{false};
    Common::LFQueue<MarketUpdate> &updatesQueue;
    utility::TLSClient &tls_client;
    utility::WebSocket &web_socket;
    const VenueRegistry &registry_;
    std::string venue_name_;
};