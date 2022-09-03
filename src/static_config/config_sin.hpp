#include "hash_algo.hpp"

#define STRATUM_PROTOCOL_BTC 1

// diff 1
// static constexpr uint32_t DIFF1_BITS = 0x1f00ffff;
static constexpr uint32_t DIFF1_BITS = 0x1d00ffff;

// in bytes
static constexpr uint32_t MAX_BLOCK_SIZE = 64000000;
static constexpr uint32_t HASH_SIZE = 32;
static constexpr uint32_t MAX_NOTIFY_MESSAGE_SIZE = (1024 * 4);
static constexpr bool MULTI_CHAIN = false;

// in seconds
static constexpr uint32_t MAX_FUTURE_BLOCK_TIME = 60;
static constexpr uint32_t BLOCK_TIME = 60;

static constexpr uint32_t COINBASE_MATURITY = 20;

static constexpr HashAlgo HASH_ALGO = HashAlgo::X25X;

// tx
static constexpr uint32_t TXVERSION = 2;

static constexpr uint32_t ADDRESS_LEN = 34;
static constexpr char ADDRESS_PREFIX = 'S';

// encoded as in block header
static constexpr uint32_t BLOCK_VERSION = 0x20000000;
static constexpr uint32_t VERSION_SIZE = 4;
static constexpr uint32_t TIME_SIZE = 4;
static constexpr uint32_t BITS_SIZE = 4;
static constexpr uint32_t PREVHASH_SIZE = HASH_SIZE;
static constexpr uint32_t MERKLE_ROOT_SIZE = HASH_SIZE;
static constexpr uint32_t NONCE_SIZE = 4;

static constexpr uint32_t BLOCK_HEADER_SIZE = VERSION_SIZE + PREVHASH_SIZE +
                                              MERKLE_ROOT_SIZE + TIME_SIZE +
                                              BITS_SIZE + NONCE_SIZE;

// not worth it to add up to bits as we copy 2 times more for 4 bytes
static constexpr uint32_t BLOCK_HEADER_STATIC_SIZE =
    VERSION_SIZE + PREVHASH_SIZE;

static constexpr uint32_t EXTRANONCE2_SIZE = 4;
