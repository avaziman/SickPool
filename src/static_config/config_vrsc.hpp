#include "hash_algo.hpp"

#define STRATUM_PROTOCOL_ZEC 1
#define HASH_ALGO HASH_ALGO_VERUSHASH

namespace CoinConstantsZec
{

static constexpr uint32_t DIFF1_BITS = 0x200f0f0f;
static constexpr uint32_t MAX_BLOCK_SIZE = 2000000;
// static constexpr uint32_t HASH_SIZE = 32;
static constexpr uint32_t PREVHASH_SIZE = HASH_SIZE;
static constexpr uint32_t MERKLE_ROOT_SIZE = HASH_SIZE;
static constexpr uint32_t FINALSROOT_SIZE = HASH_SIZE;
static constexpr uint32_t NONCE_SIZE = HASH_SIZE;

static constexpr uint32_t SOLUTION_SIZE = 1344;
static constexpr uint32_t SOLUTION_LENGTH_SIZE = 3;

// in seconds
static constexpr uint32_t MAX_FUTURE_BLOCK_TIME = 60;
static constexpr uint32_t BLOCK_TIME = 60;

static constexpr uint32_t ADDRESS_LEN = 34;
static constexpr char ADDRESS_PREFIX = 'R';
// tx
static constexpr uint32_t COINBASE_MATURITY = 100;
static constexpr uint32_t TXVERSION_GROUP = 0x892f2085;
static constexpr uint32_t TXVERSION = 4;
static constexpr bool TXOVERWINTERED = true;
static constexpr uint32_t TXVERSION_HEADER = TXVERSION | (TXOVERWINTERED << 31);

static constexpr uint32_t EXTRANONCE2_SIZE = NONCE_SIZE - EXTRANONCE_SIZE;

// encoded as in block header
static constexpr uint32_t BLOCK_VERSION = 0x04000100;
static constexpr uint32_t BLOCK_HEADER_SIZE = (140 + 3 + 1344);

// static constexpr uint32_t BLOCK_HEADER_STATIC_SIZE =
//     VERSION_SIZE + PREVHASH_SIZE + MERKLE_ROOT_SIZE + FINALSROOT_SIZE +
//     TIME_SIZE /* time, not static but we override */
//     + BITS_SIZE;
};  // namespace CoinConstantsVrsc
