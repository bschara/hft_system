#include <thread>
#include "market_data/data_ingester/MDataIngester.h"
#include "market_data/order_book/OrderBook.h"
#include "utils/env_loader.hpp"
#include "oms/order_management_system.h"

int main()
{

    loadEnv("../.env");

    const char *api_key = std::getenv("BINANCE_API_KEY");
    const char *secret_key = std::getenv("BINANCE_SECRET_KEY");

    if (!api_key || !secret_key)
    {
        std::cerr << "Missing keys!" << std::endl;
        return 1;
    }

    std::cout << "Loaded BINANCE_API_KEY: " << std::string(api_key).substr(0, 4) << "****" << std::endl;

    char *host = "stream-sbe.binance.com";
    char *port = "9443";
    char *path = "/stream";
    char *protocol = "sbe";
    // std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@bestBidAsk"],"id":1})";
    std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@depth"],"id":1})";

    Common::LFQueue<MarketUpdate> *q1 = new Common::LFQueue<MarketUpdate>(256);
    OrderBook o1(q1);

    utility::TLSClient tls_client{host, std::stoi(port)};
    utility::BMWebSocket bmwesocket{tls_client, host, api_key};

    MDataIngester mdi{q1, query, path, protocol, tls_client, bmwesocket};

    std::thread ingestThread([&mdi]()
                             { mdi.startReceiving(); });

    std::thread desThread([&q1]()
                          {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // wait once

        std::cerr << std::fixed << std::setprecision(4);

        while (true)
        {
            auto* update = q1->getNextToRead();
            if(update){
                std::cerr << "price " << update->_price << " quantity " << update->_quantity << " Side " << update->_side << std::endl;
                q1->updateReadIndex();

            }
          
        } });

    // std::thread dataThread([&o1]()
    //                        {
    //                             std::this_thread::sleep_for(std::chrono::seconds(10)); // wait once

    //                             std::cerr << std::fixed << std::setprecision(4);

    //                             while (true) {
    //                                 o1.run();
    //                                 } });

    ingestThread.join();
    desThread.join();
    // dataThread.join();
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

// #include <thread>
// #include "market_data/data_ingester/MDataIngester.h"
// #include "market_data/order_book/OrderBook.h"

// int main() {
//     Common::LFQueue<MarketUpdate>* q1 = new Common::LFQueue<MarketUpdate>(256);

//     OrderBook o1(q1);

//     MarketUpdate m1{MarketUpdateType::ADD, 13213123123, Side::BUY, 105000, 12, 12344243};
//     MarketUpdate m2{MarketUpdateType::ADD, 13213123123, Side::SELL, 105000, 12, 12344243};
//     MarketUpdate m3{MarketUpdateType::ADD, 13213123123, Side::BUY, 105000, 12, 12344243};
//     MarketUpdate m4{MarketUpdateType::ADD, 13213123123, Side::SELL, 105000, 12, 12344243};
//     o1.onMarketUpdate(&m1);
//     o1.onMarketUpdate(&m2);
//     o1.onMarketUpdate(&m3);
//     o1.onMarketUpdate(&m4);
//     o1.printOrderBook();
// }

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

// #include "order_gateway/order_gateway.h"
// #include <iostream>
// #include <thread>
// #include <chrono>

// int main()
// {

//     try
//     {
//         OrderGateway gateway;
//         gateway.start();

//         std::cout << "FIX gateway started. Press Ctrl+C to exit." << std::endl;
//         std::this_thread::sleep_for(std::chrono::hours(24));

//         gateway.stop();
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Exception: " << e.what() << std::endl;
//         return 1;
//     }

//     return 0;
// }

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

// #include <thread>
// #include "market_data/data_ingester/MDataIngester.h"
// #include "market_data/order_book/OrderBook.h"
// #include "strategies/strategy_manager.hpp"
// #include "strategies/mean_reversion/midprice_reversion.h"

// int main()
// {

//     char *host = "stream-sbe.binance.com";
//     char *port = "9443";
//     char *path = "/stream";
//     char *protocol = "sbe";
//     std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@bestBidAsk"],"id":1})";

//     Common::LFQueue<MarketUpdate> *q1 = new Common::LFQueue<MarketUpdate>(256);
//     Common::LFQueue<MarketUpdate> *q2 = new Common::LFQueue<MarketUpdate>(256);
//     OrderBook o1(q1);

//     utility::TLSClient tls_client{host, std::stoi(port)};
//     utility::BMWebSocket bmwesocket{tls_client, host, api_key};

//     utility::TLSClient tls_client2{host, std::stoi(port)};
//     utility::BMWebSocket bmwesocket2{tls_client2, host, api_key};

//     MDataIngester mdi{q1, query, path, protocol, tls_client, bmwesocket};
//     MDataIngester mdi2{q2, query, path, protocol, tls_client2, bmwesocket2};

//     StrategyManager *strat_manager = new StrategyManager();

//     MidPriceReversion *mid_price_strat = new MidPriceReversion(o1);

//     strat_manager->register_strategy(mid_price_strat);

//     std::thread ingestThread([&mdi]()
//                              { mdi.startReceiving(); });

//     std::thread ingestThread2([&mdi2]()
//                               { mdi2.startReceiving(); });

//     std::thread dataThread([&o1]()
//                            {
//                                 std::this_thread::sleep_for(std::chrono::seconds(10)); // wait once

//                                 std::cerr << std::fixed << std::setprecision(4);

//                                 while (true) {
//                                     o1.run();
//                                     } });

//     std::thread managerThread([&strat_manager, &q2]()
//                               {
//     std::cerr << "starting strat" << std::endl;

//     while (true) {
//         auto update = q2->getNextToRead();
//         if (update != nullptr) {
//             strat_manager->onMarketData(update);
//             q2->updateReadIndex();
//         }
//     } });

//     ingestThread.join();
//     ingestThread2.join();
//     dataThread.join();
//     managerThread.join();
// }

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

// #include <iostream>
// #include "utils/HttpClient/HttpClient.h"

// int main()
// {
//     try
//     {
//         HttpClient client;
//         std::string response = client.get("https://api.binance.com/api/v3/time");
//         std::cout << "Response: " << response << std::endl;
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Request failed: " << e.what() << std::endl;
//     }

//     return 0;
// }

// #include <iostream>
// #include "market_data/historical_data_aggregator/historical_aggregator.h"
// #include "market_data/data_ingester/MDataIngester.h"

// int main()
// {

//     char *host = "stream-sbe.binance.com";
//     char *port = "9443";
//     char *path = "/stream";
//     char *protocol = "sbe";
//     std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@bestBidAsk"],"id":1})";

//     Common::LFQueue<MarketUpdate> *q1 = new Common::LFQueue<MarketUpdate>(256);
//     Common::LFQueue<MarketUpdate> *q2 = new Common::LFQueue<MarketUpdate>(256);

//     OrderBook o1(q1);
//     utility::TLSClient tls_client{host, std::stoi(port)};
//     utility::BMWebSocket bmwesocket{tls_client, host, api_key};

//     utility::TLSClient tls_client2{host, std::stoi(port)};
//     utility::BMWebSocket bmwesocket2{tls_client2, host, api_key};

//     MDataIngester mdi{q1, query, path, protocol, tls_client, bmwesocket};

//     MDataIngester mdi2{q2, query, path, protocol, tls_client2, bmwesocket2};

//     HttpClient client;
//     const char *conninfo = "host=timescaledb port=5432 dbname=devdb user=dev password=devpass";

//     HistoricalDataAggergator h1{client, conninfo, q2, o1};
//     h1.connectToDB();

//     std::thread ingestThread([&mdi]()
//                              { mdi.startReceiving(); });

//     std::thread ingestThread2([&mdi2]()
//                               { mdi2.startReceiving(); });

//     std::thread dataThread([&o1]()
//                            {
//                                     std::this_thread::sleep_for(std::chrono::seconds(10)); // wait once

//                                     std::cerr << std::fixed << std::setprecision(4);

//                                     while (true) {
//                                         o1.run();
//                                         } });

//     std::thread histthread([&h1]()
//                            {std::this_thread::sleep_for(std::chrono::seconds(30));

//                             std::cerr << std::fixed << std::setprecision(4);

//                             while(true){
//                             h1.run(); } });

//     ingestThread.join();
//     ingestThread2.join();
//     dataThread.join();
//     histthread.join();
// }

// #include <iostream>
// #include "market_data/historical_data_aggregator/historical_aggregator.h"
// #include "market_data/data_ingester/MDataIngester.h"
// #include "strategies/strategy_manager.hpp"
// #include "strategies/mean_reversion/midprice_reversion.h"
// #include "pcm_model/pcm_model.h"

// int main()
// {

//     char *host = "stream-sbe.binance.com";
//     char *port = "9443";
//     char *path = "/stream";
//     char *protocol = "sbe";
//     std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@bestBidAsk"],"id":1})";

//     Common::LFQueue<MarketUpdate> *q1 = new Common::LFQueue<MarketUpdate>(256);
//     Common::LFQueue<MarketUpdate> *q2 = new Common::LFQueue<MarketUpdate>(256);
//     Common::LFQueue<double> *q3 = new Common::LFQueue<double>(256);

//     OrderBook o1(q1);
//     utility::TLSClient tls_client{host, std::stoi(port)};
//     utility::BMWebSocket bmwesocket{tls_client, host, api_key};

//     utility::TLSClient tls_client2{host, std::stoi(port)};
//     utility::BMWebSocket bmwesocket2{tls_client2, host, api_key};

//     MDataIngester mdi{q1, query, path, protocol, tls_client, bmwesocket};

//     MDataIngester mdi2{q2, query, path, protocol, tls_client2, bmwesocket2};

//     StrategyManager sm{q3};

//     MidPriceReversion *mpr = new MidPriceReversion{o1};

//     sm.register_strategy(mpr);

//     PCModel pcm{3000000, q3, o1};

//     std::thread ingestThread([&mdi]()
//                              { mdi.startReceiving(); });

//     std::thread ingestThread2([&mdi2]()
//                               { mdi2.startReceiving(); });

//     std::thread dataThread([&o1]()
//                            {
//     std::this_thread::sleep_for(std::chrono::seconds(10)); // wait once

//     std::cerr << std::fixed << std::setprecision(4);

//     while (true) {
//         o1.run();
//     } });

//     std::thread strategyManagerThread([&sm, &q2]()
//                                       {
//     std::this_thread::sleep_for(std::chrono::seconds(20));

//     while(true){

//     std::cerr << std::fixed << std::setprecision(4);
//     auto update = q2->getNextToRead();
//     if(update){
//     sm.onMarketData(update);
//     q2->updateReadIndex();
// }

//         } });

//     std::thread signalPrinterThread([&pcm]()
//                                     {
//         std::this_thread::sleep_for(std::chrono::seconds(25));

//         while (true)
//         {
//             pcm.run();
//         } });

//     ingestThread.join();
//     ingestThread2.join();
//     dataThread.join();
//     strategyManagerThread.join();
//     signalPrinterThread.join();
// }
