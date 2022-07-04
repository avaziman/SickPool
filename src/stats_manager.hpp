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

#include "stats.hpp"
#include "logger.hpp"
#include "stratum/block_submission.hpp"
#include "stratum/round.hpp"
#include "stratum/share.hpp"
#include "stratum/static_config/config.hpp"
#include "stratum/redis_manager.hpp"

class RedisManager;
class StatsManager
{
   public:
    StatsManager(RedisManager* redis_manager, int hr_interval, int effort_interval, int avg_hr_interval,
                 int hashrate_ttl);

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
                   std::string_view idTag, std::time_t curtime);
    void PopWorker(std::string_view worker, std::string_view address);

    bool ClosePoWRound(std::string_view chain,
                       const BlockSubmission* submission, double fee);

    // bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    bool UpdateStats(bool update_effort, bool update_hr,
                     int64_t update_time_ms);

    Round GetChainRound(std::string_view chain);

    static int hashrate_interval_seconds;
    static int effort_interval_seconds;
    static int average_hashrate_interval_seconds;
    static int hashrate_ttl_seconds;
   private:

    RedisManager* redis_manager;

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