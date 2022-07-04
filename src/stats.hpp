#ifndef STATS_HPP
#define STATS_HPP
#include <unordered_map>
#include "round.hpp"

enum class BadDiff
{
    STALE_SHARE_DIFF = -1,
    INVALID_SHARE_DIFF = -2
};

struct WorkerStats
{
    double average_hashrate_sum = 0.f;
    double average_hashrate = 0.f;
    double interval_hashrate = 0.f;
    double current_interval_effort = 0.f;
    uint32_t interval_valid_shares = 0;
    uint32_t interval_stale_shares = 0;
    uint32_t interval_invalid_shares = 0;
    uint32_t connection_count = 0;

    inline void ResetInterval()
    {
        this->current_interval_effort = 0;
        this->interval_valid_shares = 0;
        this->interval_stale_shares = 0;
        this->interval_invalid_shares = 0;
    }
};

struct MinerStats : public WorkerStats
{
    std::unordered_map<std::string_view, Round> round_effort;
    uint32_t worker_count = 0;
};

#endif