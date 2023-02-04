#ifndef ZANO_CONFIG_HPP_
#define ZANO_CONFIG_HPP_
#include "crypto/verushash/arith_uint256.h"
#include "crypto/verushash/uint256.h"
#include "hash_algo.hpp"
#include "static_config.hpp"
#define ZANO_ALIAS_NAME_MAX_LEN 255

static constexpr std::string_view target_zano_sv =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

constexpr StaticConf ZanoStatic = {
    .COIN_SYMBOL = Coin::ZANO,
    .HASH_ALGO = HashAlgo::PROGPOWZ,
    .STRATUM_PROTOCOL = StratumProtocol::CN,
    .DIFF1 = HexToDouble<target_zano_sv>(),
    .MAX_BLOCK_SIZE = 2000000,
    .BLOCK_HASH_SIZE = 32,
    .PREVHASH_SIZE = 32,
    .MERKLE_ROOT_SIZE = 32,
    .MAX_FUTURE_BLOCK_TIME = 60,
    .BLOCK_TIME = 60 * 2,
    .ADDRESS_LEN = 34,
    .BLOCK_HEADER_SIZE = 81,
    .BLOCK_HEADER_STATIC_SIZE = 36,  // VERSION_SIZE + PREVHASH_SIZE
    .COINBASE_MATURITY = 10};

#endif