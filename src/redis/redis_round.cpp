#include "redis_manager.hpp"

int64_t RedisManager::GetRoundTimePow(std::string_view chain)
{
    std::scoped_lock lock(rc_mutex);

    return hgeti(fmt::format("round:pow:{}", chain), "start");
}

double RedisManager::GetRoundEffortPow(std::string_view chain)
{
    std::scoped_lock lock(rc_mutex);

    return hgetd(fmt::format("round:pow:{}", chain), "total_effort");
}

bool RedisManager::SetRoundEstimatedEffort(std::string_view chain,
                                           double effort)
{
    std::scoped_lock lock(rc_mutex);

    return hset(fmt::format("round:pow:{}", chain), "estimated",
                std::to_string(effort));
}

bool RedisManager::SetRoundTimePow(std::string_view chain, int64_t val)
{
    std::scoped_lock lock(rc_mutex);

    return hset(fmt::format("round:pow:{}", chain), "start",
                std::to_string(val));
}
bool RedisManager::SetRoundEffortPow(std::string_view chain, double effort)
{
    std::scoped_lock lock(rc_mutex);

    return hset(fmt::format("round:pow:{}", chain), "total_effort",
                std::to_string(effort));
}