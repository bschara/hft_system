#include <cassert>
#include <iostream>
#include <string>
#include <thread>

#include "utils/env_loader.hpp"
#include "utils/stream_config/stream_config.h"
#include "market_data/venue_registry.hpp"
#include "utils/lock_free_queue.hpp"
#include "utils/tls_client/tls_client.h"
#include "utils/websocket/websocket.h"

#include "market_data/order_book/order_book_manager.h"
#include "market_data/data_ingester/market_data_ingester.h"

#include "strategies/strategy_manager.hpp"
#include "strategies/mean_reversion/midprice_reversion.h"

int main()
{
    loadEnv("../.env");

    // --- Config & registry ---
    StreamConfig config("../exchanges_data.csv");
    const auto  &rows = config.getRows();

    VenueRegistry registry(rows);

    // Sanity-check instrument IDs match expected ordering from CSV
    assert(registry.lookup("BINANCE", "BTCUSDT") == 0x00000000);
    assert(registry.lookup("BINANCE", "ETHUSDT") == 0x00000001);
    assert(registry.lookup("BINANCE", "BNBUSDT") == 0x00000002);

    // --- Queues ---
    Common::LFQueue<MarketUpdate> update_queue(4096);
    Common::LFQueue<double>       signals_queue(1024);

    // --- OrderBookManager ---
    OrderBookManager obm(update_queue, registry.size());
    for (const auto &r : rows)
        obm.register_instrument(registry.lookup(r.exchange, r.symbol));

    // --- Strategies ---
    StrategyManager strategy_manager(&signals_queue);
    obm.set_strategy_manager(&strategy_manager);

    const uint32_t btc_id = registry.lookup("BINANCE", "BTCUSDT");
    const uint32_t eth_id = registry.lookup("BINANCE", "ETHUSDT");
    const uint32_t bnb_id = registry.lookup("BINANCE", "BNBUSDT");

    MidPriceReversion btc_strat(obm.book_for(btc_id));
    MidPriceReversion eth_strat(obm.book_for(eth_id));
    MidPriceReversion bnb_strat(obm.book_for(bnb_id));

    strategy_manager.register_strategy(btc_id, &btc_strat);
    strategy_manager.register_strategy(eth_id, &eth_strat);
    strategy_manager.register_strategy(bnb_id, &bnb_strat);

    // --- Binance ingester ---
    // buildBinanceURL() returns the full wss:// URL; extract just the path portion.
    std::string full_url     = config.buildBinanceURL();
    std::string binance_path = full_url.substr(full_url.find("/stream?"));

    utility::TLSClient binance_tls("stream.binance.com", 9443);
    utility::WebSocket binance_ws(binance_tls, "stream.binance.com", "");

    MarketDataIngester binance_ingester(update_queue, binance_tls, binance_ws,
                                        registry, "BINANCE");

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
