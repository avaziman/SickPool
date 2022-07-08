#define COIN_VRSC 1
#define COIN_VRSCTEST 2

#define POOL_COIN COIN_VRSCTEST

#if POOL_COIN == COIN_VRSC
#define COIN_SYMBOL "VRSC"
#include "config_vrsc.hpp"
#elif POOL_COIN == COIN_VRSCTEST
#define COIN_SYMBOL "VRSCTEST"
#include "config_vrsc.hpp"
#endif