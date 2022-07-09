#ifndef STATS_MANAGER_HPP_
#define STATS_MANAGER_HPP_

#include <hiredis/hiredis.h>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "payments/payment_manager.hpp"
#include "logger.hpp"
#include "redis/redis_manager.hpp"
#include "stats/stats.hpp"
#include "blocks/block_submission.hpp"
#include "round.hpp"
#include "shares/share.hpp"
#include "static_config/config.hpp"

class RedisManager;
class StatsManager
{
   public:
    StatsManager(RedisManager* redis_manager, int hr_interval,
                 int effort_interval, int avg_hr_interval, int hashrate_ttl);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    // ) staker points
    void Start();
    bool LoadCurrentRound();
    void AddShare(const std::string& worker_full, const std::string& miner_addr,
                  const double diff);
    bool AddWorker(const std::string& address, const std::string& worker_full,
                   const std::string& idTag, std::time_t curtime);
    void PopWorker(const std::string& worker, const std::string& address);

    bool ClosePoWRound(std::string_view chain,
                       const BlockSubmission* submission, double fee);

    // bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    bool UpdateStats(bool update_effort, bool update_hr,
                     int64_t update_time_ms);
    Round GetChainRound(const std::string& chain);

    static int hashrate_interval_seconds;
    static int effort_interval_seconds;
    static int average_hashrate_interval_seconds;
    static int hashrate_ttl_seconds;

   private:
    RedisManager* redis_manager;

    std::mutex stats_map_mutex;
    // worker -> stats
    worker_map worker_stats_map;
    // miner -> stats
    miner_map miner_stats_map;
    // chain -> effort
    round_map round_stats_map;
};

#endif
// TODO: on crash load all the stats from redis (round hash)
// TODO: add another unordered map for stakers (non miners too!)