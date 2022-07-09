#ifndef STATS_HPP
#define STATS_HPP
#include <unordered_map>

#include "round.hpp"

struct StringHash
{
    using is_transparent = void;  // enables heterogenous lookup

    std::size_t operator()(std::string_view sv) const
    {
        std::hash<std::string_view> hasher;
        return hasher(sv);
    }
};

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
    std::unordered_map<std::string, double, StringHash, std::less<>>
        round_effort_map;
    uint32_t worker_count = 0;

    inline void ResetEffort() { round_effort_map.clear(); }
};

typedef std::unordered_map<std::string, WorkerStats, StringHash, std::less<>>
    worker_map;
typedef std::unordered_map<std::string, MinerStats, StringHash, std::less<>>
    miner_map;

typedef std::unordered_map<std::string, Round, StringHash, std::less<>>
    round_map;

#endif