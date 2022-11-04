#include <simdjson/simdjson.h>

// the default
namespace CoinConstantsBtc
{
static constexpr uint32_t VERSION_SIZE = 4;
static constexpr uint32_t TIME_SIZE = 4;
static constexpr uint32_t BITS_SIZE = 4;
static constexpr uint32_t NONCE_SIZE = 4;
};  // namespace CoinConstants


namespace CoinConstantsCryptoNote
{
static constexpr uint32_t VERSION_SIZE = 2;
static constexpr uint32_t TIME_SIZE = 8;
static constexpr uint32_t NONCE_SIZE = 8;
};  // namespace CoinConstants

namespace StratumConstants
{
// in bytes
static constexpr uint32_t MAX_NOTIFY_MESSAGE_SIZE = (1024 * 4);
static constexpr uint32_t MAX_WORKER_NAME_LEN = 16;
static constexpr uint32_t EXTRANONCE_SIZE = 4;
static constexpr uint32_t JOBID_SIZE = 4;

static constexpr uint32_t HTTP_REQ_ALLOCATE = 16 * 1024;
static constexpr uint32_t MAX_HTTP_JSON_DEPTH = 3;

static constexpr uint32_t REQ_BUFF_SIZE = 1024 * 5;
static constexpr uint32_t REQ_BUFF_SIZE_REAL =
    REQ_BUFF_SIZE - simdjson::SIMDJSON_PADDING;
static constexpr uint32_t MAX_CONNECTION_EVENTS = 32;
static constexpr uint32_t EPOLL_TIMEOUT = 10000;  // ms
static constexpr uint32_t MAX_CONNECTIONS_QUEUE = 64;

};  // namespace StratumConstants