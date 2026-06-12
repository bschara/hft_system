#pragma once

#include <unordered_map>
#include <vector>
#include <strategies/strategy.h>
#include "utils/lock_free_queue.hpp"

class StrategyManager
{
public:
    explicit StrategyManager(Common::LFQueue<double> *signals) : signals_(signals) {}

    // Register a strategy for a specific instrument (called at startup).
    void register_strategy(uint32_t instrument_id, Strategy *strat)
    {
        per_instrument_[instrument_id].push_back(strat);
    }

    // Register a catch-all strategy that receives updates for every instrument.
    void register_strategy(Strategy *strat)
    {
        global_strategies_.push_back(strat);
    }

    void onMarketData(const MarketUpdate *marketUpdate)
    {
        auto it = per_instrument_.find(marketUpdate->_instrument_id);
        if (it != per_instrument_.end())
        {
            for (auto *strat : it->second)
            {
                double signal = strat->onMarketData(marketUpdate);
                *signals_->getNextToWriteTo() = signal;
                signals_->updateWriteIndex();
            }
        }

        for (auto *strat : global_strategies_)
        {
            double signal = strat->onMarketData(marketUpdate);
            *signals_->getNextToWriteTo() = signal;
            signals_->updateWriteIndex();
        }
    }

private:
    std::unordered_map<uint32_t, std::vector<Strategy *>> per_instrument_;
    std::vector<Strategy *>                               global_strategies_;
    Common::LFQueue<double>                              *signals_;
};
