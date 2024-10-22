#ifndef STATS_HPP
#define STATS_HPP
#include <fmt/core.h>

#include <list>
#include <unordered_map>

#include "redis_interop.hpp"
#include "round.hpp"

struct NetworkStats
{
    double network_hr;
    double difficulty;
};

struct WorkerStats
{
    // derived from the below
    double average_hashrate = 0.0;
    double interval_hashrate = 0.0;

    uint32_t interval_valid_shares = 0;
    uint32_t interval_stale_shares = 0;
    uint32_t interval_invalid_shares = 0;

    double average_hashrate_sum = 0.0;
    double current_interval_effort = 0.0;

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
    uint32_t worker_count = 0;
};

using MinerId = uint32_t;
using WorkerId = uint32_t;
struct FullId
{
    MinerId miner_id;
    WorkerId worker_id;
};

#pragma pack(push, 1)
struct Share
{
    const MinerId miner_id;
    double diff;
};
#pragma pack(pop)

using worker_map = std::list<std::pair<FullId, WorkerStats>>;

using miner_map = std::unordered_map<MinerId, MinerStats>;

// miner -> effort
using efforts_map_t = std::unordered_map<MinerId, double>;

/* block submission attributes are
    sortable:
    time/number
    reward
    difficulty
    effort
    duration
    (no need height because it will always be grouped by chains, so you can just
   filter by chain and sort by time/number).

   non-sortable (filterable):

    chain
    type (pow/pos)
    solver address
*/

#endif