#include "market_data/order_book/order_book.h"
#include <iomanip>

OrderBook::OrderBook()
{
}

OrderBook::~OrderBook()
{
    bestAskIndex = bestBidIndex = 0;
};