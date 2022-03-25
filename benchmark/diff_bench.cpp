#include <benchmark/benchmark.h>

#include "crypto/utils.hpp"

static void BM_BitsToDiff(benchmark::State& state)
{
    for (auto _ : state)
    {
        difficulty(rand());
    }
}

static void BM_BitsToDiff2(benchmark::State& state)
{
    for (auto _ : state)
    {
        BitsToDiff(rand());
    }
}

BENCHMARK(BM_BitsToDiff);
BENCHMARK(BM_BitsToDiff2);