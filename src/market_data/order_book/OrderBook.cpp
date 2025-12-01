#include "market_data/order_book/OrderBook.h"
#include <iomanip>

OrderBook::OrderBook(Common::LFQueue<MarketUpdate> *mdq){
    this->marketDataQueue = mdq;                                                        
};

OrderBook::~OrderBook()
{
    bestAskIndex = bestBidIndex = 0;
};