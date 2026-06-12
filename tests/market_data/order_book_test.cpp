#include <gtest/gtest.h>
#include "market_data/order_book/order_book.h"
#include "market_data/order_book/market_update.h"

// Helper: build a MarketUpdate without needing a VenueRegistry in tests
static MarketUpdate ask(double price, double qty)
{
    return MarketUpdate(0, Side::SELL, price, qty, 0);
}

static MarketUpdate bid(double price, double qty)
{
    return MarketUpdate(0, Side::BUY, price, qty, 0);
}

// ─── Initialization ───────────────────────────────────────────────────────────

TEST(OrderBook, FreshBookBestPricesAreZero)
{
    OrderBook ob;
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 0.0);
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 0.0);
    EXPECT_DOUBLE_EQ(ob.getBestAskQuantity(), 0.0);
    EXPECT_DOUBLE_EQ(ob.getBestBidQuantity(), 0.0);
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
    // Must return quickly — if getMidPrice loops forever this test hangs
    EXPECT_DOUBLE_EQ(ob.getMidPrice(), 0.0);
}

// ─── Single-level operations ──────────────────────────────────────────────────

TEST(OrderBook, FirstAskSetsBestAskPrice)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.5));
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
    EXPECT_DOUBLE_EQ(ob.getBestAskQuantity(), 1.5);
    EXPECT_EQ(ob.getNumOfAsks(), 1);
}

TEST(OrderBook, FirstBidSetsBestBidPrice)
{
    OrderBook ob;
    ob.addUpdate(bid(99.99, 2.0));
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 99.99);
    EXPECT_DOUBLE_EQ(ob.getBestBidQuantity(), 2.0);
    EXPECT_EQ(ob.getNumOfBids(), 1);
}

TEST(OrderBook, UpdateExistingLevelChangesQuantity)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.5));
    ob.addUpdate(ask(100.00, 3.0)); // same price, new qty
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
    EXPECT_DOUBLE_EQ(ob.getBestAskQuantity(), 3.0);
    EXPECT_EQ(ob.getNumOfAsks(), 1);
}

// ─── Multi-level ordering ─────────────────────────────────────────────────────

TEST(OrderBook, MultiplAsksBestIsLowest)
{
    OrderBook ob;
    ob.addUpdate(ask(100.02, 1.0));
    ob.addUpdate(ask(100.00, 2.0));
    ob.addUpdate(ask(100.01, 3.0));
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
    EXPECT_EQ(ob.getNumOfAsks(), 3);
}

TEST(OrderBook, MultipleBidsBestIsHighest)
{
    OrderBook ob;
    ob.addUpdate(bid(99.97, 1.0));
    ob.addUpdate(bid(99.99, 2.0));
    ob.addUpdate(bid(99.98, 3.0));
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 99.99);
    EXPECT_EQ(ob.getNumOfBids(), 3);
}

TEST(OrderBook, AsksAndBidsAreIndependent)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0));
    ob.addUpdate(bid(99.99, 2.0));
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 99.99);
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_EQ(ob.getNumOfBids(), 1);
}

// ─── New best price (regression for priceToIndex ask-direction bug) ───────────

TEST(OrderBook, NewLowerAskBecomesNewBest)
{
    OrderBook ob;
    ob.addUpdate(ask(100.01, 1.0));
    ob.addUpdate(ask(100.00, 2.0)); // lower → should become new best
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
    EXPECT_EQ(ob.getNumOfAsks(), 2);
}

TEST(OrderBook, NewHigherBidBecomesNewBest)
{
    OrderBook ob;
    ob.addUpdate(bid(99.98, 1.0));
    ob.addUpdate(bid(99.99, 2.0)); // higher → should become new best
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 99.99);
    EXPECT_EQ(ob.getNumOfBids(), 2);
}

TEST(OrderBook, AskLevelsOrderedCorrectly)
{
    // Regression for the inverted ask priceToIndex bug.
    // After adding three asks at consecutive prices, the best must be the lowest.
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0));
    ob.addUpdate(ask(100.01, 1.0));
    ob.addUpdate(ask(100.02, 1.0));
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
}

TEST(OrderBook, BidLevelsOrderedCorrectly)
{
    OrderBook ob;
    ob.addUpdate(bid(99.99, 1.0));
    ob.addUpdate(bid(99.98, 1.0));
    ob.addUpdate(bid(99.97, 1.0));
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 99.99);
}

// ─── priceToIndex round-trip ──────────────────────────────────────────────────

TEST(OrderBook, PriceToIndexAskHigherPriceHigherIndex)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0)); // sets bestAskIndex
    int base = ob.priceToIndex(100.00, false);
    int above = ob.priceToIndex(100.01, false);
    int below = ob.priceToIndex(99.99, false);
    EXPECT_EQ(above, (base + 1) % LOB_DEPTH);
    EXPECT_EQ(below, (base - 1 + LOB_DEPTH) % LOB_DEPTH);
}

TEST(OrderBook, PriceToIndexBidHigherPriceHigherIndex)
{
    OrderBook ob;
    ob.addUpdate(bid(99.99, 1.0)); // sets bestBidIndex
    int base = ob.priceToIndex(99.99, true);
    int above = ob.priceToIndex(100.00, true);
    int below = ob.priceToIndex(99.98, true);
    EXPECT_EQ(above, (base + 1) % LOB_DEPTH);
    EXPECT_EQ(below, (base - 1 + LOB_DEPTH) % LOB_DEPTH);
}

// ─── Zero-quantity removal ────────────────────────────────────────────────────

TEST(OrderBook, RemoveAskByZeroQuantity)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.5));
    ASSERT_EQ(ob.getNumOfAsks(), 1);
    ob.addUpdate(ask(100.00, 0.0)); // remove
    EXPECT_EQ(ob.getNumOfAsks(), 0);
}

TEST(OrderBook, RemoveBidByZeroQuantity)
{
    OrderBook ob;
    ob.addUpdate(bid(99.99, 2.0));
    ASSERT_EQ(ob.getNumOfBids(), 1);
    ob.addUpdate(bid(99.99, 0.0)); // remove
    EXPECT_EQ(ob.getNumOfBids(), 0);
}

TEST(OrderBook, RemoveBestAskUpdatesBestToNextLevel)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0)); // best
    ob.addUpdate(ask(100.01, 1.0)); // second
    ob.addUpdate(ask(100.00, 0.0)); // remove best
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.01);
}

TEST(OrderBook, RemoveBestBidUpdatesBestToNextLevel)
{
    OrderBook ob;
    ob.addUpdate(bid(99.99, 1.0)); // best
    ob.addUpdate(bid(99.98, 1.0)); // second
    ob.addUpdate(bid(99.99, 0.0)); // remove best
    EXPECT_EQ(ob.getNumOfBids(), 1);
    EXPECT_DOUBLE_EQ(ob.getBestBidPrice(), 99.98);
}

TEST(OrderBook, RemoveMidLevelLeavesEndpointsUnchanged)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0)); // best
    ob.addUpdate(ask(100.01, 1.0)); // mid
    ob.addUpdate(ask(100.02, 1.0)); // worst
    ob.addUpdate(ask(100.01, 0.0)); // remove mid
    EXPECT_EQ(ob.getNumOfAsks(), 2);
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
}

TEST(OrderBook, RemoveNonExistentLevelIsNoop)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0));
    ob.addUpdate(ask(100.05, 0.0)); // price not in book
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
}

// ─── getMidPrice correctness ──────────────────────────────────────────────────

TEST(OrderBook, GetMidPriceSymmetricBook)
{
    OrderBook ob;
    // bid: $99.99 qty=1, ask: $100.01 qty=1
    // VWAP = (99.99*1 + 100.01*1) / 2 = 100.00
    ob.addUpdate(bid(99.99, 1.0));
    ob.addUpdate(ask(100.01, 1.0));
    EXPECT_NEAR(ob.getMidPrice(), 100.00, 0.001);
}

TEST(OrderBook, GetMidPriceSparseBookDoesNotHang)
{
    // Only 2 active levels per side — well below MID_PRICE_N=5
    // Must return without looping forever
    OrderBook ob;
    ob.addUpdate(bid(99.99, 1.0));
    ob.addUpdate(bid(99.98, 2.0));
    ob.addUpdate(ask(100.01, 1.0));
    ob.addUpdate(ask(100.02, 2.0));
    double mid = ob.getMidPrice();
    EXPECT_GT(mid, 0.0);
}

TEST(OrderBook, GetMidPriceWeightsQuantitiesCorrectly)
{
    OrderBook ob;
    // bid: $99.00 qty=3, ask: $101.00 qty=1
    // VWAP = (99*3 + 101*1) / 4 = 398/4 = 99.5
    ob.addUpdate(bid(99.00, 3.0));
    ob.addUpdate(ask(101.00, 1.0));
    EXPECT_NEAR(ob.getMidPrice(), 99.5, 0.001);
}

// ─── Out-of-range guard ───────────────────────────────────────────────────────

TEST(OrderBook, OutOfRangeAskIsSilentlyIgnored)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0));
    // Price more than MAX_SHIFT_STEP ticks away — should be rejected
    double far_price = 100.00 + (MAX_SHIFT_STEP + 5) * TICK_SIZE;
    ob.addUpdate(ask(far_price, 1.0));
    EXPECT_EQ(ob.getNumOfAsks(), 1);
    EXPECT_DOUBLE_EQ(ob.getBestAskPrice(), 100.00);
}

TEST(OrderBook, OutOfRangeBidIsSilentlyIgnored)
{
    OrderBook ob;
    ob.addUpdate(bid(99.99, 1.0));
    double far_price = 99.99 + (MAX_SHIFT_STEP + 5) * TICK_SIZE;
    ob.addUpdate(bid(far_price, 1.0));
    // Out-of-range on the upper side for a bid book should be ignored
    EXPECT_EQ(ob.getNumOfBids(), 1);
}

TEST(OrderBook, WithinRangeAskIsAccepted)
{
    OrderBook ob;
    ob.addUpdate(ask(100.00, 1.0));
    // Exactly one tick outside current range but within MAX_SHIFT_STEP
    ob.addUpdate(ask(100.01, 1.0));
    EXPECT_EQ(ob.getNumOfAsks(), 2);
}
