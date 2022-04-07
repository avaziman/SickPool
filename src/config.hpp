#ifndef CONFIG_HPP_
#define CONFIG_HPP_
// These properties need to be defined in compile time for efficiency!
#define COIN_VRSCTEST 1
#define DB_RETENTION 86400000  // 1 day, (1000 * 60 * 60 * 24)
#define HASHRATE_PERIOD 300000    // 5 minutes, (1000 * 60 * 5)
#define MAX_WORKER_NAME_LEN 128
#define MAX_BLOCK_SIZE 2000000 // bytes
#define BLOCK_HEADER_SIZE (140 + 3 + 1344)

#define POOL_COIN COIN_VRSCTEST
#define BLOCK_MATURITY 100

#if POOL_COIN == COIN_VRSCTEST
#define COIN_SYMBOL "VRSCTEST"
// target: 0x0f0f0f0000000000000000000000000000000000000000000000000000000000
#define DIFF1_BITS 0x200f0f0f
// #elif POOL_COIN == COIN_VRSC
// #define COIN_SYMBOL "VRSC"
// #define DIFF1_BITS 0x200f0f0f
#endif

#endif // CONFIG_HPP_