#include <benchmark/benchmark.h>
#include "payment_manager.hpp"

// static void BM_Snprintf(benchmark::State& state)
// {
//     char response[1024];
//     for (auto _ : state)
//     {
//         int len =
//             snprintf(response, sizeof(response),
//                      "{\"id\":%d,\"result\":[null,\"%s\"],\"error\":null}\n",
//                      1, "00000000");
//     }
// }


// static void BM_tx1(benchmark::State& state)
// {
//     for (auto _ : state)
//     {
//         PaymentManager payment_manager(0, 0);

//         AgingBlock aged_block;
//         aged_block.id = 4;
//         aged_block.matued_at_ms = 0;
//         memset(aged_block.coinbase_txid, 17, sizeof(aged_block.coinbase_txid));

//         reward_map_t rewards = {{"RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC", 1e8}};
//         payment_manager.AppendAgedRewards(aged_block, rewards);

//         std::vector<uint8_t> bytes;
//         payment_manager.tx.GetBytes(bytes);
//     }
// }
// BENCHMARK(BM_tx1);
