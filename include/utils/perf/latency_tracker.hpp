#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <time.h>
#include "benchmark/benchmark_utility.hpp"

namespace Common {

// Log2 histogram: bucket b holds samples in [2^b, 2^(b+1)-1].
// O(1) record, allocation-free, covers 1 cycle to ~9e18 cycles.
struct LatencyHistogram {
    char     name[32];
    uint64_t buckets[64];
    uint64_t total_samples;

    explicit LatencyHistogram(const char* n = "") : total_samples(0)
    {
        std::memset(buckets, 0, sizeof(buckets));
        std::strncpy(name, n, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }

    void record(uint64_t cycles) noexcept
    {
        int bucket = (cycles == 0) ? 0 : (63 - __builtin_clzll(cycles));
        ++buckets[bucket];
        ++total_samples;
    }

    void reset() noexcept
    {
        std::memset(buckets, 0, sizeof(buckets));
        total_samples = 0;
    }

    void report(double ns_per_cycle) const
    {
        if (total_samples == 0) {
            std::printf("[%-14s]  no samples\n", name);
            return;
        }

        auto percentile_ns = [&](double pct) -> double {
            uint64_t threshold = static_cast<uint64_t>(total_samples * pct / 100.0 + 0.5);
            if (threshold == 0) threshold = 1;
            uint64_t acc = 0;
            for (int b = 0; b < 64; ++b) {
                acc += buckets[b];
                if (acc >= threshold)
                    return static_cast<double>(1ULL << b) * ns_per_cycle;
            }
            return 0.0;
        };

        std::printf("[%-14s]  p50=%7.0f ns  p99=%8.0f ns  p999=%9.0f ns  samples=%lu\n",
                    name,
                    percentile_ns(50.0),
                    percentile_ns(99.0),
                    percentile_ns(99.9),
                    (unsigned long)total_samples);
    }
};

// Calibrate TSC frequency by comparing a ~10ms wall-clock spin to TSC delta.
// Returns nanoseconds per TSC cycle.
inline double calibrate_tsc_ns()
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uint64_t tsc0 = rdtsc_start();

    // Busy-wait for 10ms
    struct timespec target = t0;
    target.tv_nsec += 10'000'000;
    if (target.tv_nsec >= 1'000'000'000) {
        target.tv_sec++;
        target.tv_nsec -= 1'000'000'000;
    }
    do {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    } while (t1.tv_sec < target.tv_sec ||
             (t1.tv_sec == target.tv_sec && t1.tv_nsec < target.tv_nsec));

    uint64_t tsc1 = rdtsc_end();

    uint64_t wall_ns  = static_cast<uint64_t>(t1.tv_sec  - t0.tv_sec)  * 1'000'000'000ULL
                      + static_cast<uint64_t>(t1.tv_nsec - t0.tv_nsec);
    uint64_t tsc_delta = tsc1 - tsc0;

    return static_cast<double>(wall_ns) / static_cast<double>(tsc_delta);
}

} // namespace Common
