#pragma once
#include <unordered_map>
#include "simdjson.h"
#include "utils/net/http_client/http_client.h"
#include <libpq-fe.h>
#include "market_data/order_book/order_book.h"

constexpr int TICK_DATA_SIZE = 100;

class HistoricalDataAggergator
{
public:
    HistoricalDataAggergator(HttpClient &client_ref, const char *_conninfo,
                             Common::LFQueue<MarketUpdate> *_market_data_queue, OrderBook &_order_book);

    ~HistoricalDataAggergator();

    void fetchHistoricalData();

    void connectToDB();

    double fetchAverageDailyVolume();

    void ComputeVolatility();

    void insert_tick_data();

    void run();

    void stop();

    void writeToDB(const MarketUpdate *_market_update);

    double getVolatility();

    double getAverageDailyVolume();

    std::vector<double> computeReturns();

private:
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    HttpClient &http_client;
    Common::LFQueue<MarketUpdate> *market_data_queue;
    PGconn *conn;
    OrderBook &order_book;
    std::array<double, TICK_DATA_SIZE> tick_data;
    std::atomic<size_t> write_index = {0};
    const char *conninfo;
    int num_of_elements = 0;
    double volatility = 0;
    double average_daily_volume = 0;
    bool running = false;
};