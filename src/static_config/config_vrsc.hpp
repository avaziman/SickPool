#ifndef VRSC_CONFIG_HPP_
#define VRSC_CONFIG_HPP_
#include "crypto/verushash/arith_uint256.h"
#include "crypto/verushash/uint256.h"
#include "hash_algo.hpp"
#include "static_config.hpp"
#define ZANO_ALIAS_NAME_MAX_LEN 255

static constexpr std::string_view target_vrsc_sv =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

struct CoinConstantsZec
{

static constexpr uint32_t DIFF1_BITS = 0x200f0f0f;
static constexpr uint32_t MAX_BLOCK_SIZE = 2000000;
#ifndef HASH_SIZE
static constexpr uint32_t HASH_SIZE = 32;
#endif
static constexpr uint32_t VERSION_SIZE = 4;
static constexpr uint32_t PREVHASH_SIZE = HASH_SIZE;
static constexpr uint32_t MERKLE_ROOT_SIZE = HASH_SIZE;
static constexpr uint32_t FINALSROOT_SIZE = HASH_SIZE;
static constexpr uint32_t NONCE_SIZE = HASH_SIZE;
static constexpr uint32_t BITS_SIZE = 4;
static constexpr uint32_t TIME_SIZE = 4;

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

static constexpr uint32_t EXTRANONCE2_SIZE = NONCE_SIZE - StratumConstants::EXTRANONCE_SIZE;

static constexpr uint32_t BLOCK_VERSION = 0x04000100;
static constexpr uint32_t BLOCK_HEADER_SIZE = (140 + 3 + 1344);
};  // namespace CoinConstantsVrsc

constexpr StaticConf VrscStatic = {
    .COIN_SYMBOL = Coin::VRSC,
    .HASH_ALGO = HashAlgo::VERUSHASH_V2b2,
    .STRATUM_PROTOCOL = StratumProtocol::ZEC,
    .DIFF1 = HexToDouble<target_vrsc_sv>(),
    .MAX_BLOCK_SIZE = 2000000,
    .BLOCK_HASH_SIZE = 32,
    .PREVHASH_SIZE = 32,
    .MERKLE_ROOT_SIZE = 32,
    .MAX_FUTURE_BLOCK_TIME = 60,
    .BLOCK_TIME = 60 * 2,
    .ADDRESS_LEN = 34,
    .BLOCK_HEADER_SIZE = CoinConstantsZec::BLOCK_HEADER_SIZE,
    .BLOCK_HEADER_STATIC_SIZE = 36,  // VERSION_SIZE + PREVHASH_SIZE
    .COINBASE_MATURITY = 100};

#endif