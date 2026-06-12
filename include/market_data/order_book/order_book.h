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

    PriceLevel(double price, double quantity) : _price(price), _totalQuantity(quantity) {}
};

class OrderBook
{

public:
    OrderBook();

    ~OrderBook();

    // Maps a price to a circular-buffer slot index, relative to the current best price.
    //
    // Layout invariant (same for both sides):
    //   - bestBidIndex / bestAskIndex → slot of the best (most favourable) price
    //   - For BIDS:  higher index = higher price = better bid; -1 walk in getMidPrice
    //   - For ASKS:  higher index = higher price = worse ask;  +1 walk in getMidPrice
    //   - priceToIndex uses (price - best) / TICK_SIZE so that a price one tick
    //     ABOVE best → index+1, one tick BELOW best → index-1, for BOTH sides.
    int priceToIndex(double price, bool is_bid) const
    {
        if (is_bid)
        {
            int tickDiff = int(round((price - bids[bestBidIndex]._price) / TICK_SIZE));
            return (bestBidIndex + tickDiff + LOB_DEPTH) % LOB_DEPTH;
        }
        else
        {
            // Corrected: same sign convention as bids — higher price → higher index
            int tickDiff = int(round((price - asks[bestAskIndex]._price) / TICK_SIZE));
            return (bestAskIndex + tickDiff + LOB_DEPTH) % LOB_DEPTH;
        }
    }

    double getMidPrice() noexcept
    {
        double bidsSum = 0.0, bidsVolume = 0.0;
        double asksSum = 0.0, asksVolume = 0.0;

        // Walk bids: bestBidIndex is highest price; -1 goes to lower (worse) bids
        int idx = bestBidIndex;
        int levels = 0;
        int steps = 0;
        while (levels < MID_PRICE_N && steps < LOB_DEPTH)
        {
            if (bids[idx]._isActive)
            {
                bidsSum += bids[idx]._price * bids[idx]._totalQuantity;
                bidsVolume += bids[idx]._totalQuantity;
                ++levels;
            }
            idx = (idx - 1 + LOB_DEPTH) % LOB_DEPTH;
            ++steps;
        }

        // Walk asks: bestAskIndex is lowest price; +1 goes to higher (worse) asks
        idx = bestAskIndex;
        levels = 0;
        steps = 0;
        while (levels < MID_PRICE_N && steps < LOB_DEPTH)
        {
            if (asks[idx]._isActive)
            {
                asksSum += asks[idx]._price * asks[idx]._totalQuantity;
                asksVolume += asks[idx]._totalQuantity;
                ++levels;
            }
            idx = (idx + 1) % LOB_DEPTH;
            ++steps;
        }

        double totalVolume = bidsVolume + asksVolume;
        if (totalVolume == 0.0)
            return 0.0;

        return (bidsSum + asksSum) / totalVolume;
    }

    inline bool is_bids_full() const { return numOfBids == LOB_DEPTH; }
    inline bool is_asks_full() const { return numOfAsks == LOB_DEPTH; }

    void addUpdate(const MarketUpdate &update)
    {
        if (update._side == Side::SELL)
        {
            // --- Remove (quantity == 0 means "delete this level") ---
            if (update._quantity == 0.0)
            {
                if (numOfAsks == 0) return;
                int index = priceToIndex(update._price, false);
                if (!asks[index]._isActive) return;

                asks[index]._isActive = false;
                asks[index]._totalQuantity = 0.0;
                --numOfAsks;

                if (numOfAsks == 0) { bestAskIndex = worstAskIndex = 0; return; }

                // If best was removed, scan +1 (toward higher/worse prices) for new best.
                // The new best is the lowest remaining price, which is one slot higher.
                if (index == bestAskIndex)
                {
                    for (int i = 1; i < LOB_DEPTH; ++i)
                    {
                        int c = (index + i) % LOB_DEPTH;
                        if (asks[c]._isActive) { bestAskIndex = c; break; }
                    }
                }
                // If worst was removed, scan -1 (toward lower/better prices) for new worst.
                else if (index == worstAskIndex)
                {
                    for (int i = 1; i < LOB_DEPTH; ++i)
                    {
                        int c = (index - i + LOB_DEPTH) % LOB_DEPTH;
                        if (asks[c]._isActive) { worstAskIndex = c; break; }
                    }
                }
                return;
            }

            // --- Add / Update ---
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
                    // Single level — guard against placing a second level too far away
                    int step_size = int(round(std::abs(update._price - asks[bestAskIndex]._price) / TICK_SIZE));
                    if (step_size > MAX_SHIFT_STEP) return;

                    int index = priceToIndex(update._price, false);
                    if (update._price > asks[bestAskIndex]._price)
                        worstAskIndex = index;
                    else if (update._price < asks[bestAskIndex]._price)
                        bestAskIndex = index;
                    // else: same price — update quantity only

                    asks[index]._price = update._price;
                    asks[index]._totalQuantity = update._quantity;
                    asks[index]._isActive = true;
                    if (update._price != asks[bestAskIndex == index ? worstAskIndex : bestAskIndex]._price)
                        ++numOfAsks;
                }
                else if (update._price >= asks[bestAskIndex]._price &&
                         update._price <= asks[worstAskIndex]._price)
                {
                    // In range — update or add the slot
                    int index = priceToIndex(update._price, false);
                    if (!asks[index]._isActive) ++numOfAsks;
                    asks[index]._price = update._price;
                    asks[index]._totalQuantity = update._quantity;
                    asks[index]._isActive = true;
                }
                else
                {
                    // Out of range — extend best or worst
                    int step_size;
                    if (update._price < asks[bestAskIndex]._price)
                    {
                        // New best ask (lower price): new index = bestAskIndex - steps
                        step_size = int(round((asks[bestAskIndex]._price - update._price) / TICK_SIZE));
                        if (step_size > MAX_SHIFT_STEP || bestAskIndex - step_size < 0)
                            return;
                        int index = priceToIndex(update._price, false);
                        bestAskIndex = index;
                        asks[index]._price = update._price;
                        asks[index]._totalQuantity = update._quantity;
                        asks[index]._isActive = true;
                        ++numOfAsks;
                    }
                    else
                    {
                        // New worst ask (higher price): new index = worstAskIndex + steps
                        step_size = int(round((update._price - asks[worstAskIndex]._price) / TICK_SIZE));
                        if (step_size > MAX_SHIFT_STEP || worstAskIndex + step_size >= LOB_DEPTH)
                            return;
                        int index = priceToIndex(update._price, false);
                        worstAskIndex = index;
                        asks[index]._price = update._price;
                        asks[index]._totalQuantity = update._quantity;
                        asks[index]._isActive = true;
                        ++numOfAsks;
                    }
                }
            }
        }
        else // BUY side
        {
            // --- Remove ---
            if (update._quantity == 0.0)
            {
                if (numOfBids == 0) return;
                int index = priceToIndex(update._price, true);
                if (!bids[index]._isActive) return;

                bids[index]._isActive = false;
                bids[index]._totalQuantity = 0.0;
                --numOfBids;

                if (numOfBids == 0) { bestBidIndex = worstBidIndex = 0; return; }

                // bestBid = highest price = highest index; removing it → scan -1 for new best
                if (index == bestBidIndex)
                {
                    for (int i = 1; i < LOB_DEPTH; ++i)
                    {
                        int c = (index - i + LOB_DEPTH) % LOB_DEPTH;
                        if (bids[c]._isActive) { bestBidIndex = c; break; }
                    }
                }
                // worstBid = lowest price = lowest index; removing it → scan +1 for new worst
                else if (index == worstBidIndex)
                {
                    for (int i = 1; i < LOB_DEPTH; ++i)
                    {
                        int c = (index + i) % LOB_DEPTH;
                        if (bids[c]._isActive) { worstBidIndex = c; break; }
                    }
                }
                return;
            }

            // --- Add / Update ---
            if (__glibc_unlikely(numOfBids == 0))
            {
                int index = static_cast<int>(update._price / TICK_SIZE) % LOB_DEPTH;
                bids[index]._price = update._price;
                bids[index]._totalQuantity = update._quantity;
                bids[index]._isActive = true;
                bestBidIndex = worstBidIndex = index;
                ++numOfBids;
            }
            else
            {
                if (bestBidIndex == worstBidIndex)
                {
                    int step_size = int(round(std::abs(update._price - bids[bestBidIndex]._price) / TICK_SIZE));
                    if (step_size > MAX_SHIFT_STEP) return;

                    int index = priceToIndex(update._price, true);
                    if (update._price > bids[bestBidIndex]._price)
                        bestBidIndex = index;
                    else if (update._price < bids[bestBidIndex]._price)
                        worstBidIndex = index;

                    bids[index]._price = update._price;
                    bids[index]._totalQuantity = update._quantity;
                    bids[index]._isActive = true;
                    if (update._price != bids[bestBidIndex == index ? worstBidIndex : bestBidIndex]._price)
                        ++numOfBids;
                }
                else if (update._price <= bids[bestBidIndex]._price &&
                         update._price >= bids[worstBidIndex]._price)
                {
                    int index = priceToIndex(update._price, true);
                    if (!bids[index]._isActive) ++numOfBids;
                    bids[index]._price = update._price;
                    bids[index]._totalQuantity = update._quantity;
                    bids[index]._isActive = true;
                }
                else
                {
                    int step_size;
                    if (update._price > bids[bestBidIndex]._price)
                    {
                        // New best bid (higher price): new index = bestBidIndex + steps
                        step_size = int(round((update._price - bids[bestBidIndex]._price) / TICK_SIZE));
                        if (step_size > MAX_SHIFT_STEP || bestBidIndex + step_size >= LOB_DEPTH)
                            return;
                        int index = priceToIndex(update._price, true);
                        bestBidIndex = index;
                        bids[index]._price = update._price;
                        bids[index]._totalQuantity = update._quantity;
                        bids[index]._isActive = true;
                        ++numOfBids;
                    }
                    else
                    {
                        // New worst bid (lower price): new index = worstBidIndex - steps
                        step_size = int(round((bids[worstBidIndex]._price - update._price) / TICK_SIZE));
                        if (step_size > MAX_SHIFT_STEP || worstBidIndex - step_size < 0)
                            return;
                        int index = priceToIndex(update._price, true);
                        worstBidIndex = index;
                        bids[index]._price = update._price;
                        bids[index]._totalQuantity = update._quantity;
                        bids[index]._isActive = true;
                        ++numOfBids;
                    }
                }
            }
        }
    }

    void printOrderBook()
    {
        std::cout << "bids\n";
        for (int i = 0; i < (int)bids.size(); i++)
        {
            if (bids[i]._isActive)
                std::cout << std::fixed << std::setprecision(6)
                          << "  bid[" << i << "] price=" << bids[i]._price
                          << " qty=" << bids[i]._totalQuantity << "\n";
        }
        std::cout << "asks\n";
        for (int i = 0; i < (int)asks.size(); i++)
        {
            if (asks[i]._isActive)
                std::cout << std::fixed << std::setprecision(6)
                          << "  ask[" << i << "] price=" << asks[i]._price
                          << " qty=" << asks[i]._totalQuantity << "\n";
        }
    }

    double getBestAskPrice()     const { return asks[bestAskIndex]._price; }
    double getBestAskQuantity()  const { return asks[bestAskIndex]._totalQuantity; }
    double getBestBidPrice()     const { return bids[bestBidIndex]._price; }
    double getBestBidQuantity()  const { return bids[bestBidIndex]._totalQuantity; }

    int getNumOfAsks() const { return numOfAsks; }
    int getNumOfBids() const { return numOfBids; }

private:
    std::array<PriceLevel, LOB_DEPTH> bids;
    std::array<PriceLevel, LOB_DEPTH> asks;
    int bestBidIndex  = 0;
    int worstBidIndex = 0;
    int bestAskIndex  = 0;
    int worstAskIndex = 0;
    int numOfBids     = 0;
    int numOfAsks     = 0;
    int last_update_id = 0;
};
