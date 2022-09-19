#ifndef STATIC_CONF_HPP_
#define STATIC_CONF_HPP_

#include "constants.hpp"
using namespace StratumConstants;

#define BTC 1
#define ZCASH 2
#define VRSC 3
#define VRSCTEST 4
#define SIN 5

#define COIN SIN

#if COIN == VRSC
#define COIN_SYMBOL "VRSC"
#include "config_vrsc.hpp"
using namespace CoinConstantsVrsc;
#elif COIN == VRSCTEST
#define COIN_SYMBOL "VRSCTESTLOCAL"
#include "config_vrsc.hpp"
using namespace CoinConstantsVrsc;
#elif COIN == SIN
#include "config_sin.hpp"
using namespace CoinConstantsSin;
#else
#error "No coin selected"
#endif

// coin derived
static constexpr uint32_t DIFF1_COEFFICIENT = DIFF1_BITS & 0x00ffffff;
static constexpr uint32_t DIFF1_EXPONENT = DIFF1_BITS >> 24;
static constexpr uint32_t HASH_SIZE_HEX = HASH_SIZE * 2;
#endif