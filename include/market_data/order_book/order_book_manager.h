#pragma once

#include <unordered_map>
#include <vector>
#include <cstdint>
#include "order_book.h"
#include "utils/lock_free_queue.hpp"
#include "utils/latency_tracker.hpp"

class StrategyManager;

class OrderBookManager
{
public:
    static constexpr uint64_t kReportInterval = 1'000'000;

    explicit OrderBookManager(uint32_t num_instruments);
    ~OrderBookManager() = default;

    // Call once per instrument at startup (before any threads launch).
    void register_instrument(uint32_t instrument_id);

    // Returns a stable reference; safe because the vector is never resized after reserve().
    OrderBook &book_for(uint32_t instrument_id);

    // Register a per-venue queue. Each queue must have exactly one producer (one ingester thread).
    void add_queue(Common::LFQueue<MarketUpdate> &q);

    void set_strategy_manager(StrategyManager *sm);
    void set_ns_per_cycle(double ns_per_cycle);

    void passUpdateToOrderbook(const MarketUpdate &update, uint64_t dequeue_tsc);
    void report_latencies() const;
    void run();
    void stop();

private:
    std::vector<Common::LFQueue<MarketUpdate>*> update_queues_;  // one per venue
    std::vector<OrderBook>                       order_books_;    // pre-reserved; never resized after init
    std::unordered_map<uint32_t, uint16_t>       id_to_index_;
    StrategyManager                             *strategy_manager_ = nullptr;
    std::atomic<bool>                            running_{false};

    double                   ns_per_cycle_  = 1.0;
    uint64_t                 update_count_  = 0;
    Common::LatencyHistogram hist_queue_wait_{"queue_wait"};
    Common::LatencyHistogram hist_book_update_{"book_update"};
    Common::LatencyHistogram hist_strategy_{"strategy"};
    Common::LatencyHistogram hist_obm_total_{"obm_total"};
};
