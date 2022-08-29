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

#include "round_manager.hpp"
#include "blocks/block_submission.hpp"
#include "difficulty/difficulty_manager.hpp"
#include "logger.hpp"
#include "payments/payment_manager.hpp"
#include "redis/redis_manager.hpp"
#include "round.hpp"
#include "shares/share.hpp"
#include "static_config/static_config.hpp"
#include "stats/stats.hpp"

class RedisManager;
class RoundManager;
// update manager?
class StatsManager
{
   public:
    StatsManager(RedisManager* redis_manager, DifficultyManager* diff_manager, RoundManager* round_manager,
                 int hr_interval, int effort_interval, int avg_hr_interval,
                 int diff_adjust_seconds, int hashrate_ttl);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    void Start(std::stop_token st);
    bool LoadAvgHashrateSums();
    void AddShare(const std::string& worker_full, const std::string& miner_addr,
                  const double diff);
    bool AddWorker(const std::string& address, const std::string& worker_full,
                   std::string_view script_pub_key, std::time_t curtime,
                   const std::string& idTag = "null");
    void PopWorker(const std::string& worker, const std::string& address);

    // bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    bool UpdateIntervalStats(int64_t update_time_ms);
    bool UpdateEffortStats(int64_t update_time_ms);

    static int hashrate_interval_seconds;
    static int effort_interval_seconds;
    static int average_hashrate_interval_seconds;
    static int diff_adjust_seconds;
    static int hashrate_ttl_seconds;

   private:
    RedisManager* redis_manager;
    DifficultyManager* diff_manager;
    RoundManager* round_manager;

    std::mutex stats_map_mutex;
    // worker -> stats
    worker_map worker_stats_map;
};

#endif
// TODO: add another unordered map for stakers (non miners too!)