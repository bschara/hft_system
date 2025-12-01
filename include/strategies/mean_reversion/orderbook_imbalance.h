#include "market_data/order_book/OrderBook.h"

class OrderBookImbalance
{
public:
    OrderBookImbalance(OrderBook &ob);

    // if positive sell ||| negative buy
    double generate_obi(double askPrice, double askQuantity, double bidPrice, double bidQuantity);

private:
    OrderBook &order_book;
};