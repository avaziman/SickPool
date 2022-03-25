#include <benchmark/benchmark.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>

#include "redis_manager.hpp"

using namespace std::chrono;

static RedisManager redisManager("VRSC_BENCH", "127.0.0.1:6379");

static void BM_GetCurTime(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::time_t time =
            duration_cast<milliseconds>(system_clock::now().time_since_epoch())
                .count();
    }
}

static void BM_GetCurTimeC(benchmark::State& state)
{
    for (auto _ : state)
    {
        struct timeval time_now
        {
        };
        gettimeofday(&time_now, nullptr);
        time_t msecs_time =
            (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    }
}

// create a timeseries for each worker
static void BM_AddWorker(benchmark::State& state)
{
    for (auto _ : state)
    {
        redisManager.AddWorker("worker1");
    }
}

static void BM_AddShare(benchmark::State& state)
{
    for (auto _ : state)
    {
        redisManager.AddShare("worker1", 1647508482627, 15042.401234);
    }
}

BENCHMARK(BM_GetCurTime);
BENCHMARK(BM_GetCurTimeC);
BENCHMARK(BM_AddWorker);
BENCHMARK(BM_AddShare);