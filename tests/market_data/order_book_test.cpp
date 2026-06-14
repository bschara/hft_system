#include <gtest/gtest.h>
#include "market_data/order_book/order_book.h"
#include "market_data/order_book/market_update.h"

// Prices as raw integers with priceExp=-2 (raw 10000 = $100.00).
// Quantities as raw integers with qtyExp=0 for simplicity in tests.
static constexpr int8_t TEST_PRICE_EXP = -2;
static constexpr int8_t TEST_QTY_EXP   =  0;

static MarketUpdate ask(int64_t price, int64_t qty)
{
    return MarketUpdate(0, Side::SELL, price, qty, TEST_PRICE_EXP, TEST_QTY_EXP, 0);
}

static MarketUpdate bid(int64_t price, int64_t qty)
{
    return MarketUpdate(0, Side::BUY, price, qty, TEST_PRICE_EXP, TEST_QTY_EXP, 0);
}

// ─── Initialization ───────────────────────────────────────────────────────────

TEST(OrderBook, FreshBookBestPricesAreZero)
{
    OrderBook ob;
    EXPECT_EQ(ob.getBestAskRaw(), 0);
    EXPECT_EQ(ob.getBestBidRaw(), 0);
    EXPECT_EQ(ob.getBestAskQtyRaw(), 0);
    EXPECT_EQ(ob.getBestBidQtyRaw(), 0);
}

TEST(OrderBook, FreshBookCountsAreZero)
{
    OrderBook ob;
    EXPECT_EQ(ob.getNumOfAsks(), 0);
    EXPECT_EQ(ob.getNumOfBids(), 0);
}

TEST(OrderBook, GetMidPriceEmptyBookReturnsZeroWithoutHanging)
{
    OrderBook ob;
    EXPECT_EQ(ob.getMidRaw(), 0);
}

// ─── Single-level operations ──────────────────────────────────────────────────

TEST(OrderBook, FirstAskSetsBestAskPrice)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 15));  // $100.00, qty=15
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
    EXPECT_EQ(ob.getBestAskQtyRaw(), 15);
    EXPECT_EQ(ob.getNumOfAsks(), 1);
}

TEST(OrderBook, FirstBidSetsBestBidPrice)
{
    OrderBook ob;
    ob.addUpdate(bid(9999, 20));  // $99.99, qty=20
    EXPECT_EQ(ob.getBestBidRaw(), 9999);
    EXPECT_EQ(ob.getBestBidQtyRaw(), 20);
    EXPECT_EQ(ob.getNumOfBids(), 1);
}

TEST(OrderBook, UpdateExistingLevelChangesQuantity)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 15));
    ob.addUpdate(ask(10000, 30));  // same price, new qty
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
    EXPECT_EQ(ob.getBestAskQtyRaw(), 30);
    EXPECT_EQ(ob.getNumOfAsks(), 1);
}

// ─── Multi-level ordering ─────────────────────────────────────────────────────

TEST(OrderBook, MultiplAsksBestIsLowest)
{
    OrderBook ob;
    ob.addUpdate(ask(10002, 10));
    ob.addUpdate(ask(10000, 20));
    ob.addUpdate(ask(10001, 30));
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
    EXPECT_EQ(ob.getNumOfAsks(), 3);
}

TEST(OrderBook, MultipleBidsBestIsHighest)
{
    OrderBook ob;
    ob.addUpdate(bid(9997, 10));
    ob.addUpdate(bid(9999, 20));
    ob.addUpdate(bid(9998, 30));
    EXPECT_EQ(ob.getBestBidRaw(), 9999);
    EXPECT_EQ(ob.getNumOfBids(), 3);
}

TEST(OrderBook, AsksAndBidsAreIndependent)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));
    ob.addUpdate(bid(9999,  20));
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
    EXPECT_EQ(ob.getBestBidRaw(), 9999);
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_EQ(ob.getNumOfBids(), 1);
}

// ─── New best price ───────────────────────────────────────────────────────────

TEST(OrderBook, NewLowerAskBecomesNewBest)
{
    OrderBook ob;
    ob.addUpdate(ask(10001, 10));
    ob.addUpdate(ask(10000, 20));  // lower → should become new best
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
    EXPECT_EQ(ob.getNumOfAsks(), 2);
}

TEST(OrderBook, NewHigherBidBecomesNewBest)
{
    OrderBook ob;
    ob.addUpdate(bid(9998, 10));
    ob.addUpdate(bid(9999, 20));  // higher → should become new best
    EXPECT_EQ(ob.getBestBidRaw(), 9999);
    EXPECT_EQ(ob.getNumOfBids(), 2);
}

TEST(OrderBook, AskLevelsOrderedCorrectly)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));
    ob.addUpdate(ask(10001, 10));
    ob.addUpdate(ask(10002, 10));
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
}

TEST(OrderBook, BidLevelsOrderedCorrectly)
{
    OrderBook ob;
    ob.addUpdate(bid(9999, 10));
    ob.addUpdate(bid(9998, 10));
    ob.addUpdate(bid(9997, 10));
    EXPECT_EQ(ob.getBestBidRaw(), 9999);
}

// ─── priceToIndex round-trip ──────────────────────────────────────────────────

TEST(OrderBook, PriceToIndexAskHigherPriceHigherIndex)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));  // sets bestAskIndex
    int base  = ob.priceToIndex(10000, false);
    int above = ob.priceToIndex(10001, false);
    int below = ob.priceToIndex(9999,  false);
    EXPECT_EQ(above, (base + 1) % LOB_DEPTH);
    EXPECT_EQ(below, (base - 1 + LOB_DEPTH) % LOB_DEPTH);
}

TEST(OrderBook, PriceToIndexBidHigherPriceHigherIndex)
{
    OrderBook ob;
    ob.addUpdate(bid(9999, 10));  // sets bestBidIndex
    int base  = ob.priceToIndex(9999,  true);
    int above = ob.priceToIndex(10000, true);
    int below = ob.priceToIndex(9998,  true);
    EXPECT_EQ(above, (base + 1) % LOB_DEPTH);
    EXPECT_EQ(below, (base - 1 + LOB_DEPTH) % LOB_DEPTH);
}

// ─── Zero-quantity removal ────────────────────────────────────────────────────

TEST(OrderBook, RemoveAskByZeroQuantity)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 15));
    ASSERT_EQ(ob.getNumOfAsks(), 1);
    ob.addUpdate(ask(10000, 0));  // remove
    EXPECT_EQ(ob.getNumOfAsks(), 0);
}

TEST(OrderBook, RemoveBidByZeroQuantity)
{
    OrderBook ob;
    ob.addUpdate(bid(9999, 20));
    ASSERT_EQ(ob.getNumOfBids(), 1);
    ob.addUpdate(bid(9999, 0));  // remove
    EXPECT_EQ(ob.getNumOfBids(), 0);
}

TEST(OrderBook, RemoveBestAskUpdatesBestToNextLevel)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));  // best
    ob.addUpdate(ask(10001, 10));  // second
    ob.addUpdate(ask(10000, 0));   // remove best
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_EQ(ob.getBestAskRaw(), 10001);
}

TEST(OrderBook, RemoveBestBidUpdatesBestToNextLevel)
{
    OrderBook ob;
    ob.addUpdate(bid(9999, 10));  // best
    ob.addUpdate(bid(9998, 10));  // second
    ob.addUpdate(bid(9999, 0));   // remove best
    EXPECT_EQ(ob.getNumOfBids(), 1);
    EXPECT_EQ(ob.getBestBidRaw(), 9998);
}

TEST(OrderBook, RemoveMidLevelLeavesEndpointsUnchanged)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));  // best
    ob.addUpdate(ask(10001, 10));  // mid
    ob.addUpdate(ask(10002, 10));  // worst
    ob.addUpdate(ask(10001, 0));   // remove mid
    EXPECT_EQ(ob.getNumOfAsks(), 2);
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
}

TEST(OrderBook, RemoveNonExistentLevelIsNoop)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));
    ob.addUpdate(ask(10005, 0));  // price not in book
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
}

// ─── getMidRaw / getMidPrice correctness ─────────────────────────────────────

TEST(OrderBook, GetMidRawSymmetricBook)
{
    OrderBook ob;
    // bid: 9999 qty=1, ask: 10001 qty=1 → VWAP = (9999+10001)/2 = 10000
    ob.addUpdate(bid(9999, 1));
    ob.addUpdate(ask(10001, 1));
    EXPECT_EQ(ob.getMidRaw(), 10000);
}

TEST(OrderBook, GetMidPriceSparseBookDoesNotHang)
{
    // Only 2 active levels per side — well below MID_PRICE_N=5
    OrderBook ob;
    ob.addUpdate(bid(9999, 10));
    ob.addUpdate(bid(9998, 20));
    ob.addUpdate(ask(10001, 10));
    ob.addUpdate(ask(10002, 20));
    EXPECT_GT(ob.getMidRaw(), 0);
}

TEST(OrderBook, GetMidRawWeightsQuantitiesCorrectly)
{
    OrderBook ob;
    // bid: 9900 qty=3, ask: 10100 qty=1 → VWAP = (9900*3 + 10100*1)/(3+1) = 39800/4 = 9950
    ob.addUpdate(bid(9900, 3));
    ob.addUpdate(ask(10100, 1));
    EXPECT_EQ(ob.getMidRaw(), 9950);
    // getMidPrice() converts to double: 9950 * 10^-2 = 99.50
    EXPECT_NEAR(ob.getMidPrice(), 99.50, 0.001);
}

// ─── Out-of-range guard ───────────────────────────────────────────────────────

TEST(OrderBook, OutOfRangeAskIsSilentlyIgnored)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));
    // Price more than MAX_SHIFT_STEP ticks away — should be rejected
    int64_t far_price = 10000 + (MAX_SHIFT_STEP + 5) * TICK_UNITS;
    ob.addUpdate(ask(far_price, 10));
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_EQ(ob.getBestAskRaw(), 10000);
}

TEST(OrderBook, OutOfRangeBidIsSilentlyIgnored)
{
    OrderBook ob;
    ob.addUpdate(bid(9999, 10));
    int64_t far_price = 9999 + (MAX_SHIFT_STEP + 5) * TICK_UNITS;
    ob.addUpdate(bid(far_price, 10));
    EXPECT_EQ(ob.getNumOfBids(), 1);
}

TEST(OrderBook, WithinRangeAskIsAccepted)
{
    OrderBook ob;
    ob.addUpdate(ask(10000, 10));
    ob.addUpdate(ask(10001, 10));  // one tick away — accepted
    EXPECT_EQ(ob.getNumOfAsks(), 2);
}
