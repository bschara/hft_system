#pragma once

#include <array>
#include <stdint.h>
#include <cmath>
#include <iomanip>
#include "market_update.h"
#include "utils/lock_free_queue.hpp"

constexpr int LOB_DEPTH = 256;
constexpr double TICK_SIZE = 0.01;
constexpr double QUANTITY_STEP_SIZE = 0.0001;
constexpr int MID_PRICE_N = 5;
constexpr int MAX_SHIFT_STEP = LOB_DEPTH / 4;

struct alignas(32) PriceLevel
{
    double _price = 0;
    double _totalQuantity = 0;
    bool _isActive = false;

    PriceLevel() = default;

    PriceLevel(double price, double quantity) : _price(price), _totalQuantity(quantity) {
                                                };
};

class OrderBook
{

public:
    OrderBook();

    ~OrderBook();

    int priceToIndex(double price, bool is_bid) const
    {
        if (is_bid)
        {
            int tickDiff = int(round((price - bids[bestBidIndex]._price) / TICK_SIZE));
            int index = (bestBidIndex + tickDiff + LOB_DEPTH) % LOB_DEPTH;
            return abs(index);
        }

        else
        {
            int tickDiff = int(round((asks[bestAskIndex]._price - price) / TICK_SIZE));
            int index = (bestAskIndex + tickDiff + LOB_DEPTH) % LOB_DEPTH;
            return abs(index);
        }

        return -1;
    }

    void shiftBookToPrice(int steps, bool is_bid, bool shift_left)
    {
        if (!is_bid) // ASK side
        {
            if (shift_left) // Shift toward lower prices (left)
            {
                for (int s = 0; s < steps; ++s)
                {
                    int from = (bestAskIndex - s + LOB_DEPTH) % LOB_DEPTH;
                    int to = (from - 1 + LOB_DEPTH) % LOB_DEPTH;
                    asks[to] = asks[from];
                    asks[from]._isActive = false;
                }
                bestAskIndex = (bestAskIndex - steps + LOB_DEPTH) % LOB_DEPTH;
            }
            else // Shift toward higher prices (right)
            {
                for (int s = 0; s < steps; ++s)
                {
                    int from = (worstAskIndex + s + LOB_DEPTH) % LOB_DEPTH;
                    int to = (from + 1) % LOB_DEPTH;
                    asks[to] = asks[from];
                    asks[from]._isActive = false;
                }
                worstAskIndex = (worstAskIndex + steps) % LOB_DEPTH;
            }
        }
        else // BID side
        {
            if (shift_left) // Shift toward lower prices (left)
            {
                for (int s = 0; s < steps; ++s)
                {
                    int from = (bestBidIndex - s + LOB_DEPTH) % LOB_DEPTH;
                    int to = (from - 1 + LOB_DEPTH) % LOB_DEPTH;
                    bids[to] = bids[from];
                    bids[from]._isActive = false;
                }
                bestBidIndex = (bestBidIndex - steps + LOB_DEPTH) % LOB_DEPTH;
            }
            else // Shift toward higher prices (right)
            {
                for (int s = 0; s < steps; ++s)
                {
                    int from = (worstBidIndex + s + LOB_DEPTH) % LOB_DEPTH;
                    int to = (from + 1) % LOB_DEPTH;
                    bids[to] = bids[from];
                    bids[from]._isActive = false;
                }
                worstBidIndex = (worstBidIndex + steps) % LOB_DEPTH;
            }
        }
    }

    double getMidPrice() noexcept
    {
        double bidsSum = 0.0, bidsVolume = 0.0;
        double asksSum = 0.0, asksVolume = 0.0;

        int idx = bestBidIndex;
        int levels = 0;
        while (levels < MID_PRICE_N)
        {
            if (bids[idx]._isActive)
            {
                bidsSum += bids[idx]._price * bids[idx]._totalQuantity;
                bidsVolume += bids[idx]._totalQuantity;
                ++levels;
            }
            idx = (idx - 1 + LOB_DEPTH) % LOB_DEPTH;
        }

        idx = bestAskIndex;
        levels = 0;
        while (levels < MID_PRICE_N)
        {
            if (asks[idx]._isActive)
            {
                asksSum += asks[idx]._price * asks[idx]._totalQuantity;
                asksVolume += asks[idx]._totalQuantity;
                ++levels;
            }
            idx = (idx + 1) % LOB_DEPTH;
        }

        double totalVolume = bidsVolume + asksVolume;
        if (totalVolume == 0.0)
            return 0;

        return (bidsSum + asksSum) / totalVolume;
    }

    inline bool is_bids_full()
    {
        return numOfBids == LOB_DEPTH;
    }

    inline bool is_asks_full()
    {
        return numOfAsks == LOB_DEPTH;
    }

    void addUpdate(const MarketUpdate &update)
    {

        if (update._side == Side::SELL)
        {
            if (__glibc_unlikely(numOfAsks == 0))
            {
                int index = static_cast<int>(update._price / TICK_SIZE) % LOB_DEPTH;
                asks[index]._price = update._price;
                asks[index]._totalQuantity = update._quantity;
                asks[index]._isActive = true;
                bestAskIndex = worstAskIndex = index;
                ++numOfAsks;
            }
            else
            {
                if (bestAskIndex == worstAskIndex)
                {
                    int index = priceToIndex(update._price, false);
                    if (update._price > asks[bestAskIndex]._price)
                    {
                        worstAskIndex = index;
                    }
                    else
                    {
                        bestAskIndex = index;
                    }
                    asks[index]._price = update._price;
                    asks[index]._totalQuantity = update._quantity;
                    asks[index]._isActive = true;
                    ++numOfAsks;
                }

                else if (update._price < asks[worstAskIndex]._price && update._price > asks[bestAskIndex]._price)
                {
                    int index = priceToIndex(update._price, false);
                    asks[index]._price = update._price;
                    asks[index]._totalQuantity = update._quantity;
                    asks[index]._isActive = true;
                    ++numOfAsks;
                }
                else
                {
                    double ref_price = (update._price < asks[bestAskIndex]._price)
                                           ? asks[bestAskIndex]._price
                                           : asks[worstAskIndex]._price;
                    int step_size = abs(static_cast<int>(round((update._price - ref_price) / TICK_SIZE)));

                    if (step_size > MAX_SHIFT_STEP || (step_size > worstAskIndex || step_size > (LOB_DEPTH - bestAskIndex)))
                    {
                        return;
                    }
                    else
                    {
                        int index = priceToIndex(update._price, false);
                        if (update._price > asks[worstAskIndex]._price)
                        {
                            worstAskIndex = index;
                        }

                        else
                        {
                            bestAskIndex = index;
                        }

                        asks[index]._price = update._price;
                        asks[index]._totalQuantity = update._quantity;
                        asks[index]._isActive = true;
                        ++numOfAsks;
                    }
                }
            }
        }

        else
        {
            if (__glibc_unlikely(numOfBids == 0))
            {
                int index = static_cast<int>(update._price / TICK_SIZE) % LOB_DEPTH;
                bids[index]._price = update._price;
                bids[index]._totalQuantity = update._quantity;
                bids[index]._isActive = true;
                ++numOfBids;
                bestBidIndex = worstBidIndex = index;
            }
            else
            {
                if (bestBidIndex == worstBidIndex)
                {
                    int index = priceToIndex(update._price, true);
                    if (update._price > bids[bestBidIndex]._price)
                    {
                        bestBidIndex = index;
                    }
                    else
                    {
                        worstBidIndex = index;
                    }
                    bids[index]._price = update._price;
                    bids[index]._totalQuantity = update._quantity;
                    bids[index]._isActive = true;
                    ++numOfBids;
                }

                else if (update._price > bids[worstBidIndex]._price && update._price < bids[bestBidIndex]._price)
                {
                    int index = priceToIndex(update._price, true);
                    bids[index]._price = update._price;
                    bids[index]._totalQuantity = update._quantity;
                    bids[index]._isActive = true;
                    ++numOfBids;
                }
                else
                {
                    double ref_price = (update._price > bids[bestBidIndex]._price)
                                           ? bids[bestBidIndex]._price
                                           : bids[worstBidIndex]._price;
                    int step_size = abs(static_cast<int>(round((update._price - ref_price) / TICK_SIZE)));

                    if (step_size > MAX_SHIFT_STEP || (step_size > bestBidIndex || step_size > (LOB_DEPTH - worstBidIndex)))
                    {
                        return;
                    }
                    else
                    {
                        int index = priceToIndex(update._price, true);
                        if (update._price > bids[bestBidIndex]._price)
                        {
                            bestBidIndex = index;
                        }

                        else
                        {
                            worstBidIndex = index;
                        }

                        bids[index]._price = update._price;
                        bids[index]._totalQuantity = update._quantity;
                        bids[index]._isActive = true;
                        ++numOfBids;
                    }
                }
            }
        }
        // std::cerr << "order added with price  " << update._price << " and quantity " << update._quantity << std::endl;
    }

    void
    printOrderBook()
    {

        std::cout << "bids" << std::endl;
        for (int i = 0; i < bids.size(); i++)
        {
            std::cout << std::fixed << std::setprecision(6);
            std::cout << "bid price " << bids[i]._price << " quantity " << bids[i]._totalQuantity << std::endl;
        }

        std::cout << "////" << std::endl;
        std::cout << "asks" << std::endl;
        std::cout << "////" << std::endl;

        for (int i = 0; i < asks.size(); i++)
        {
            std::cout << std::fixed << std::setprecision(6);
            std::cout << "ask price " << asks[i]._price << " quantity " << asks[i]._totalQuantity << std::endl;
        }
    }

    double getBestAskPrice()
    {
        return asks[bestAskIndex]._price;
    }

    double getBestAskQuantity()
    {
        return asks[bestAskIndex]._totalQuantity;
    }

    double getBestBidPrice()
    {
        return bids[bestBidIndex]._price;
    }

    double getBestBidQuantity()
    {
        return bids[bestBidIndex]._totalQuantity;
    }

private:
    std::array<PriceLevel, LOB_DEPTH> bids;
    std::array<PriceLevel, LOB_DEPTH> asks;
    int bestBidIndex = 0;
    int worstBidIndex = 0;
    int bestAskIndex = 0;
    int worstAskIndex = 0;
    int numOfBids = 0;
    int numOfAsks = 0;
    int last_update_id = 0;
};