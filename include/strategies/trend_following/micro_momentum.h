#include "market_data/order_book/OrderBook.h"

// for crypto use window of 5-30 ticks

class MicroMomentum
{
public:
    MicroMomentum(OrderBook &ob, Common::LFQueue<MarketUpdate> &muq);

    // positive and increasing -> go long |||| negative and decreasing -> go short
    double getMomentumSignal(double numAggBids, double numAggAsks);

private:
    OrderBook &order_book;
    Common::LFQueue<MarketUpdate> &market_update_queue;
};