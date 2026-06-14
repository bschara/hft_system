#include <cassert>
#include <iostream>
#include <string>
#include <thread>

#include "utils/config/env_loader.hpp"
#include "utils/config/stream_config/stream_config.h"
#include "market_data/venue_registry.hpp"
#include "utils/containers/lock_free_queue.hpp"
#include "utils/net/tls_client/tls_client.h"
#include "utils/net/websocket/websocket.h"
#include "utils/perf/latency_tracker.hpp"

#include "market_data/order_book/order_book_manager.h"
#include "market_data/data_ingester/message_schema.h"
#include "market_data/data_ingester/schema_registry.h"
#include "market_data/data_ingester/sbe_venue_parser.h"
#include "market_data/data_ingester/market_data_ingester.h"

#include "strategies/strategy_manager.hpp"
#include "strategies/mean_reversion/midprice_reversion.h"
#include "strategies/mean_reversion/orderbook_imbalance.h"
#include "strategies/trend_following/micro_momentum.h"

int main()
{
    loadEnv("../.env");

    // --- Config & registry ---
    StreamConfig      config("../exchanges_data.csv");
    const auto       &rows = config.getRows();
    VenueRegistry     registry(rows);

    // Sanity-check instrument IDs match expected ordering from CSV
    assert(registry.lookup("BINANCE", "BTCUSDT") == 0x00000000);
    assert(registry.lookup("BINANCE", "ETHUSDT") == 0x00000001);
    assert(registry.lookup("BINANCE", "BNBUSDT") == 0x00000002);

    // --- TSC calibration ---
    double ns_per_cycle = Common::calibrate_tsc_ns();
    std::printf("[main] TSC calibration: %.3f ns/cycle\n", ns_per_cycle);

    // --- Per-venue queues (one per ingester thread — preserves SPSC guarantee) ---
    Common::LFQueue<MarketUpdate> binance_queue(4096);
    Common::LFQueue<int32_t>      signals_queue(8192);

    // --- OrderBookManager ---
    OrderBookManager obm(registry.size());
    for (const auto &r : rows)
        obm.register_instrument(registry.lookup(r.exchange, r.symbol));
    obm.add_queue(binance_queue);
    obm.set_ns_per_cycle(ns_per_cycle);

    // --- Strategies (all three types, one instance per symbol) ---
    StrategyManager strategy_manager(&signals_queue);
    obm.set_strategy_manager(&strategy_manager);

    const uint32_t btc_id = registry.lookup("BINANCE", "BTCUSDT");
    const uint32_t eth_id = registry.lookup("BINANCE", "ETHUSDT");
    const uint32_t bnb_id = registry.lookup("BINANCE", "BNBUSDT");

    MidPriceReversion  btc_mr(obm.book_for(btc_id));
    OrderBookImbalance btc_oi(obm.book_for(btc_id));
    MicroMomentum      btc_mm(obm.book_for(btc_id));

    MidPriceReversion  eth_mr(obm.book_for(eth_id));
    OrderBookImbalance eth_oi(obm.book_for(eth_id));
    MicroMomentum      eth_mm(obm.book_for(eth_id));

    MidPriceReversion  bnb_mr(obm.book_for(bnb_id));
    OrderBookImbalance bnb_oi(obm.book_for(bnb_id));
    MicroMomentum      bnb_mm(obm.book_for(bnb_id));

    strategy_manager.register_strategy(btc_id, &btc_mr);
    strategy_manager.register_strategy(btc_id, &btc_oi);
    strategy_manager.register_strategy(btc_id, &btc_mm);

    strategy_manager.register_strategy(eth_id, &eth_mr);
    strategy_manager.register_strategy(eth_id, &eth_oi);
    strategy_manager.register_strategy(eth_id, &eth_mm);

    strategy_manager.register_strategy(bnb_id, &bnb_mr);
    strategy_manager.register_strategy(bnb_id, &bnb_oi);
    strategy_manager.register_strategy(bnb_id, &bnb_mm);

    // --- Binance schema + parser ---
    SchemaRegistry binance_schemas;
    binance_schemas.registerSchema(kBinanceDepthDiff);      // depth@<sym>
    binance_schemas.registerSchema(kBinanceDepthSnapshot);  // depth<N>@<sym>
    binance_schemas.registerSchema(kBinanceBestBidAsk);     // bookTicker@<sym> (skip)
    binance_schemas.registerSchema(kBinanceTrades);         // trade@<sym>      (skip)

    SBEVenueParser binance_parser(std::move(binance_schemas), registry, "BINANCE");

    // Extract path from full wss:// URL
    std::string full_url     = config.buildBinanceURL();
    std::string binance_path = full_url.substr(full_url.find("/stream?"));

    const char *api_key_env = std::getenv("BINANCE_API_KEY");
    std::string binance_api_key = api_key_env ? api_key_env : "";

    utility::TLSClient binance_tls("stream-sbe.binance.com", 9443);
    utility::WebSocket binance_ws(binance_tls, "stream-sbe.binance.com", binance_api_key);

    MarketDataIngester binance_ingester(binance_queue, binance_tls, binance_ws, binance_parser);
    binance_ingester.set_ns_per_cycle(ns_per_cycle);

    // --- Launch threads ---
    std::thread obm_thread([&] { obm.run(); });

    std::thread binance_thread([&] {
        binance_ingester.startReceiving(binance_path, "");
    });

    binance_thread.join();
    obm.stop();
    obm_thread.join();

    return 0;
}
