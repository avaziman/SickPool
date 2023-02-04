#ifndef STATIC_CONF_HPP_
#define STATIC_CONF_HPP_
#include "crypto/verushash/arith_uint256.h"
#include "constants.hpp"
#include "hash_algo.hpp"

// #define BTC 1
#define PAYMENT_SCHEME PAYMENT_SCHEME_PPLNS

struct StaticConf{
    const Coin COIN_SYMBOL;
    const HashAlgo HASH_ALGO;
    const StratumProtocol STRATUM_PROTOCOL;
    const double DIFF1;

    uint32_t MAX_BLOCK_SIZE;
    uint32_t BLOCK_HASH_SIZE;
    uint32_t PREVHASH_SIZE;
    uint32_t MERKLE_ROOT_SIZE;
    uint32_t MAX_FUTURE_BLOCK_TIME;
    uint32_t BLOCK_TIME;
    uint32_t ADDRESS_LEN;
    uint32_t BLOCK_HEADER_SIZE;
    uint32_t BLOCK_HEADER_STATIC_SIZE;
    uint32_t COINBASE_MATURITY;
};

// SICK_COIN derived
// static constexpr uint32_t DIFF1_COEFFICIENT = DIFF1_BITS & 0x00ffffff;
// static constexpr uint32_t DIFF1_EXPONENT = DIFF1_BITS >> 24;
#endif