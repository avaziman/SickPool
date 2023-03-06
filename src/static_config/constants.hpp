#ifndef CONSTANTS_HPP_
#define CONSTANTS_HPP_
#include <simdjson/simdjson.h>

#include <algorithm>
#include <cmath>
#include "utils/hex_utils.hpp"
#define HASH_SIZE_HEX (2 * HASH_SIZE)

// the default
namespace CoinConstantsBtc
{
static constexpr uint32_t VERSION_SIZE = 4;
static constexpr uint32_t TIME_SIZE = 4;
static constexpr uint32_t BITS_SIZE = 4;
static constexpr uint32_t NONCE_SIZE = 4;
static constexpr uint32_t EXTRANONCE2_SIZE = 4;

};  // namespace CoinConstants


struct CoinConstantsCryptoNote
{
static constexpr uint32_t VERSION_SIZE = 2;
static constexpr uint32_t TIME_SIZE = 8;
static constexpr uint32_t NONCE_SIZE = 8;
};  // namespace CoinConstants

struct ServerConstants
{
static constexpr uint32_t MAX_CONNECTIONS_QUEUE = 64;
static constexpr uint32_t MAX_CONNECTION_EVENTS = 32;
static constexpr uint32_t REQ_BUFF_SIZE = 1024 * 24;
static constexpr uint32_t REQ_BUFF_SIZE_REAL =
    ServerConstants::REQ_BUFF_SIZE - simdjson::SIMDJSON_PADDING;
static constexpr uint32_t EPOLL_TIMEOUT = 1000;  // ms
};

struct StratumConstants
{
// in bytes
static constexpr uint32_t MAX_NOTIFY_MESSAGE_SIZE = (1024 * 4);
static constexpr uint32_t MAX_WORKER_NAME_LEN = 16;
static constexpr uint32_t EXTRANONCE_SIZE = 4;
static constexpr uint32_t JOBID_SIZE = 4;

static constexpr uint32_t HTTP_REQ_ALLOCATE = 16 * 1024;
static constexpr uint32_t MAX_HTTP_JSON_DEPTH = 3;

};  // namespace StratumConstants


#endif