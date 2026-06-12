#pragma once

#include <unordered_map>
#include <vector>
#include <cstdint>
#include "order_book.h"
#include "utils/lock_free_queue.hpp"

class StrategyManager;

class OrderBookManager
{
public:
    OrderBookManager(Common::LFQueue<MarketUpdate> &queue, uint32_t num_instruments);
    ~OrderBookManager() = default;

    // Call once per instrument at startup (before any threads launch).
    void register_instrument(uint32_t instrument_id);

    // Returns a stable reference; safe because the vector is never resized after reserve().
    OrderBook &book_for(uint32_t instrument_id);

    void set_strategy_manager(StrategyManager *sm);

    void passUpdateToOrderbook(const MarketUpdate &update);
    void run();
    void stop();

private:
    Common::LFQueue<MarketUpdate>         &updates_queue_;
    std::vector<OrderBook>                 order_books_;   // pre-reserved; never resized after init
    std::unordered_map<uint32_t, uint16_t> id_to_index_;
    StrategyManager                       *strategy_manager_ = nullptr;
    bool                                   running_{false};
};
