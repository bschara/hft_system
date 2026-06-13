#include "strategies/mean_reversion/midprice_reversion.h"

MidPriceReversion::MidPriceReversion(OrderBook &ob) : order_book(ob) {};

// double MidPriceReversion::generate_reversion_signal(double last_trade_price)
// {
//     double midPrice = order_book.getMidPrice();
//     return last_trade_price - midPrice;
// }

double MidPriceReversion::generate_reversion_signal(double last_trade_price)
{
    double midPrice = order_book.getMidPrice();
    double spread = order_book.getBestAskPrice() - order_book.getBestBidPrice();
    if (spread <= 0.0)
        return 0.0;

    double raw_signal = (last_trade_price - midPrice) / spread;
    raw_signal = std::clamp(raw_signal, -3.0, 3.0);
    return raw_signal / 3.0;
}

double MidPriceReversion::onMarketData(const MarketUpdate *marketUpdate)
{
    return generate_reversion_signal(marketUpdate->_price);
}