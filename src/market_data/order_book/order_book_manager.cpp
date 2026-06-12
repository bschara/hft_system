#include "market_data/order_book/order_book_manager.h"
#include "strategies/strategy_manager.hpp"

OrderBookManager::OrderBookManager(Common::LFQueue<MarketUpdate> &queue, uint32_t num_instruments)
    : updates_queue_(queue)
{
    order_books_.reserve(num_instruments);
}

void OrderBookManager::register_instrument(uint32_t instrument_id)
{
    id_to_index_[instrument_id] = static_cast<uint16_t>(order_books_.size());
    order_books_.emplace_back();
}

OrderBook &OrderBookManager::book_for(uint32_t instrument_id)
{
    return order_books_[id_to_index_.at(instrument_id)];
}

void OrderBookManager::set_strategy_manager(StrategyManager *sm)
{
    strategy_manager_ = sm;
}

void OrderBookManager::passUpdateToOrderbook(const MarketUpdate &update)
{
    auto it = id_to_index_.find(update._instrument_id);
    if (__builtin_expect(it == id_to_index_.end(), 0))
        return;
    order_books_[it->second].addUpdate(update);
    if (strategy_manager_)
        strategy_manager_->onMarketData(&update);
}

void OrderBookManager::run()
{
    running_ = true;
    while (running_)
    {
        const MarketUpdate *update = updates_queue_.getNextToRead();
        if (update)
        {
            passUpdateToOrderbook(*update);
            updates_queue_.updateReadIndex();
        }
    }
}

void OrderBookManager::stop()
{
    running_ = false;
}
