#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <iostream>
#include "market_update.h"
#include "utils/lock_free_queue.hpp"
#include "utils/price_utils.hpp"

constexpr int     LOB_DEPTH      = 256;
constexpr int64_t TICK_UNITS     = 1;      // one raw integer unit = 1 tick
constexpr int     MID_PRICE_N    = 5;
constexpr int     MAX_SHIFT_STEP = LOB_DEPTH / 4;

struct alignas(32) PriceLevel
{
    int64_t _price    = 0;
    int64_t _quantity = 0;
    bool    _isActive = false;

    PriceLevel() = default;
    PriceLevel(int64_t price, int64_t quantity) : _price(price), _quantity(quantity) {}
};

class OrderBook
{
public:
    OrderBook()  = default;
    ~OrderBook() = default;

    // Maps a price to a circular-buffer slot index, relative to the current best.
    // Higher index = higher price for both sides.
    int priceToIndex(int64_t price, bool is_bid) const
    {
        if (is_bid) {
            int64_t diff = (price - bids[bestBidIndex]._price) / TICK_UNITS;
            int64_t raw  = static_cast<int64_t>(bestBidIndex) + diff;
            return static_cast<int>(((raw % LOB_DEPTH) + LOB_DEPTH) % LOB_DEPTH);
        } else {
            int64_t diff = (price - asks[bestAskIndex]._price) / TICK_UNITS;
            int64_t raw  = static_cast<int64_t>(bestAskIndex) + diff;
            return static_cast<int>(((raw % LOB_DEPTH) + LOB_DEPTH) % LOB_DEPTH);
        }
    }

    // Volume-weighted mid price in raw integer units.
    int64_t getMidRaw() noexcept
    {
        __int128 bidsSum = 0; int64_t bidsVol = 0;
        __int128 asksSum = 0; int64_t asksVol = 0;

        int idx = bestBidIndex, levels = 0, steps = 0;
        while (levels < MID_PRICE_N && steps < LOB_DEPTH) {
            if (bids[idx]._isActive) {
                bidsSum += (__int128)bids[idx]._price * bids[idx]._quantity;
                bidsVol += bids[idx]._quantity;
                ++levels;
            }
            idx = (idx - 1 + LOB_DEPTH) % LOB_DEPTH;
            ++steps;
        }

        idx = bestAskIndex; levels = 0; steps = 0;
        while (levels < MID_PRICE_N && steps < LOB_DEPTH) {
            if (asks[idx]._isActive) {
                asksSum += (__int128)asks[idx]._price * asks[idx]._quantity;
                asksVol += asks[idx]._quantity;
                ++levels;
            }
            idx = (idx + 1) % LOB_DEPTH;
            ++steps;
        }

        int64_t totalVol = bidsVol + asksVol;
        if (totalVol == 0) return 0;
        return static_cast<int64_t>((bidsSum + asksSum) / totalVol);
    }

    // Double mid price for PCModel / display only — not on the strategy hot path.
    double getMidPrice() noexcept
    {
        return PriceUtils::to_double(getMidRaw(), price_exp_);
    }

    inline bool is_bids_full() const { return numOfBids == LOB_DEPTH; }
    inline bool is_asks_full() const { return numOfAsks == LOB_DEPTH; }

    void addUpdate(const MarketUpdate &update)
    {
        // Latch exponents from first update; assumed constant per instrument.
        if (!exp_set_) {
            price_exp_ = update._price_exp;
            qty_exp_   = update._qty_exp;
            exp_set_   = true;
        }

        if (update._side == Side::SELL)
        {
            // --- Remove ---
            if (update._quantity == 0)
            {
                if (numOfAsks == 0) return;
                int index = priceToIndex(update._price, false);
                if (!asks[index]._isActive) return;
                asks[index]._isActive = false;
                asks[index]._quantity = 0;
                --numOfAsks;
                if (numOfAsks == 0) { bestAskIndex = worstAskIndex = 0; return; }
                if (index == bestAskIndex) {
                    for (int i = 1; i < LOB_DEPTH; ++i) {
                        int c = (index + i) % LOB_DEPTH;
                        if (asks[c]._isActive) { bestAskIndex = c; break; }
                    }
                } else if (index == worstAskIndex) {
                    for (int i = 1; i < LOB_DEPTH; ++i) {
                        int c = (index - i + LOB_DEPTH) % LOB_DEPTH;
                        if (asks[c]._isActive) { worstAskIndex = c; break; }
                    }
                }
                return;
            }

            // --- Add / Update ---
            if (__glibc_unlikely(numOfAsks == 0))
            {
                int index = static_cast<int>(std::abs(update._price) % LOB_DEPTH);
                asks[index]._price    = update._price;
                asks[index]._quantity = update._quantity;
                asks[index]._isActive = true;
                bestAskIndex = worstAskIndex = index;
                ++numOfAsks;
            }
            else if (bestAskIndex == worstAskIndex)
            {
                int step_size = static_cast<int>(std::abs(update._price - asks[bestAskIndex]._price));
                if (step_size > MAX_SHIFT_STEP) return;
                int index = priceToIndex(update._price, false);
                if (update._price > asks[bestAskIndex]._price)
                    worstAskIndex = index;
                else if (update._price < asks[bestAskIndex]._price)
                    bestAskIndex = index;
                asks[index]._price    = update._price;
                asks[index]._quantity = update._quantity;
                asks[index]._isActive = true;
                if (update._price != asks[bestAskIndex == index ? worstAskIndex : bestAskIndex]._price)
                    ++numOfAsks;
            }
            else if (update._price >= asks[bestAskIndex]._price &&
                     update._price <= asks[worstAskIndex]._price)
            {
                int index = priceToIndex(update._price, false);
                if (!asks[index]._isActive) ++numOfAsks;
                asks[index]._price    = update._price;
                asks[index]._quantity = update._quantity;
                asks[index]._isActive = true;
            }
            else if (update._price < asks[bestAskIndex]._price)
            {
                int step_size = static_cast<int>(asks[bestAskIndex]._price - update._price);
                if (step_size > MAX_SHIFT_STEP || bestAskIndex - step_size < 0) return;
                int index = priceToIndex(update._price, false);
                bestAskIndex = index;
                asks[index]._price    = update._price;
                asks[index]._quantity = update._quantity;
                asks[index]._isActive = true;
                ++numOfAsks;
            }
            else
            {
                int step_size = static_cast<int>(update._price - asks[worstAskIndex]._price);
                if (step_size > MAX_SHIFT_STEP || worstAskIndex + step_size >= LOB_DEPTH) return;
                int index = priceToIndex(update._price, false);
                worstAskIndex = index;
                asks[index]._price    = update._price;
                asks[index]._quantity = update._quantity;
                asks[index]._isActive = true;
                ++numOfAsks;
            }
        }
        else // BUY
        {
            // --- Remove ---
            if (update._quantity == 0)
            {
                if (numOfBids == 0) return;
                int index = priceToIndex(update._price, true);
                if (!bids[index]._isActive) return;
                bids[index]._isActive = false;
                bids[index]._quantity = 0;
                --numOfBids;
                if (numOfBids == 0) { bestBidIndex = worstBidIndex = 0; return; }
                if (index == bestBidIndex) {
                    for (int i = 1; i < LOB_DEPTH; ++i) {
                        int c = (index - i + LOB_DEPTH) % LOB_DEPTH;
                        if (bids[c]._isActive) { bestBidIndex = c; break; }
                    }
                } else if (index == worstBidIndex) {
                    for (int i = 1; i < LOB_DEPTH; ++i) {
                        int c = (index + i) % LOB_DEPTH;
                        if (bids[c]._isActive) { worstBidIndex = c; break; }
                    }
                }
                return;
            }

            // --- Add / Update ---
            if (__glibc_unlikely(numOfBids == 0))
            {
                int index = static_cast<int>(std::abs(update._price) % LOB_DEPTH);
                bids[index]._price    = update._price;
                bids[index]._quantity = update._quantity;
                bids[index]._isActive = true;
                bestBidIndex = worstBidIndex = index;
                ++numOfBids;
            }
            else if (bestBidIndex == worstBidIndex)
            {
                int step_size = static_cast<int>(std::abs(update._price - bids[bestBidIndex]._price));
                if (step_size > MAX_SHIFT_STEP) return;
                int index = priceToIndex(update._price, true);
                if (update._price > bids[bestBidIndex]._price)
                    bestBidIndex = index;
                else if (update._price < bids[bestBidIndex]._price)
                    worstBidIndex = index;
                bids[index]._price    = update._price;
                bids[index]._quantity = update._quantity;
                bids[index]._isActive = true;
                if (update._price != bids[bestBidIndex == index ? worstBidIndex : bestBidIndex]._price)
                    ++numOfBids;
            }
            else if (update._price <= bids[bestBidIndex]._price &&
                     update._price >= bids[worstBidIndex]._price)
            {
                int index = priceToIndex(update._price, true);
                if (!bids[index]._isActive) ++numOfBids;
                bids[index]._price    = update._price;
                bids[index]._quantity = update._quantity;
                bids[index]._isActive = true;
            }
            else if (update._price > bids[bestBidIndex]._price)
            {
                int step_size = static_cast<int>(update._price - bids[bestBidIndex]._price);
                if (step_size > MAX_SHIFT_STEP || bestBidIndex + step_size >= LOB_DEPTH) return;
                int index = priceToIndex(update._price, true);
                bestBidIndex = index;
                bids[index]._price    = update._price;
                bids[index]._quantity = update._quantity;
                bids[index]._isActive = true;
                ++numOfBids;
            }
            else
            {
                int step_size = static_cast<int>(bids[worstBidIndex]._price - update._price);
                if (step_size > MAX_SHIFT_STEP || worstBidIndex - step_size < 0) return;
                int index = priceToIndex(update._price, true);
                worstBidIndex = index;
                bids[index]._price    = update._price;
                bids[index]._quantity = update._quantity;
                bids[index]._isActive = true;
                ++numOfBids;
            }
        }
    }

    void printOrderBook() const
    {
        std::cout << "bids\n";
        for (int i = 0; i < LOB_DEPTH; ++i)
            if (bids[i]._isActive)
                std::cout << "  bid[" << i << "] price=" << bids[i]._price
                          << " qty=" << bids[i]._quantity << "\n";
        std::cout << "asks\n";
        for (int i = 0; i < LOB_DEPTH; ++i)
            if (asks[i]._isActive)
                std::cout << "  ask[" << i << "] price=" << asks[i]._price
                          << " qty=" << asks[i]._quantity << "\n";
    }

    int64_t getBestAskRaw()    const { return asks[bestAskIndex]._price; }
    int64_t getBestAskQtyRaw() const { return asks[bestAskIndex]._quantity; }
    int64_t getBestBidRaw()    const { return bids[bestBidIndex]._price; }
    int64_t getBestBidQtyRaw() const { return bids[bestBidIndex]._quantity; }

    int getNumOfAsks() const { return numOfAsks; }
    int getNumOfBids() const { return numOfBids; }

    int8_t price_exp() const { return price_exp_; }
    int8_t qty_exp()   const { return qty_exp_; }

private:
    std::array<PriceLevel, LOB_DEPTH> bids{};
    std::array<PriceLevel, LOB_DEPTH> asks{};
    int    bestBidIndex   = 0;
    int    worstBidIndex  = 0;
    int    bestAskIndex   = 0;
    int    worstAskIndex  = 0;
    int    numOfBids      = 0;
    int    numOfAsks      = 0;
    int    last_update_id = 0;
    int8_t price_exp_     = 0;
    int8_t qty_exp_       = 0;
    bool   exp_set_       = false;
};
