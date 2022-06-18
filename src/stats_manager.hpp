#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <string_view>
#include <cinttypes>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "round.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "share.hpp"

#define STALE_SHARE_DIFF -1
#define INVALID_SHARE_DIFF -2

// #define STATS_INTERVAL_SECONDS (60 * 5)

struct WorkerStats
{
    WorkerStats()
        : interval_effort(0),
          interval_valid_shares(0),
          interval_invalid_shares(0),
          interval_stale_shares(0)
    {
    }

    // double round_effort;
    // chain -> effort
    // std::unordered_map<std::string, double> round_effort_pow;

    double interval_effort;
    uint32_t interval_valid_shares;
    uint32_t interval_invalid_shares;
    uint32_t interval_stale_shares;

    inline void ResetInterval()
    {
        this->interval_effort = 0;
        this->interval_invalid_shares = 0;
        this->interval_stale_shares = 0;
        this->interval_stale_shares = 0;
    }
};

struct MinerStats : public WorkerStats
{
    MinerStats() : WorkerStats(), round_effort() {}
    std::unordered_map<std::string_view, ChainEffort> round_effort;
};

// beautiful!
class StatsManager
{
   public:
    StatsManager(redisContext* rc, std::mutex* rc_mutex, int hr_interval,
                 int effort_interval);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    // ) staker points
    void Start();
    void LoadCurrentRound();
    void AddShare(std::string_view worker_full, std::string_view miner_addr, double diff);
    bool ClosePoWRound(std::string_view chain, const BlockSubmission& submission,
                       bool accepted, double fee);

    bool AppendPoSBalances(std::string_view chain, int64_t from_ms);\
    std::unordered_map<std::string_view, MinerStats>::iterator LoadMinerStats(
        std::string_view miner_addr);

    double GetTotalEffort(std::string_view chain);

   private:
    const int hashrate_interval_seconds;
    const int effort_interval_seconds;

    redisContext* rc;
    std::mutex* rc_mutex;

    std::mutex stats_map_mutex;
    // worker -> stats
    std::unordered_map<std::string_view, WorkerStats> worker_stats_map;
    // miner -> stats
    std::unordered_map<std::string_view, MinerStats> miner_stats_map;
    // chain -> effort
    std::unordered_map<std::string_view, ChainEffort> total_round_effort;
};

#endif
// TODO: on crash load all the stats from redis (round hash)
// TODO: add another unordered map for stakers (non miners too!)