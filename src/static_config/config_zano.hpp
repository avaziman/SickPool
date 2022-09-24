#include "hash_algo.hpp"

#define STRATUM_PROTOCOL_BTC 1
#define HASH_ALGO HASH_ALGO_PROGPOW
#define COIN_SYMBOL "ZANO"

using namespace CoinConstantsCryptoNote;

namespace CoinConstantsZano
{
// 0xffffff0000000000000000000000000000000000000000000000000000000000
static constexpr uint32_t DIFF1_BITS = 0x20ffffff;
static constexpr uint32_t MAX_BLOCK_SIZE = 64000000;
static constexpr uint32_t HASH_SIZE = 32;
static constexpr uint32_t PREVHASH_SIZE = HASH_SIZE;
static constexpr uint32_t MERKLE_ROOT_SIZE = HASH_SIZE;

// in seconds
static constexpr uint32_t MAX_FUTURE_BLOCK_TIME = 60;
static constexpr uint32_t BLOCK_TIME = 60;

static constexpr uint32_t ADDRESS_LEN = 34;
static constexpr char ADDRESS_PREFIX = 'S';

// tx
static constexpr uint32_t TXVERSION = 2;
static constexpr uint32_t COINBASE_MATURITY = 20;

// encoded as in block header
static constexpr uint32_t BLOCK_VERSION = 0x20000000;

static constexpr uint32_t BLOCK_HEADER_SIZE = VERSION_SIZE + PREVHASH_SIZE +
                                              MERKLE_ROOT_SIZE + TIME_SIZE +
                                              BITS_SIZE + NONCE_SIZE;

static constexpr uint32_t BLOCK_HEADER_STATIC_SIZE =
    VERSION_SIZE + PREVHASH_SIZE;
};  // namespace CoinConstantSin