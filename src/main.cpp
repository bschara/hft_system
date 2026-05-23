#include <thread>
// #include "market_data/data_ingester/MDataIngester.h"
// #include "market_data/order_book/OrderBook.h"
// #include "utils/env_loader.hpp"
// #include "oms/order_management_system.h"
#include "utils/stream_config/stream_config.h"
#include <iostream>

int main()
{

    // loadEnv("../.env");

    // const char *api_key = std::getenv("BINANCE_API_KEY");
    // const char *secret_key = std::getenv("BINANCE_SECRET_KEY");

    // if (!api_key || !secret_key)
    // {
    //     std::cerr << "Missing keys!" << std::endl;
    //     return 1;
    // }

    // std::cout << "Loaded BINANCE_API_KEY: " << std::string(api_key).substr(0, 4) << "****" << std::endl;

    // char *host = "stream-sbe.binance.com";
    // char *port = "9443";
    // char *path = "/stream";
    // char *protocol = "sbe";
    // // std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@bestBidAsk"],"id":1})";
    // std::string query = R"({"method":"SUBSCRIBE","params":["btcusdt@depth"],"id":1})";

    try
    {
        StreamConfig config("../exchanges_data.csv");

        std::string url = config.buildBinanceURL();
        std::cout << "Binance WS URL: " << url << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
