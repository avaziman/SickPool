#include <benchmark/benchmark.h>

static void BM_Snprintf(benchmark::State& state)
{
    char response[1024];
    for (auto _ : state)
    {
        int len =
            snprintf(response, sizeof(response),
                     "{\"id\":%d,\"result\":[null,\"%s\"],\"error\":null}\n",
                     1, "00000000");
    }
}

BENCHMARK(BM_Snprintf);