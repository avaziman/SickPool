#include "hash_algo.hpp"
#define STRATUM_PROTOCOL_ZEC 1

static constexpr uint32_t DIFF1_BITS = 0x200f0f0f;

// in bytes
static constexpr uint32_t MAX_BLOCK_SIZE = 2000000;
static constexpr uint32_t BLOCK_HEADER_SIZE = (140 + 3 + 1344);
static constexpr uint32_t HASH_SIZE = 32;
static constexpr uint32_t MAX_NOTIFY_MESSAGE_SIZE = (1024 * 4);

// in seconds
static constexpr uint32_t MAX_FUTURE_BLOCK_TIME = 60;

static constexpr uint32_t COINBASE_MATURITY = 100;

static constexpr HashAlgo HASH_ALGO = HashAlgo::VERUSHASH_V2b2;

// tx
static constexpr uint32_t TXVERSION_GROUP = 0x892f2085;
static constexpr uint32_t TXVERSION = 4;

static constexpr uint32_t ADDRESS_LEN = 34;
static constexpr char ADDRESS_PREFIX = 'R';

// encoded as in block header
static constexpr uint32_t BLOCK_VERSION = 0x04000100;
static constexpr uint32_t VERSION_SIZE = 4;
static constexpr uint32_t TIME_SIZE = 4;
static constexpr uint32_t BITS_SIZE = 4;
static constexpr uint32_t PREVHASH_SIZE = HASH_SIZE;
static constexpr uint32_t MERKLE_ROOT_SIZE = HASH_SIZE;
static constexpr uint32_t NONCE_SIZE = HASH_SIZE;

static constexpr uint32_t EXTRANONCE2_SIZE = NONCE_SIZE - EXTRANONCE_SIZE;

static constexpr uint32_t FINALSROOT_SIZE = HASH_SIZE;
static constexpr uint32_t SOLUTION_SIZE = 1344;
static constexpr uint32_t SOLUTION_LENGTH_SIZE = 3;

static constexpr uint32_t BLOCK_HEADER_STATIC_SIZE =
    VERSION_SIZE       /* version */
    + PREVHASH_SIZE    /* prevhash */
    + MERKLE_ROOT_SIZE /* merkle_root */
    + FINALSROOT_SIZE  /* final sapling root */
    + TIME_SIZE        /* time, not static but we override */
    + BITS_SIZE /* bits */;