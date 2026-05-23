#include "market_data/order_book/order_book_manager.h"

OrderBookManager::OrderBookManager()
{
}

void OrderBookManager::passUpdateToOrderbook(const MarketUpdate &_update)
{
    const int64_t orderbook_index = symbol_to_index.get(_update._symbol);
    order_books[orderbook_index]->addUpdate(_update);
}

void OrderBookManager::run()
{
    if (!running)
    {
        running = true;
    }

    while (running)
    {
        const MarketUpdate *update = updates_queue.getNextToRead();
        if (update)
        {
            passUpdateToOrderbook(*update);
            updates_queue.updateReadIndex();
        }
    }
}

void OrderBookManager::stop()
{
    running = false;
}