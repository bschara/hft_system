#include "strategies/trend_following/micro_momentum.h"

MicroMomentum::MicroMomentum(OrderBook &ob, Common::LFQueue<MarketUpdate> &muq) : order_book(ob), market_update_queue(muq) {}

double MicroMomentum::getMomentumSignal(double numAggBids, double numAggAsks)
{
    return numAggBids - numAggAsks;
}