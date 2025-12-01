#pragma once

#include <vector>
#include <strategies/strategy.h>
#include "utils/LFQueue.hpp"

constexpr int MAX_STRATEGIES = 10;

class StrategyManager
{
public:
    StrategyManager(Common::LFQueue<double> *_signals)
    {
        strategies.reserve(MAX_STRATEGIES);
        signals = _signals;
    };

    void register_strategy(Strategy *strat_ref)
    {
        strategies.push_back(strat_ref);
    };

    void onMarketData(const MarketUpdate *marketUpdate)
    {
        for (auto &strat : strategies)
        {
            double signal = strat->onMarketData(marketUpdate);
            *signals->getNextToWriteTo() = signal;
            signals->updateWriteIndex();
        }
    }

private:
    std::vector<Strategy *> strategies;
    Common::LFQueue<double> *signals;
};