#ifndef STATIC_CONF_HPP_
#define STATIC_CONF_HPP_

#include "constants.hpp"
using namespace StratumConstants;

#define BTC 1
#define ZCASH 2
#define VRSC 3
#define VRSCTEST 4
#define SIN 5
#define ZANO 6

#define SICK_COIN ZANO
#define PAYMENT_SCHEME PAYMENT_SCHEME_PPLNS

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
using namespace CoinConstantsZano;
#else
#error "No SICK_COIN selected"
#endif

// SICK_COIN derived
static constexpr uint32_t DIFF1_COEFFICIENT = DIFF1_BITS & 0x00ffffff;
static constexpr uint32_t DIFF1_EXPONENT = DIFF1_BITS >> 24;

static constexpr uint32_t HASH_SIZE_HEX = HASH_SIZE * 2;
#endif