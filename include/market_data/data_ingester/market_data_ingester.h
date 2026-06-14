#pragma once

#include <atomic>
#include <string_view>
#include "utils/containers/lock_free_queue.hpp"
#include "market_data/order_book/market_update.h"
#include "utils/net/websocket/websocket.h"
#include "market_data/data_ingester/venue_parser.h"
#include "utils/perf/latency_tracker.hpp"

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
    void set_ns_per_cycle(double ns) noexcept { ns_per_cycle_ = ns; }
    void report_ws_latencies() const;

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

    double   ns_per_cycle_   = 1.0;
    uint64_t ws_frame_count_ = 0;
    static constexpr uint64_t kWsReportInterval = 10'000;

    Common::LatencyHistogram hist_ws_total_{"ws_frame_tot"};
    Common::LatencyHistogram hist_ws_io_   {"ws_io"};
    Common::LatencyHistogram hist_ws_cpu_  {"ws_cpu"};
    Common::LatencyHistogram hist_sbe_     {"sbe_decode"};
};
