#include "market_data/data_ingester/market_data_ingester.h"
#include "utils/perf/benchmark/benchmark_utility.hpp"
#include <cstdio>
#include <iostream>

MarketDataIngester::MarketDataIngester(Common::LFQueue<MarketUpdate> &mDataQueue,
                                       utility::TLSClient &tlsClient,
                                       utility::WebSocket &socketClient,
                                       VenueParser &parser)
    : updatesQueue(mDataQueue), tls_client(tlsClient),
      web_socket(socketClient), parser_(parser)
{
}

MarketDataIngester::~MarketDataIngester()
{
    running = false;
}

void MarketDataIngester::startReceiving(std::string_view path, std::string_view protocol)
{
    if (!running)
        running = true;

    if (!tls_client.connect())
    {
        std::cerr << "TLS connection failed\n";
        return;
    }

    if (!web_socket.perform_handshake(path, protocol))
    {
        std::cerr << "WebSocket handshake failed\n";
        return;
    }

    while (running)
    {
        uint64_t t_start = rdtsc_start();
        auto frame = web_socket.read_frame();
        uint64_t t_frame_done = rdtsc_end();

        if (!frame)
            continue;

        uint64_t t_io = web_socket.io_done_tsc();
        hist_ws_io_   .record(t_io          - t_start);
        hist_ws_cpu_  .record(t_frame_done  - t_io);
        hist_ws_total_.record(t_frame_done  - t_start);

        if (frame->opcode == 0x2)
        {
            uint64_t t_sbe0 = rdtsc_start();
            parser_.parse(frame->payload, updatesQueue);
            uint64_t t_sbe1 = rdtsc_end();
            hist_sbe_.record(t_sbe1 - t_sbe0);
        }

        if (++ws_frame_count_ % kWsReportInterval == 0)
            report_ws_latencies();
    }
}

void MarketDataIngester::report_ws_latencies() const
{
    std::printf("--- WS latency (%lu frames) ---\n", (unsigned long)ws_frame_count_);
    hist_ws_total_.report(ns_per_cycle_);
    hist_ws_io_   .report(ns_per_cycle_);
    hist_ws_cpu_  .report(ns_per_cycle_);
    hist_sbe_     .report(ns_per_cycle_);
    std::fflush(stdout);
}

void MarketDataIngester::stopReceiving()
{
    running = false;
}
