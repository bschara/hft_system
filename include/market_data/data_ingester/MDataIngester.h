#pragma once

#include "utils/LFQueue.hpp"
#include "market_data/order_book/market_update.h"
#include "utils/BMWebSocket/BMWebSocket.h"
#include <cmath>

class MDataIngester
{

public:
    explicit MDataIngester(Common::LFQueue<MarketUpdate> *mDataQueue, std::string queryString, char *path, char *protocol, utility::TLSClient &tlsClient,
                           utility::BMWebSocket &socketClient);

    ~MDataIngester();

    void startReceiving();

    void stopReceiving();

    void parseAndEnqueueUpdates(std::span<const uint8_t> payload);

    MDataIngester() = delete;

    MDataIngester(const MDataIngester &) = delete;

    MDataIngester(const MDataIngester &&) = delete;

    MDataIngester &operator=(const MDataIngester &) = delete;

    MDataIngester &operator=(const MDataIngester &&) = delete;

private:
    std::atomic<bool> running{false};
    Common::LFQueue<MarketUpdate> *updatesQueue;
    std::string queryString;
    char *path;
    char *protocol;
    utility::TLSClient &tls_client;
    utility::BMWebSocket &web_socket;
};