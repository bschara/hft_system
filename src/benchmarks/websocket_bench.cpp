#include <cstdio>
#include <cstdint>
#include <cstring>
#include <array>
#include <optional>

#include "utils/perf/latency_tracker.hpp"
#include "utils/perf/benchmark/benchmark_utility.hpp"
#include "utils/config/env_loader.hpp"
#include "utils/config/stream_config/stream_config.h"
#include "utils/net/tls_client/tls_client.h"
#include "utils/net/websocket/websocket.h"
#include "utils/containers/lock_free_queue.hpp"
#include "market_data/order_book/market_update.h"
#include "market_data/venue_registry.hpp"
#include "market_data/data_ingester/message_schema.h"
#include "market_data/data_ingester/schema_registry.h"
#include "market_data/data_ingester/sbe_venue_parser.h"

static constexpr int      N_WARMUP      = 200;
static constexpr int      N_LIVE_FRAMES = 10'000;
static constexpr int      N_CPU_ITERS   = 1'000'000;

// ── Phase B: CPU-only microbenchmarks ────────────────────────────────────────

static void bench_mask_payload(double ns_per_cycle)
{
    alignas(64) std::array<uint8_t, 256> buf{};
    for (size_t i = 0; i < buf.size(); ++i) buf[i] = static_cast<uint8_t>(i);
    const uint8_t mask[4] = {0xAB, 0xCD, 0xEF, 0x12};

    Common::LatencyHistogram hist{"mask_256B"};

    for (int i = 0; i < N_CPU_ITERS; ++i)
    {
        buf[0] = static_cast<uint8_t>(i); // re-dirty to block CSE/hoisting

        uint64_t t0 = rdtsc_start();
        for (size_t j = 0; j < buf.size(); ++j)
            buf[j] ^= mask[j % 4];
        uint64_t t1 = rdtsc_end();

        hist.record(t1 - t0);
    }

    hist.report(ns_per_cycle);
}

static void bench_header_decode(double ns_per_cycle)
{
    // 2-byte header + 2-byte extended length (payload_len == 126 path, len=256)
    alignas(8) uint8_t hdr[4] = {0x82, 0x7E, 0x01, 0x00};

    Common::LatencyHistogram hist{"hdr_decode"};

    for (int i = 0; i < N_CPU_ITERS; ++i)
    {
        uint64_t t0 = rdtsc_start();

        bool     fin         = hdr[0] & 0x80;
        uint8_t  opcode      = hdr[0] & 0x0F;
        bool     masked      = hdr[1] & 0x80;
        uint64_t payload_len = hdr[1] & 0x7F;

        if (payload_len == 126)
            payload_len = (uint64_t(hdr[2]) << 8) | uint64_t(hdr[3]);

        uint64_t t1 = rdtsc_end();
        hist.record(t1 - t0);

        // prevent DCE
        if (__builtin_expect(opcode == 0xFF, 0))
            std::printf("%d %d %lu\n", (int)fin, (int)masked, payload_len);
    }

    hist.report(ns_per_cycle);
}

// ── Phase A: Live network baseline ───────────────────────────────────────────

static void run_live_bench(double ns_per_cycle)
{
    StreamConfig config("../exchanges_data.csv");
    VenueRegistry registry(config.getRows());

    const char *key_env = std::getenv("BINANCE_API_KEY");
    std::string api_key = key_env ? key_env : "";

    std::string full_url = config.buildBinanceURL();
    std::string ws_path  = full_url.substr(full_url.find("/stream?"));

    utility::TLSClient tls("stream-sbe.binance.com", 9443);
    utility::WebSocket ws(tls, "stream-sbe.binance.com", api_key);

    if (!tls.connect())       { std::fprintf(stderr, "[wsbench] TLS connect failed\n");   return; }
    if (!ws.perform_handshake(ws_path, "")) { std::fprintf(stderr, "[wsbench] WS handshake failed\n"); return; }

    SchemaRegistry schemas;
    schemas.registerSchema(kBinanceDepthDiff);
    schemas.registerSchema(kBinanceDepthSnapshot);
    schemas.registerSchema(kBinanceBestBidAsk);
    schemas.registerSchema(kBinanceTrades);
    SBEVenueParser parser(std::move(schemas), registry, "BINANCE");

    Common::LFQueue<MarketUpdate> null_queue(4096);

    Common::LatencyHistogram hist_total{"ws_frame_tot"};
    Common::LatencyHistogram hist_io   {"ws_io"};
    Common::LatencyHistogram hist_cpu  {"ws_cpu"};
    Common::LatencyHistogram hist_sbe  {"sbe_decode"};

    std::printf("[wsbench] warming up (%d frames)...\n", N_WARMUP);

    int collected    = 0;
    int total_frames = 0;

    while (collected < N_LIVE_FRAMES)
    {
        uint64_t t_start = rdtsc_start();
        auto frame = ws.read_frame();
        uint64_t t_done = rdtsc_end();

        if (!frame) continue;

        ++total_frames;
        if (total_frames <= N_WARMUP) continue;

        uint64_t t_io = ws.io_done_tsc();
        hist_io   .record(t_io   - t_start);
        hist_cpu  .record(t_done - t_io);
        hist_total.record(t_done - t_start);

        if (frame->opcode == 0x2)
        {
            uint64_t t_sbe0 = rdtsc_start();
            parser.parse(frame->payload, null_queue);
            uint64_t t_sbe1 = rdtsc_end();
            hist_sbe.record(t_sbe1 - t_sbe0);

            // drain so queue never fills
            while (const MarketUpdate *u = null_queue.getNextToRead())
            {
                (void)u;
                null_queue.updateReadIndex();
            }
        }

        ++collected;
    }

    std::printf("\n--- Live WS Benchmark (%d frames, %d warmup discarded) ---\n",
                collected, N_WARMUP);
    hist_total.report(ns_per_cycle);
    hist_io   .report(ns_per_cycle);
    hist_cpu  .report(ns_per_cycle);
    hist_sbe  .report(ns_per_cycle);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main()
{
    loadEnv("../.env");

    double ns_per_cycle = Common::calibrate_tsc_ns();
    std::printf("[wsbench] TSC: %.3f ns/cycle\n\n", ns_per_cycle);

    run_live_bench(ns_per_cycle);

    std::printf("\n--- CPU-only microbenchmarks (%d iters each) ---\n", N_CPU_ITERS);
    bench_mask_payload(ns_per_cycle);
    bench_header_decode(ns_per_cycle);

    return 0;
}
