#ifndef STATIC_CONF_HPP_
#define STATIC_CONF_HPP_
#include "crypto/verushash/arith_uint256.h"
#include "constants.hpp"
using namespace StratumConstants;

// #define BTC 1
#define ZCASH 2
#define VRSC 3
#define VRSCTEST 4
#define SIN 5

#define SICK_COIN ZANO
#define PAYMENT_SCHEME PAYMENT_SCHEME_PPLNS
#include "hash_algo.hpp"
struct StaticConf{
    const Coin COIN_SYMBOL;
    const HashAlgo HASH_ALGO;
    const StratumProtocol STRATUM_PROTOCOL;
    const uint256 DIFF1_TARGET;
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
// const double HASH_MULTIPLIER;  // how many hashes is a share of difficulty 1

#if SICK_COIN == VRSC
#define SICK_COIN_SYMBOL "VRSC"
#include "config_vrsc.hpp"
using namespace CoinConstantsVrsc;
#elif SICK_COIN == VRSCTEST
#define SICK_COIN_SYMBOL "VRSCTESTLOCAL"
#include "config_vrsc.hpp"
using namespace CoinConstantsVrsc;
#elif SICK_COIN == SIN
#include "config_sin.hpp"
using namespace CoinConstantsSin;
#elif SICK_COIN == ZANO
#include "config_zano.hpp"
// using namespace CoinConstantsZano;
#else
#error "No SICK_COIN selected"
#endif
// SICK_COIN derived
// static constexpr uint32_t DIFF1_COEFFICIENT = DIFF1_BITS & 0x00ffffff;
// static constexpr uint32_t DIFF1_EXPONENT = DIFF1_BITS >> 24;
#endif