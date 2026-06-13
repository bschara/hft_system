#pragma once

#include <atomic>
#include <string_view>
#include "utils/lock_free_queue.hpp"
#include "market_data/order_book/market_update.h"
#include "utils/websocket/websocket.h"
#include "market_data/data_ingester/venue_parser.h"

class MarketDataIngester
{
public:
    MarketDataIngester(Common::LFQueue<MarketUpdate> &mDataQueue,
                       utility::TLSClient &tlsClient,
                       utility::WebSocket &socketClient,
                       VenueParser &parser);

    ~MarketDataIngester();

    void startReceiving(std::string_view path, std::string_view protocol);
    void stopReceiving();

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
    VenueParser &parser_;
};
