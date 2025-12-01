#include "strategies/mean_reversion/orderbook_imbalance.h"

OrderBookImbalance::OrderBookImbalance(OrderBook &ob) : order_book(ob) {}

// double OrderBookImbalance::generate_obi(double askPrice, double askQuantity, double bidPrice, double bidQuantity)
// {
//     double part1 = askPrice * bidQuantity;
//     double part2 = bidPrice * askQuantity;
//     double part3 = bidQuantity + askQuantity;

//     double midPrice = order_book.getMidPrice();
//     double microPrice = (part1 + part2) / part3;

//     return microPrice - midPrice;
// }

double OrderBookImbalance::generate_obi(double askPrice, double askQty,
                                        double bidPrice, double bidQty)
{
    double spread = askPrice - bidPrice;
    if (spread <= 0.0)
        return 0.0;

    double part1 = askPrice * bidQty;
    double part2 = bidPrice * askQty;
    double part3 = bidQty + askQty;
    if (part3 == 0.0)
        return 0.0;

    double microPrice = (part1 + part2) / part3;
    double midPrice = order_book.getMidPrice();

    double signal = (microPrice - midPrice) / spread;
    signal = std::clamp(signal, -3.0, 3.0);
    return signal / 3.0;
}
