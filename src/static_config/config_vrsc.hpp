#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#define MAX_WORKER_NAME_LEN 16
#define ADDRESS_LEN 34

#define POW_ALGO_VERUSHASH 1

/**
 * Maximum amount of time that a block timestamp is allowed to exceed the
 * current network-adjusted time before the block will be accepted.
 */
#define MAX_FUTURE_BLOCK_TIME 60  // minute in to the future

#define TXVERSION_GROUP 0x892f2085
#define TXVERSION 4

#define POW_ALGO POW_ALGO_VERUSHASH
// target: 0x0f0f0f0000000000000000000000000000000000000000000000000000000000
#define DIFF1_BITS 0x200f0f0f
#define MAX_BLOCK_SIZE 2000000  // bytes
#define BLOCK_HEADER_SIZE (140 + 3 + 1344)
#define HASH_SIZE 32
#define HASH_SIZE_HEX (HASH_SIZE * 2)
#define POW_BLOCK_TIME 60
#define MAX_NOTIFY_MESSAGE_SIZE (1024 * 4)

#define DEBUG 1
#ifndef DEBUG
#define BLOCK_MATURITY 100
#else
#define BLOCK_MATURITY 0
#endif

#endif // CONFIG_HPP_