#include "redis_round.hpp"
using enum Prefix;

uint32_t RedisManager::GetBlockNumber()
{
    auto reply = Command({"GET", key_names.block_number});
    if (reply->type != REDIS_REPLY_STRING) return 0;

    return std::strtol(reply->str, nullptr, 10);
}