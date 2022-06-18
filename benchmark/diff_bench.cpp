#include <benchmark/benchmark.h>

#include "crypto/utils.hpp"

inline double difficultyStatic(const unsigned bits)
{
    const unsigned exponent_diff = 8 * (0x20 - ((bits >> 24) & 0xFF));
    const double significand = bits & 0xFFFFFF;
    return std::ldexp(0x0f0f0f / significand, exponent_diff);
}


static void BM_BitsToDiff(benchmark::State& state)
{
    for (auto _ : state)
    {
        difficulty(rand());
    }
}

static void BM_BitsToDiffOld(benchmark::State& state)
{
    for (auto _ : state)
    {
        BitsToDiff(rand());
    }
}

inline double GetExpectedHashesStatic(const double diff)
{
    // return diff * (16777216 / 0x0f0f0f); // 2^24
    return diff * (17.00000101327902);  // 2^ 24 / 0x0f0f0f = 17...z
}

static void BM_EXPECTED_HASHES(benchmark::State& state)
{
    for (auto _ : state)
    {
        GetExpectedHashes(rand());
    }
}

static void BM_EXPECTED_HASHES_STATIC(benchmark::State& state)
{
    for (auto _ : state)
    {
        GetExpectedHashesStatic(rand());
    }
}

BENCHMARK(BM_BitsToDiff);
BENCHMARK(BM_BitsToDiffOld);
BENCHMARK(BM_EXPECTED_HASHES);
BENCHMARK(BM_EXPECTED_HASHES_STATIC);