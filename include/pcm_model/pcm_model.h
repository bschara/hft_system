#pragma once

#include "utils/lock_free_queue.hpp"
#include "strategies/strategy.h"
#include "tcm_model/tcm.h"
#include "risk_management/risk_model.h"
#include <unordered_map>
#include "market_data/order_book/order_book.h"

constexpr double capital_fraction = 0.02;

class PCModel
{
public:
    PCModel(double _capital, Common::LFQueue<double> *_signals, OrderBook &_order_book, TransactionCostModel &_tcm,
            RiskModel &_risk_model);

    TradeIntent generatetradeIntent(double signal);

    void run();

private:
    double initialCapital;
    double availableCapital;
    Common::LFQueue<double> &signals;
    TransactionCostModel &tcm;
    RiskModel &risk_model;
    std::unordered_map<double, double> active_orders;
    OrderBook &order_book;
    bool running = false;
};