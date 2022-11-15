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
#include "redis_stats.hpp"
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
    StatsManager(const RedisManager& redis_manager, DifficultyManager* diff_manager,
                 RoundManager* round_manager, const StatsConfig* cc);

    // Every hashrate_interval_seconds we need to write:
    // ) worker hashrate
    // ) miner hashrate
    // ) pool hashrate
    template <StaticConf confs>
    void Start(std::stop_token st);

    bool LoadAvgHashrateSums(int64_t hr_time);
    void AddShare(const WorkerFullId& id, const double diff);
    bool AddWorker(WorkerFullId& worker_full_id, const std::string_view address,
                   std::string_view worker_full, std::time_t curtime,
                   std::string_view alias, int64_t min_payout);
    void PopWorker(const WorkerFullId& fullid);

    // bool AppendPoSBalances(std::string_view chain, int64_t from_ms);

    template <StaticConf confs>
    bool UpdateIntervalStats(int64_t update_time_ms);

    bool UpdateEffortStats(int64_t update_time_ms);

    static int hashrate_interval_seconds;
    static int effort_interval_seconds;
    static int average_hashrate_interval_seconds;
    static int diff_adjust_seconds;
    static double average_interval_ratio;

   private:
    static constexpr std::string_view field_str = "StatsManager";
    Logger<field_str> logger;
    const StatsConfig* conf;
    RedisStats redis_manager;
    DifficultyManager* diff_manager;
    RoundManager* round_manager;

    std::mutex stats_map_mutex;
    // worker -> stats
    worker_map worker_stats_map;
};

#endif