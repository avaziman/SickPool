#ifndef STATIC_CONF_HPP_
#define STATIC_CONF_HPP_

#include <simdjson/simdjson.h>

#define BTC 1
#define ZCASH 2
#define VRSC 3
#define VRSCTEST 4
#define SIN 5

#define COIN VRSC

// general for pool
static constexpr uint32_t MAX_WORKER_NAME_LEN = 16;
static constexpr uint32_t EXTRANONCE_SIZE = 4;
static constexpr uint32_t JOBID_SIZE = 4;

static constexpr uint32_t HTTP_REQ_ALLOCATE = 16 * 1024;
static constexpr uint32_t MAX_HTTP_JSON_DEPTH = 3;

static constexpr uint32_t REQ_BUFF_SIZE = 1024 * 5;
static constexpr uint32_t REQ_BUFF_SIZE_REAL =
    REQ_BUFF_SIZE - simdjson::SIMDJSON_PADDING;
static constexpr uint32_t MAX_CONNECTION_EVENTS = 10;
static constexpr uint32_t EPOLL_TIMEOUT = 1000; // ms
static constexpr uint32_t MAX_CONNECTIONS_QUEUE = 64;

#if COIN == VRSC
#define COIN_SYMBOL "VRSC"
#include "config_vrsc.hpp"
#elif COIN == VRSCTEST
#define COIN_SYMBOL "VRSCTESTLOCAL"
#include "config_vrsc.hpp"
#elif COIN == SIN
#define COIN_SYMBOL "SIN"
#include "config_sin.hpp"
#else
#error "No coin selected"
#endif

// coin derived
static constexpr uint32_t DIFF1_COEFFICIENT = DIFF1_BITS & 0x00ffffff;
static constexpr uint32_t DIFF1_EXPONENT = DIFF1_BITS >> 24;

static constexpr uint32_t HASH_SIZE_HEX = HASH_SIZE * 2;


// static constexpr uint32_t VERSION_SIZE = 4;
// static constexpr uint32_t TIME_SIZE = 4;
// static constexpr uint32_t BITS_SIZE = 4;
// static constexpr uint32_t PREVHASH_SIZE = HASH_SIZE;
// static constexpr uint32_t MERKLE_ROOT_SIZE = HASH_SIZE;
// static constexpr uint32_t FINALSROOT_SIZE = HASH_SIZE;
// static constexpr uint32_t NONCE_SIZE = HASH_SIZE;
// static constexpr uint32_t SOLUTION_SIZE = 1344;
// static constexpr uint32_t SOLUTION_LENGTH_SIZE = 3;
// static constexpr uint32_t NONCE2_SIZE = (NONCE_SIZE - EXTRANONCE_SIZE);

#endif