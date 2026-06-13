#include "strategies/mean_reversion/orderbook_imbalance.h"
#include <algorithm>

OrderBookImbalance::OrderBookImbalance(OrderBook &ob) : order_book(ob) {}

int32_t OrderBookImbalance::onMarketData(const MarketUpdate * /*update*/)
{
    int64_t askPrice = order_book.getBestAskRaw();
    int64_t askQty   = order_book.getBestAskQtyRaw();
    int64_t bidPrice = order_book.getBestBidRaw();
    int64_t bidQty   = order_book.getBestBidQtyRaw();

    int64_t denom = bidQty + askQty;
    if (denom == 0) return 0;

    // __int128 prevents overflow on price × quantity products
    __int128 micro128  = (__int128)askPrice * bidQty + (__int128)bidPrice * askQty;
    int64_t  microPrice = static_cast<int64_t>(micro128 / denom);

    int64_t midPrice = order_book.getMidRaw();
    int64_t spread   = askPrice - bidPrice;
    if (spread <= 0) return 0;

    return static_cast<int32_t>(
        std::clamp((microPrice - midPrice) * 1000 / (3 * spread),
                   (int64_t)-1000, (int64_t)1000));
}
