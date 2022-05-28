#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <cinttypes>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "config.hpp"
#include "logger.hpp"
#include "share.hpp"

#define STALE_SHARE_DIFF -1
#define INVALID_SHARE_DIFF -2

#define STATS_INTERVAL_SECONDS (60 * 5)
#define ROUND_INTERVAL_SECONDS 10

struct WorkerStats
{
    inline WorkerStats()
        : round_effort(0),
          interval_effort(0),
          interval_valid_shares(0),
          interval_invalid_shares(0),
          interval_stale_shares(0)
    {
    }

    // double round_effort;
    // chain -> effort
    std::unordered_map<std::string, double> round_effort;
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

// beautiful!
class StatsManager
{
   public:
    StatsManager(redisContext* rc, std::mutex* rc_mutex);

    // Every stats_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    // ) staker points
    void Start();
    void LoadCurrentRound();
    void AddShare(std::string worker_full, std::string miner_addr, double diff);
    bool ClosePoWRound(std::string chain, BlockSubmission& submission,
                       bool accepted, double fee);
    double GetTotalEffort(std::string chain);

   private:
    redisContext* rc;
    std::mutex* rc_mutex;

    std::mutex stats_map_mutex;
    // worker -> stats
    std::unordered_map<std::string, WorkerStats> worker_stats_map;
    // miner -> stats
    std::unordered_map<std::string, WorkerStats> miner_stats_map;
    // chain -> effort
    std::unordered_map<std::string, double> total_round_effort;
};

#endif
// TODO: on crash load all the stats from redis (round hash)