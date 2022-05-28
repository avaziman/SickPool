#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <cinttypes>
#include <thread>
#include <unordered_map>

#include "logger.hpp"

#define STALE_SHARE_DIFF -1
#define INVALID_SHARE_DIFF -2

#define STATS_INTERVAL_SECONDS (60 * 5)
#define ROUND_INTERVAL_SECONDS 10

struct WorkerStats
{
    WorkerStats()
        : round_effort(0),
          interval_effort(0),
          interval_valid_shares(0),
          interval_invalid_shares(0)
    {
    }

    double round_effort;
    double interval_effort;
    uint32_t interval_valid_shares;
    uint32_t interval_invalid_shares;
    uint32_t interval_stale_shares;

    void ResetInterval()
    {
        this->interval_effort = 0;
        this->interval_invalid_shares = 0;
        this->interval_stale_shares = 0;
        this->interval_stale_shares = 0;
    }
};

// beautiful!
class StatsManger
{
   public:
    StatsManger() : worker_stats_map() {}

    // Every stats_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    // ) staker points
    void Start(redisContext* rc);
    void LoadCurrentRound();
    void AddShare(std::string worker, double diff);

   private:
    std::unordered_map<std::string, WorkerStats> worker_stats_map;
};

#endif
// TODO: on crash load all the stats from redis (round hash)