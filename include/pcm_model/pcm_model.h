#pragma once

#include "utils/containers/lock_free_queue.hpp"
#include "strategies/strategy.h"
#include "tcm_model/tcm.h"
#include "risk_management/risk_model.h"
#include <unordered_map>
#include "market_data/order_book/order_book.h"

constexpr double capital_fraction = 0.02;

class PCModel
{
public:
    PCModel(double _capital, Common::LFQueue<int32_t> *_signals, OrderBook &_order_book,
            TransactionCostModel &_tcm, RiskModel &_risk_model);

    TradeIntent generatetradeIntent(int32_t signal);

    void run();

private:
    double initialCapital;
    double availableCapital;
    Common::LFQueue<int32_t> &signals;
    TransactionCostModel &tcm;
    RiskModel &risk_model;
    std::unordered_map<int32_t, double> active_orders;
    OrderBook &order_book;
    bool running = false;
};
