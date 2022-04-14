#ifndef CONFIG_HPP_
#define CONFIG_HPP_
// These properties need to be defined in compile time for efficiency!
#define COIN_VRSCTEST 1
#define COIN_VRSC 2

#define DB_RETENTION 86400000  // 1 day, (1000 * 60 * 60 * 24)
#define HASHRATE_PERIOD 300000    // 5 minutes, (1000 * 60 * 5)

#define MAX_WORKER_NAME_LEN 128

#define POOL_COIN COIN_VRSCTEST

/**
 * Maximum amount of time that a block timestamp is allowed to exceed the
 * current network-adjusted time before the block will be accepted.
 */
#define MAX_FUTURE_BLOCK_TIME 60  // minute in to the future

#if POOL_COIN <= COIN_VRSC
// target: 0x0f0f0f0000000000000000000000000000000000000000000000000000000000
#define DIFF1_BITS 0x200f0f0f
#define MAX_BLOCK_SIZE 2000000  // bytes
#define BLOCK_HEADER_SIZE (140 + 3 + 1344)
#define BLOCK_MATURITY 100
#define HASH_SIZE 32

#define MAX_NOTIFY_MESSAGE_SIZE (1024 * 4)

#endif

#if POOL_COIN == COIN_VRSCTEST
#define COIN_SYMBOL "VRSCTEST"

#elif POOL_COIN == COIN_VRSC
#define COIN_SYMBOL "VRSC"
#endif

#endif // CONFIG_HPP_