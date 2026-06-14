#include "strategies/mean_reversion/midprice_reversion.h"
#include <algorithm>

MidPriceReversion::MidPriceReversion(OrderBook &ob) : order_book(ob) {}

int32_t MidPriceReversion::onMarketData(const MarketUpdate *update)
{
    int64_t midPrice = order_book.getMidRaw();
    int64_t bestAsk  = order_book.getBestAskRaw();
    int64_t bestBid  = order_book.getBestBidRaw();
    int64_t spread   = bestAsk - bestBid;
    if (spread <= 0) return 0;

    // Signal = (price - mid) * 1000 / (3 * spread), clamped to [-1000, 1000]
    int64_t deviation = update->_price - midPrice;
    return static_cast<int32_t>(std::clamp(deviation * 1000 / (3 * spread),
                                           (int64_t)-1000, (int64_t)1000));
}
