#include "market_data/order_book/order_book_manager.h"
#include "strategies/strategy_manager.hpp"
#include <cstdio>

OrderBookManager::OrderBookManager(uint32_t num_instruments)
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

void OrderBookManager::add_queue(Common::LFQueue<MarketUpdate> &q)
{
    update_queues_.push_back(&q);
}

void OrderBookManager::set_strategy_manager(StrategyManager *sm)
{
    strategy_manager_ = sm;
}

void OrderBookManager::set_ns_per_cycle(double ns_per_cycle)
{
    ns_per_cycle_ = ns_per_cycle;
}

void OrderBookManager::passUpdateToOrderbook(const MarketUpdate &update, uint64_t dequeue_tsc)
{
    auto it = id_to_index_.find(update._instrument_id);
    if (__builtin_expect(it == id_to_index_.end(), 0))
        return;

    uint64_t t0 = rdtsc_start();
    order_books_[it->second].addUpdate(update);
    uint64_t t1 = rdtsc_end();
    hist_book_update_.record(t1 - t0);

    uint64_t t3 = t1;
    if (strategy_manager_)
    {
        uint64_t t2 = rdtsc_start();
        strategy_manager_->onMarketData(&update);
        t3 = rdtsc_end();
        hist_strategy_.record(t3 - t2);
    }

    if (update._recv_tsc != 0)
        hist_queue_wait_.record(dequeue_tsc - update._recv_tsc);
    hist_obm_total_.record(t3 - dequeue_tsc);
}

void OrderBookManager::report_latencies() const
{
    std::printf("--- Latency report (%lu updates) ---\n", (unsigned long)update_count_);
    hist_queue_wait_.report(ns_per_cycle_);
    hist_book_update_.report(ns_per_cycle_);
    hist_strategy_.report(ns_per_cycle_);
    hist_obm_total_.report(ns_per_cycle_);
    std::fflush(stdout);
}

void OrderBookManager::run()
{
    running_ = true;
    while (running_)
    {
        for (auto *q : update_queues_)
        {
            const MarketUpdate *update = q->getNextToRead();
            if (update)
            {
                uint64_t dequeue_tsc = rdtsc_start();
                passUpdateToOrderbook(*update, dequeue_tsc);
                q->updateReadIndex();
                if (++update_count_ % kReportInterval == 0)
                    report_latencies();
            }
        }
    }
}

void OrderBookManager::stop()
{
    running_ = false;
}
