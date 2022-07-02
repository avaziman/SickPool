#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <chrono>
#include <cinttypes>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "block_submission.hpp"
#include "logger.hpp"
#include "round.hpp"
#include "share.hpp"
#include "static_config/config.hpp"

enum class BadDiff
{
    STALE_SHARE_DIFF = -1,
    INVALID_SHARE_DIFF = -2
};

struct WorkerStats
{
    double average_hashrate_sum = 0.f;
    double interval_effort = 0.f;
    uint32_t interval_valid_shares = 0;
    uint32_t interval_stale_shares = 0;
    uint32_t interval_invalid_shares = 0;
    uint32_t connection_count = 0;

    inline void ResetInterval()
    {
        this->interval_effort = 0;
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

// beautiful!
class StatsManager
{
   public:
    StatsManager(redisContext* rc, std::mutex* rc_mutex, int hr_interval,
                 int effort_interval, int avg_hr_interval, int hashrate_ttl);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    // ) staker points
    void Start();
    bool LoadCurrentRound();
    void AddShare(std::string_view worker_full, std::string_view miner_addr,
                  double diff);
    bool AddWorker(std::string_view address, std::string_view worker_full,
                   std::time_t curtime);
    void PopWorker(std::string_view worker, std::string_view address);

    bool ClosePoWRound(std::string_view chain,
                       const BlockSubmission& submission, double fee);

    bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    bool UpdateStats(bool update_effort, bool update_hr,
                     int64_t update_time_ms);

    int AppendStatsUpdate(std::string_view addr, std::string_view prefix,
                          int64_t update_time_ms, double hr,
                          const WorkerStats& ws);
    int AppendCreateStatsTs(std::string_view addr, std::string_view prefix);

    Round GetChainRound(std::string_view chain);

   private:
    redisContext* rc;
    std::mutex* rc_mutex;

    const int hashrate_interval_seconds;
    const int effort_interval_seconds;
    const int average_hashrate_interval_seconds;
    const int hashrate_ttl_seconds;

    std::mutex stats_map_mutex;
    // worker -> stats
    std::unordered_map<std::string_view, WorkerStats> worker_stats_map;
    // miner -> stats
    std::unordered_map<std::string_view, MinerStats> miner_stats_map;
    // chain -> effort
    std::unordered_map<std::string_view, Round> round_map;
};

#endif
// TODO: on crash load all the stats from redis (round hash)
// TODO: add another unordered map for stakers (non miners too!)